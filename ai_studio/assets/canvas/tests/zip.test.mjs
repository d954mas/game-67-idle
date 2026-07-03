// Canvas export zip tests (T0229): the minimal STORE-mode zip writer, the ops.zipExport
// bundler over a finished export run, the HTTP export-zip route, and CLI --zip parity.
// Run: node --test ai_studio/assets/canvas/tests/zip.test.mjs
//
// Verification unzips the archive with a tiny inline reader (STORE = no compression, no
// data descriptor, so sizes live in the local headers) and checks the central-directory
// entry count, each entry's CRC-32, and that the stored bytes equal the originals — an
// independent check that the archive is a real, correct zip.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import { addImage, createProject, exportElements, zipExport } from "../ops.mjs";
import { createCanvasApi } from "../api.mjs";
import { crc32, zipStore } from "../zip.mjs";
import { magentaSheetPng, solidPng } from "./png_fixture.mjs";

const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-zip-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// Minimal STORE-zip reader: walk local file headers from the front, then read the EOCD
// entry count. Returns { total, entries:[{name, method, crc, data}] }.
function readZip(buffer) {
  assert.ok(buffer.length >= 22, "buffer holds at least an EOCD");
  const eocd = buffer.length - 22;
  assert.equal(buffer.readUInt32LE(eocd), 0x06054b50, "EOCD signature present at the tail");
  const total = buffer.readUInt16LE(eocd + 10);
  const centralSize = buffer.readUInt32LE(eocd + 12);
  const centralOffset = buffer.readUInt32LE(eocd + 16);
  assert.equal(centralOffset + centralSize, eocd, "central directory sits right before the EOCD");

  const entries = [];
  let pos = 0;
  while (pos + 4 <= buffer.length && buffer.readUInt32LE(pos) === 0x04034b50) {
    const method = buffer.readUInt16LE(pos + 8);
    const crc = buffer.readUInt32LE(pos + 14);
    const size = buffer.readUInt32LE(pos + 18);
    const nameLen = buffer.readUInt16LE(pos + 26);
    const extraLen = buffer.readUInt16LE(pos + 28);
    const name = buffer.toString("utf8", pos + 30, pos + 30 + nameLen);
    const dataStart = pos + 30 + nameLen + extraLen;
    const data = buffer.subarray(dataStart, dataStart + size);
    entries.push({ name, method, crc, data });
    pos = dataStart + size;
  }
  return { total, entries };
}

// ---- the pure writer ---------------------------------------------------------

test("zipStore writes a valid STORE archive with correct CRCs and verbatim bytes", () => {
  const a = Buffer.from("hello canvas", "utf8");
  const b = magentaSheetPng();
  const archive = zipStore([{ name: "note.txt", data: a }, { name: "pic.png", data: b }]);

  const { total, entries } = readZip(archive);
  assert.equal(total, 2, "EOCD reports two entries");
  assert.equal(entries.length, 2, "two local file headers parsed");
  assert.deepEqual(entries.map((e) => e.name), ["note.txt", "pic.png"]);
  for (const entry of entries) assert.equal(entry.method, 0, "STORE method (no compression)");
  assert.ok(entries[0].data.equals(a), "first entry bytes are verbatim");
  assert.ok(entries[1].data.equals(b), "second entry bytes are verbatim");
  assert.equal(entries[0].crc, crc32(a), "CRC-32 matches the note bytes");
  assert.equal(entries[1].crc, crc32(b), "CRC-32 matches the png bytes");
});

// ---- ops.zipExport over a real export run ------------------------------------

test("zipExport bundles a finished run's image files (unzip-verify)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Zip" });
  const png = magentaSheetPng();
  const a = addImage(REPO_ROOT, project.id, { name: "hero.png", bytes: png }).element;
  const b = addImage(REPO_ROOT, project.id, { name: "villain.png", bytes: png }).element;

  // Two default 1x-png rows -> two pure Node copies, so no Python is required.
  const run = await exportElements(REPO_ROOT, { projectId: project.id, elementIds: [a.id, b.id] });
  const { bytes, files } = zipExport(REPO_ROOT, { projectId: project.id, stamp: run.stamp });
  assert.deepEqual(files.sort(), ["hero_png.png", "villain_png.png"]);

  const { total, entries } = readZip(bytes);
  assert.equal(total, 2);
  const byName = Object.fromEntries(entries.map((e) => [e.name, e]));
  assert.ok(byName["hero_png.png"].data.equals(png), "hero bytes round-trip through the zip");
  assert.ok(byName["villain_png.png"].data.equals(png), "villain bytes round-trip through the zip");
  // The manifest itself is NOT bundled (only the produced images).
  assert.equal(entries.some((e) => e.name === "manifest.json"), false);
});

test("zipExport is loud on an unknown stamp", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Zip missing" });
  assert.throws(() => zipExport(REPO_ROOT, { projectId: project.id, stamp: "nope" }), /export run not found/);
});

// ---- HTTP export-zip route ---------------------------------------------------

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

test("GET export-zip returns an application/zip bundle of the run's images", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Zip API" })).json().project.id;
  const png = magentaSheetPng();
  const eid = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "hero.png",
    bytes_base64: png.toString("base64"),
  })).json().element.id;
  const eid2 = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "villain.png",
    bytes_base64: png.toString("base64"),
  })).json().element.id;

  const exported = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/export`, { elementIds: [eid, eid2] });
  const stamp = exported.json().stamp;

  const zipRes = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/export-zip/${stamp}`);
  assert.equal(zipRes.status, 200);
  assert.equal(zipRes.headers["content-type"], "application/zip");
  const { total, entries } = readZip(zipRes.buffer);
  assert.equal(total, 2);
  assert.deepEqual(entries.map((e) => e.name).sort(), ["hero_png.png", "villain_png.png"]);

  // A bad stamp is a loud 404 ("export run not found"), not an empty archive (T0254
  // Tier 1 #2: statusForError maps "not found" to 404, not the old catch-all 400).
  const bad = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/export-zip/nope`);
  assert.equal(bad.status, 404);
});

// ---- CLI --zip parity --------------------------------------------------------

test("cli export --zip writes a STORE archive of the exported files", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-zip-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(9, 6, [12, 34, 56]));
  const zipPath = join(dir, "bundle.zip");

  const run = (...args) => {
    const stdout = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(stdout.trim().split("\n").filter(Boolean).at(-1));
  };

  const projectId = run("create", "--title", "CLI Zip").project.id;
  run("add-image", projectId, "--file", pngPath);
  const exported = run("export", projectId, "--all", "--zip", zipPath);
  assert.equal(exported.zip, resolve(zipPath));
  assert.ok(existsSync(zipPath), "the .zip was written to --zip path");

  const { entries } = readZip(readFileSync(zipPath));
  assert.deepEqual(entries.map((e) => e.name), ["pic_png.png"]);
});
