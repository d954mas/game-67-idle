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
  patchElement,
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
      { scale: "2x", format: "jpg", quality: 80, resample: "nearest" },
      { scale: "1x", format: "png" },
    ],
  });
  // png rows drop quality; lossy rows keep a clamped quality; defaults filled in. No
  // suffix field (T0229 removed it — file names are automatic).
  assert.deepEqual(set.rows, [
    { scale: "2x", format: "jpg", resample: "nearest", quality: 80 },
    { scale: "1x", format: "png", resample: "lanczos" },
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

test("setExportSettings rejects unknown format/scale and a removed suffix field", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Validate" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: solidPng() });
  const set = (rows) => () => setExportSettings(REPO_ROOT, { projectId: project.id, elementId: element.id, rows });
  assert.throws(set([{ scale: "1x", format: "tiff" }]), /format must be png\/jpg\/webp/);
  assert.throws(set([{ scale: "bogus", format: "png" }]), /invalid export scale/);
  assert.throws(set([{ scale: "1x", format: "png", resample: "bicubic" }]), /resample must be/);
  // T0229: suffix is gone — a NEW write carrying it is rejected LOUDLY (a stale client),
  // whatever the value (even an empty string), so bad rows never persist.
  assert.throws(set([{ scale: "1x", format: "png", suffix: "@2x" }]), /removed "suffix" field/);
  assert.throws(set([{ scale: "1x", format: "png", suffix: "" }]), /removed "suffix" field/);
});

// ---- export base (T0235: "source" vs "canvas") -------------------------------

test("setExportSettings whitelists + validates row.base and keeps stored JSON minimal", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Base rows" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "hero.png", bytes: solidPng(8, 8) });

  // "source" is kept; an explicit "canvas" and an absent base are both normalized away
  // (canvas is the DEFAULT base since the lead's flip — the allowlist would otherwise
  // silently DROP the field either way; here it must be whitelisted through, then
  // minimized) so old/default rows stay minimal in storage.
  const set = setExportSettings(REPO_ROOT, {
    projectId: project.id,
    elementId: element.id,
    rows: [
      { scale: "2x", format: "png", base: "source" },
      { scale: "1x", format: "png", base: "canvas" },
      { scale: "0.5x", format: "png" },
    ],
  });
  assert.deepEqual(set.rows, [
    { scale: "2x", format: "png", resample: "lanczos", base: "source" },
    { scale: "1x", format: "png", resample: "lanczos" },
    { scale: "0.5x", format: "png", resample: "lanczos" },
  ]);
  // What actually persisted to project.json carries the same minimal shape (no
  // base:"canvas" ever written to disk).
  const stored = getProject(REPO_ROOT, project.id).elements[0].export;
  assert.equal(stored[0].base, "source");
  assert.equal(stored[1].base, undefined);
  assert.equal(stored[2].base, undefined);
  assert.equal("base" in stored[1], false);

  // The whitelist rejects anything but source/canvas, loudly — never a silent drop.
  const setRows = (rows) => () => setExportSettings(REPO_ROOT, { projectId: project.id, elementId: element.id, rows });
  assert.throws(setRows([{ scale: "1x", format: "png", base: "bogus" }]), /base must be source\/canvas/);
  assert.throws(setRows([{ scale: "1x", format: "png", base: 5 }]), /base must be source\/canvas/);
  // An empty string is treated like an absent field (no value to validate) — not thrown.
  const empty = setExportSettings(REPO_ROOT, {
    projectId: project.id,
    elementId: element.id,
    rows: [{ scale: "1x", format: "png", base: "" }],
  });
  assert.equal(empty.rows[0].base, undefined);
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
        { scale: "2x", format: "png" },
        { scale: "0.5x", format: "png" },
        { scale: "128w", format: "png" },
      ],
    });
  } catch (error) {
    t.skip(`export_images.py / PIL unavailable: ${error.message}`);
    return;
  }

  // Several rows on one element -> automatic Figma scale markers (T0229): @2x, @0.5x, @128w.
  const base = slugName("sheet.png");
  const byName = Object.fromEntries(result.items.map((item) => [item.file, item]));
  const two = decodePng(readFileSync(join(result.folder, `${base}@2x.png`)));
  const half = decodePng(readFileSync(join(result.folder, `${base}@0.5x.png`)));
  const wide = decodePng(readFileSync(join(result.folder, `${base}@128w.png`)));
  assert.deepEqual([two.width, two.height], [128, 96], "2x doubles both axes");
  assert.deepEqual([half.width, half.height], [32, 24], "0.5x halves both axes");
  assert.deepEqual([wide.width, wide.height], [128, 96], "128w sets width, keeps aspect");
  // Manifest records the same target pixels the op computed.
  assert.deepEqual([byName[`${base}@2x.png`].w, byName[`${base}@2x.png`].h], [128, 96]);
});

