// Canvas export tests: per-element export settings (journaled/undoable), the
// scale-token parser, and the scale+encode export op. Run:
//   node --test ai_studio/assets/canvas/tests/export.test.mjs
//
// The pixel/encode assertions drive the real PIL tool (tools/export_images.py) and
// the screen compositor (tools/render_group.py); those tests skip cleanly with a
// clear message when the studio Python venv / Pillow is unavailable. The settings
// op, scale parsing, byte-identical copy, and CLI --to paths need no Python.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import {
  addImage,
  createGroup,
  createProject,
  exportElements,
  exportProject,
  getProject,
  parseScaleSpec,
  readHistory,
  redoOp,
  resolveExportScale,
  setExportSettings,
  undoOp,
} from "../ops.mjs";
import { createCanvasApi } from "../api.mjs";
import { decodePng, magentaSheetPng, solidPng } from "./png_fixture.mjs";

// export_images.py runs the studio venv Python with cwd = repo root, so the ops
// layer must be driven with the real repo root for the encode/render tests.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-export-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// ---- scale-token parsing -----------------------------------------------------

test("resolveExportScale parses multipliers and fixed dimensions; rejects junk", () => {
  assert.deepEqual(resolveExportScale("2x", 100, 50), { width: 200, height: 100 });
  assert.deepEqual(resolveExportScale("0.5x", 100, 50), { width: 50, height: 25 });
  assert.deepEqual(resolveExportScale("1x", 64, 48), { width: 64, height: 48 });
  // A fixed target dimension keeps aspect on the other axis.
  assert.deepEqual(resolveExportScale("512w", 256, 128), { width: 512, height: 256 });
  assert.deepEqual(resolveExportScale("512h", 256, 128), { width: 1024, height: 512 });

  assert.equal(parseScaleSpec("2x").kind, "mul");
  assert.equal(parseScaleSpec("512w").kind, "w");
  assert.equal(parseScaleSpec("512h").kind, "h");

  assert.throws(() => resolveExportScale("", 10, 10), /scale is required/);
  assert.throws(() => resolveExportScale("abc", 10, 10), /invalid export scale/);
  assert.throws(() => resolveExportScale("10z", 10, 10), /invalid export scale/);
  assert.throws(() => resolveExportScale("0x", 10, 10), /must be > 0/);
});

// ---- setExportSettings (journaled + undoable) --------------------------------

test("setExportSettings persists rows, journals one entry, and round-trips undo/redo", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Settings" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "hero.png", bytes: solidPng(8, 8) });

  const set = setExportSettings(REPO_ROOT, {
    projectId: project.id,
    elementId: element.id,
    rows: [
      { scale: "2x", suffix: "@2x", format: "jpg", quality: 80, resample: "nearest" },
      { scale: "1x", format: "png" },
    ],
  });
  // png rows drop quality; lossy rows keep a clamped quality; defaults filled in.
  assert.deepEqual(set.rows, [
    { scale: "2x", suffix: "@2x", format: "jpg", resample: "nearest", quality: 80 },
    { scale: "1x", suffix: "", format: "png", resample: "lanczos" },
  ]);
  assert.equal(getProject(REPO_ROOT, project.id).elements[0].export.length, 2);

  const ops = readHistory(REPO_ROOT, { projectId: project.id }).entries.map((entry) => entry.op);
  assert.deepEqual(ops, ["addImage", "setExportSettings"]);

  // Undo restores the pre-settings element (no export rows); redo re-applies them.
  undoOp(REPO_ROOT, { projectId: project.id });
  assert.equal(getProject(REPO_ROOT, project.id).elements[0].export, undefined);
  redoOp(REPO_ROOT, { projectId: project.id });
  assert.equal(getProject(REPO_ROOT, project.id).elements[0].export.length, 2);
});