test("exportElements JPEG/WebP quality changes the encoded file size (skips without Python)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Quality" });
  // Without a suffix column (T0229), distinct file names come from distinct element names;
  // each element carries one row so it exports to a clean per-element name.
  const hi = addImage(REPO_ROOT, project.id, { name: "hi.png", bytes: magentaSheetPng() }).element;
  const lo = addImage(REPO_ROOT, project.id, { name: "lo.png", bytes: magentaSheetPng() }).element;
  const wp = addImage(REPO_ROOT, project.id, { name: "wp.png", bytes: magentaSheetPng() }).element;
  setExportSettings(REPO_ROOT, { projectId: project.id, elementId: hi.id, rows: [{ scale: "1x", format: "jpg", quality: 92 }] });
  setExportSettings(REPO_ROOT, { projectId: project.id, elementId: lo.id, rows: [{ scale: "1x", format: "jpg", quality: 12 }] });
  setExportSettings(REPO_ROOT, { projectId: project.id, elementId: wp.id, rows: [{ scale: "1x", format: "webp", quality: 90 }] });

  let result;
  try {
    result = await exportElements(REPO_ROOT, { projectId: project.id, elementIds: [hi.id, lo.id, wp.id] });
  } catch (error) {
    t.skip(`export_images.py / PIL unavailable: ${error.message}`);
    return;
  }

  const size = (file) => statSync(join(result.folder, file)).size;
  assert.ok(size(`${slugName("lo.png")}.jpg`) < size(`${slugName("hi.png")}.jpg`), "lower JPEG quality = smaller file");
  assert.ok(existsSync(join(result.folder, `${slugName("wp.png")}.webp`)), "webp row encoded a .webp file");
});

// Automatic disambiguation (T0229): two elements sharing a name collide on the same base
// file; the second gets a deterministic numeric "_02". Pure copy, so no Python needed.
test("exportElements auto-disambiguates same-named elements with a numeric suffix", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Names" });
  const png = magentaSheetPng();
  const a = addImage(REPO_ROOT, project.id, { name: "hero.png", bytes: png }).element;
  const b = addImage(REPO_ROOT, project.id, { name: "hero.png", bytes: png }).element;
  const result = await exportElements(REPO_ROOT, { projectId: project.id, elementIds: [a.id, b.id] });
  assert.deepEqual(
    result.items.map((item) => item.file),
    ["hero_png.png", "hero_png_02.png"],
    "the second same-named element gets a deterministic _02",
  );
});

// ---- export base: "canvas" resolves against on-canvas size (T0235) -----------

// File naming only (no Python needed): a source-base row must never collide with the
// unmarked CANVAS 1x baseline (canvas is the default base), even at 1x — so it always
// carries the "-source" marker in a multi-row set. A single row stays the clean base
// name regardless of its base. The element is left at its default (unresized) size, so
// every row here resolves to the same 64x48 as the source file -> pure byte copies.
test('exportElements: a source-base row gets a "-source"-tagged marker in a multi-row set, even at 1x; a single row stays clean', async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Source marker" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() }); // 64x48, w/h == source_w/h
  const base = slugName("sheet.png");

  const multi = await exportElements(REPO_ROOT, {
    projectId: project.id,
    elementIds: [element.id],
    rows: [
      { scale: "1x", format: "png" }, // default base "canvas" -> unmarked baseline
      { scale: "1x", format: "png", base: "source" }, // must not collide with the baseline
    ],
  });
  assert.deepEqual(
    multi.items.map((item) => item.file),
    [`${base}.png`, `${base}@1x-source.png`],
  );

  const single = await exportElements(REPO_ROOT, {
    projectId: project.id,
    elementIds: [element.id],
    rows: [{ scale: "1x", format: "png", base: "source" }],
  });
  assert.deepEqual(single.items.map((item) => item.file), [`${base}.png`], "a single row is always the clean base name");
});