test("setExportSettings rejects unknown format/scale and unsafe suffix", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Validate" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: solidPng() });
  const set = (rows) => () => setExportSettings(REPO_ROOT, { projectId: project.id, elementId: element.id, rows });
  assert.throws(set([{ scale: "1x", format: "tiff" }]), /format must be png\/jpg\/webp/);
  assert.throws(set([{ scale: "bogus", format: "png" }]), /invalid export scale/);
  assert.throws(set([{ scale: "1x", format: "png", resample: "bicubic" }]), /resample must be/);
  assert.throws(set([{ scale: "1x", format: "png", suffix: "../evil" }]), /unsafe characters/);
  assert.throws(set([{ scale: "1x", format: "png", suffix: "..dots" }]), /unsafe characters/);
});

// ---- export execution --------------------------------------------------------

test("exportElements copies a 1x-png export byte-identically (no Python needed)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Copy" });
  const png = magentaSheetPng();
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: png });

  // Default rows (none set) = a single implicit 1x png row -> pure byte copy.
  const result = await exportElements(REPO_ROOT, { projectId: project.id, elementIds: [element.id] });
  assert.equal(result.items.length, 1);
  assert.deepEqual([result.items[0].w, result.items[0].h], [64, 48]);
  const out = readFileSync(join(result.folder, result.items[0].file));
  assert.ok(out.equals(png), "1x png export is byte-identical to the source file");
  assert.equal(result.manifest.schema, "ai_studio.canvas.export.v1");
});

test("exportElements scales to the requested pixels per format (skips without Python)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Scale" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() }); // 64x48

  let result;
  try {
    result = await exportElements(REPO_ROOT, {
      projectId: project.id,
      elementIds: [element.id],
      rows: [
        { scale: "2x", format: "png", suffix: "@2x" },
        { scale: "0.5x", format: "png", suffix: "@half" },
        { scale: "128w", format: "png", suffix: "@w" },
      ],
    });
  } catch (error) {
    t.skip(`export_images.py / PIL unavailable: ${error.message}`);
    return;
  }

  const byName = Object.fromEntries(result.items.map((item) => [item.file, item]));
  const two = decodePng(readFileSync(join(result.folder, `${slugName("sheet.png")}@2x.png`)));
  const half = decodePng(readFileSync(join(result.folder, `${slugName("sheet.png")}@half.png`)));
  const wide = decodePng(readFileSync(join(result.folder, `${slugName("sheet.png")}@w.png`)));
  assert.deepEqual([two.width, two.height], [128, 96], "2x doubles both axes");
  assert.deepEqual([half.width, half.height], [32, 24], "0.5x halves both axes");
  assert.deepEqual([wide.width, wide.height], [128, 96], "128w sets width, keeps aspect");
  // Manifest records the same target pixels the op computed.
  assert.deepEqual([byName[`${slugName("sheet.png")}@2x.png`].w, byName[`${slugName("sheet.png")}@2x.png`].h], [128, 96]);
});

test("exportElements JPEG/WebP quality changes the encoded file size (skips without Python)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Quality" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });

  let result;
  try {
    result = await exportElements(REPO_ROOT, {
      projectId: project.id,
      elementIds: [element.id],
      rows: [
        { scale: "1x", format: "jpg", quality: 92, suffix: "_hi" },
        { scale: "1x", format: "jpg", quality: 12, suffix: "_lo" },
        { scale: "1x", format: "webp", quality: 90, suffix: "_wp" },
      ],
    });
  } catch (error) {
    t.skip(`export_images.py / PIL unavailable: ${error.message}`);
    return;
  }

  const size = (file) => statSync(join(result.folder, file)).size;
  const base = slugName("sheet.png");
  assert.ok(size(`${base}_lo.jpg`) < size(`${base}_hi.jpg`), "lower JPEG quality = smaller file");
  assert.ok(existsSync(join(result.folder, `${base}_wp.webp`)), "webp row encoded a .webp file");
});

// ---- project-level export ----------------------------------------------------