// Pixel proof: resize the element ON THE CANVAS (element.w/h) without touching the
// source file's actual pixels (source_w/h unchanged) — exactly what a drag-resize does.
// A canvas-base row must resolve against the NEW on-canvas size; a source-base row on
// the SAME element must still resolve against the original source pixels.
test("exportElements: canvas-base on a RESIZED element produces on-canvas pixels; a source row on the same element still produces source pixels", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Canvas resize" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() }); // 64x48 source
  patchElement(REPO_ROOT, project.id, element.id, { w: 32, h: 24 }); // resized on canvas; source_w/h untouched

  let result;
  try {
    result = await exportElements(REPO_ROOT, {
      projectId: project.id,
      elementIds: [element.id],
      rows: [
        { scale: "1x", format: "png" }, // default base "canvas"
        { scale: "1x", format: "png", base: "source" },
      ],
    });
  } catch (error) {
    t.skip(`export_images.py / PIL unavailable: ${error.message}`);
    return;
  }

  const byBase = Object.fromEntries(result.items.map((item) => [item.base, item]));
  assert.deepEqual([byBase.canvas.w, byBase.canvas.h], [32, 24], "canvas base resolves against the resized on-canvas size");
  assert.deepEqual([byBase.source.w, byBase.source.h], [64, 48], "source base still resolves against the original source pixels");

  const canvasPng = decodePng(readFileSync(join(result.folder, byBase.canvas.file)));
  const sourcePng = decodePng(readFileSync(join(result.folder, byBase.source.file)));
  assert.deepEqual([canvasPng.width, canvasPng.height], [32, 24], "the encoded canvas-base file is really 32x24 pixels");
  assert.deepEqual([sourcePng.width, sourcePng.height], [64, 48], "the encoded source-base file is really 64x48 pixels");
  // The source row's target dims match the source file's actual pixels -> a
  // byte-identical Node copy (no resize, no re-encode).
  assert.ok(
    readFileSync(join(result.folder, byBase.source.file)).equals(magentaSheetPng()),
    "source-base 1x export is byte-identical to the source file",
  );
});

// Loud, not silent: a canvas-base row on an element with no real on-canvas size (zero
// width/height) throws instead of silently exporting a 1x1 or NaN-sized image.
test("exportElements: a canvas-base row on a zero-size element throws loudly", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Zero dim" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  patchElement(REPO_ROOT, project.id, element.id, { w: 0, h: 0 });
  await assert.rejects(
    () =>
      exportElements(REPO_ROOT, {
        projectId: project.id,
        elementIds: [element.id],
        rows: [{ scale: "1x", format: "png", base: "canvas" }],
      }),
    /no on-canvas size/,
  );
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

// ---- CLI parity: --base source|canvas (T0235) ---------------------------------

test("cli export-set/export --base source|canvas: persists, exports, and rejects an unknown value loudly", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-export-base-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(9, 6, [12, 34, 56]));

  const run = (...args) => {
    const stdout = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(stdout.trim().split("\n").filter(Boolean).at(-1));
  };

  const projectId = run("create", "--title", "CLI Base").project.id;
  const elementId = run("add-image", projectId, "--file", pngPath).element.id;

  // export-set --base source persists the field (round-trips through the store);
  // --base canvas is the default and is normalized AWAY in storage.
  const set = run("export-set", projectId, "--element", elementId, "--scale", "1x", "--base", "source");
  assert.equal(set.rows[0].base, "source");
  const setDefault = run("export-set", projectId, "--element", elementId, "--scale", "1x", "--base", "canvas");
  assert.equal(setDefault.rows[0].base, undefined);

  // export --base source (ad-hoc override) exports with it too — tool parity with the
  // site's Base segmented control. The element was never resized, so canvas dims ==
  // source dims here (9x6) -> still a pure copy, no Python required.
  const exported = run("export", projectId, "--elements", elementId, "--scale", "1x", "--base", "source");
  assert.equal(exported.items[0].base, "source");
  assert.deepEqual([exported.items[0].w, exported.items[0].h], [9, 6]);

  // An unknown --base value is rejected loudly (exit 1, the ops validation message on
  // stderr), never silently coerced to a default.
  let failed;
  try {
    execFileSync(process.execPath, [CLI, "export-set", projectId, "--element", elementId, "--scale", "1x", "--base", "bogus"], {
      env: { ...process.env, ...env },
      encoding: "utf8",
    });
    assert.fail('expected --base "bogus" to fail');
  } catch (error) {
    failed = error;
  }
  assert.match(failed.stderr, /base must be source\/canvas/);
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
    rows: [{ scale: "1x", format: "png" }],
  });
  assert.equal(put.status, 200);
  assert.equal(put.json().rows[0].suffix, undefined); // T0229: no suffix field persisted
  assert.equal(put.json().rows[0].scale, "1x");

  // A stale client PUTting a suffix is rejected LOUDLY (400), not silently dropped.
  const rejected = await invokeApi(handler, "PUT", `/api/canvas/projects/${projectId}/elements/${elementId}/export`, {
    rows: [{ scale: "1x", format: "png", suffix: "_a" }],
  });
  assert.equal(rejected.status, 400);
  assert.match(rejected.json().error, /removed "suffix" field/);

  // Export honors the stored rows: a 1x png copy, so no Python is required. The single
  // row gives the clean automatic name (slug of "pic.png").
  const exported = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/export`, { elementIds: [elementId] });
  assert.equal(exported.status, 200);
  assert.equal(exported.json().items[0].file, "pic_png.png");
});

// The op's filename base = the store slug of the element name (mirror of ops.slug):
// lowercase, non-alnum runs -> "_", trimmed. "sheet.png" -> "sheet_png".
function slugName(name) {
  return String(name).toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "").slice(0, 60) || "element";
}