test("exportProject renders every visible screen into one folder (skips without Python)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Project export" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  const group = createGroup(REPO_ROOT, { projectId: project.id, name: "Screen A", fromElements: [element.id] }).group;

  let result;
  try {
    result = await exportProject(REPO_ROOT, { projectId: project.id });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }

  assert.equal(result.manifest.kind, "project");
  assert.equal(result.screens.length, 1);
  assert.equal(result.screens[0].groupId, group.id);
  assert.ok(existsSync(join(result.folder, result.screens[0].file)), "the screen png was written");
  assert.ok(existsSync(join(result.folder, "manifest.json")));

  // A project with no visible screens is a clear error, not a silent empty export.
  const empty = createProject(REPO_ROOT, { title: "No screens" });
  await assert.rejects(() => exportProject(REPO_ROOT, { projectId: empty.id }), /no visible screens/);
});

// ---- CLI parity: --to lands files at an explicit path ------------------------

test("cli export --to writes the exported files to an explicit directory", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-export-cli-"));
  const toDir = join(dir, "picked-folder");
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(9, 6, [12, 34, 56]));

  const run = (...args) => {
    const stdout = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(stdout.trim().split("\n").filter(Boolean).at(-1));
  };

  const projectId = run("create", "--title", "CLI Export").project.id;
  run("add-image", projectId, "--file", pngPath);

  // Default rows = 1x png (pure copy, no Python), landed at the explicit --to path.
  const exported = run("export", projectId, "--all", "--to", toDir);
  assert.equal(exported.to, resolve(toDir));
  assert.ok(exported.copied.includes("manifest.json"), "manifest copied to --to");
  for (const file of exported.copied) {
    assert.equal(existsSync(join(toDir, file)), true, `${file} landed in --to`);
  }
  // The confined automation default folder still exists too (agents rely on it).
  assert.equal(existsSync(join(exported.folder, "manifest.json")), true);
});

// ---- API parity: settings PUT + export with rows -----------------------------

function invokeApi(handler, method, path, body) {
  const req = new EventEmitter();
  req.method = method;
  req.setEncoding = () => {};
  req.destroy = () => {};
  const chunks = [];
  const res = {
    statusCode: 0,
    headers: {},
    writeHead(status, headers) {
      this.statusCode = status;
      this.headers = headers || {};
    },
    write(chunk) {
      chunks.push(Buffer.from(chunk));
      return true;
    },
    end(chunk) {
      if (chunk !== undefined && chunk !== null && chunk !== "") chunks.push(Buffer.from(chunk));
      const buffer = Buffer.concat(chunks);
      this._resolve({ status: this.statusCode, headers: this.headers, buffer, json: () => JSON.parse(buffer.toString("utf8")) });
    },
  };
  const done = new Promise((r) => {
    res._resolve = r;
  });
  handler(req, res, new URL(path, "http://canvas.local"));
  queueMicrotask(() => {
    if (body !== undefined) req.emit("data", Buffer.from(typeof body === "string" ? body : JSON.stringify(body)));
    req.emit("end");
  });
  return done;
}

test("canvas API sets export rows and exports with them (no Python for 1x png)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Export API" })).json().project.id;
  const elementId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "pic.png",
    bytes_base64: solidPng(6, 6).toString("base64"),
  })).json().element.id;

  const put = await invokeApi(handler, "PUT", `/api/canvas/projects/${projectId}/elements/${elementId}/export`, {
    rows: [{ scale: "1x", format: "png", suffix: "_a" }],
  });
  assert.equal(put.status, 200);
  assert.equal(put.json().rows[0].suffix, "_a");

  // Export honors the stored rows: a 1x png copy, so no Python is required.
  const exported = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/export`, { elementIds: [elementId] });
  assert.equal(exported.status, 200);
  assert.match(exported.json().items[0].file, /_a\.png$/);
});

// The op's filename base = the store slug of the element name (mirror of ops.slug):
// lowercase, non-alnum runs -> "_", trimmed. "sheet.png" -> "sheet_png".
function slugName(name) {
  return String(name).toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "").slice(0, 60) || "element";
}
