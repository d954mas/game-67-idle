// setRegions op + region API/CLI parity tests (no Python needed). Run:
//   node --test ai_studio/assets/canvas/tests/regions.test.mjs
//
// setRegions is the ADJUST/SELECT step: it replaces an element's regions array,
// validates ids + in-bounds integer rects, preserves extra detector fields, and
// is journaled so undo restores the previous regions.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import { addImage, createProject, getProject, setRegions, undoOp, redoOp } from "../ops.mjs";
import { createCanvasApi } from "../api.mjs";
import { magentaSheetPng, solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-regions-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

test("setRegions round-trips regions and preserves extra detector fields", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Regions" });
  const { element } = addImage(ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() }); // 64x48

  const regions = [
    { id: "region_001", name: "  Hero  ", rect: [8, 8, 20, 20], content_bbox: [8, 8, 20, 20], area_px: 400 },
    { id: "region_002", name: "", rect: [36, 16, 20, 24], content_bbox: [36, 16, 20, 24], area_px: 480, merged_from: ["a", "b"] },
  ];
  const result = setRegions(ROOT, { projectId: project.id, elementId: element.id, regions });
  assert.equal(result.regions.length, 2);
  // rect + id normalized, extra fields preserved verbatim.
  assert.deepEqual(result.regions[0].rect, [8, 8, 20, 20]);
  assert.deepEqual(result.regions[1].content_bbox, [36, 16, 20, 24]);
  assert.deepEqual(result.regions[1].merged_from, ["a", "b"]);
  // Optional `name` is validated: trimmed when present, dropped when blank.
  assert.equal(result.regions[0].name, "Hero");
  assert.equal("name" in result.regions[1], false);

  // Persisted on the element and re-readable from disk.
  const stored = getProject(ROOT, project.id).elements.find((el) => el.id === element.id);
  assert.deepEqual(stored.regions, result.regions);
});

test("setRegions is journaled: undo restores the previous regions, redo re-applies", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Undo regions" });
  const { element } = addImage(ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });

  // Baseline set, then a second set that replaces it.
  setRegions(ROOT, { projectId: project.id, elementId: element.id, regions: [{ id: "r1", rect: [0, 0, 10, 10] }] });
  setRegions(ROOT, { projectId: project.id, elementId: element.id, regions: [{ id: "r2", rect: [5, 5, 20, 20] }] });

  // Undo the second set -> the first regions come back.
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  const afterUndo = undone.elements.find((el) => el.id === element.id).regions;
  assert.deepEqual(afterUndo.map((r) => r.id), ["r1"]);
  assert.deepEqual(afterUndo[0].rect, [0, 0, 10, 10]);

  // Redo re-applies the second set.
  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.deepEqual(redone.elements.find((el) => el.id === element.id).regions.map((r) => r.id), ["r2"]);

  // Undo twice from here -> back to the pre-region state (no regions field).
  undoOp(ROOT, { projectId: project.id });
  const base = undoOp(ROOT, { projectId: project.id }).project;
  const baseRegions = base.elements.find((el) => el.id === element.id).regions;
  assert.ok(!baseRegions || baseRegions.length === 0, "regions cleared back to the base state");
});

test("setRegions renames a region (name-only change) while preserving its rect", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Rename region" });
  const { element } = addImage(ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });

  setRegions(ROOT, { projectId: project.id, elementId: element.id, regions: [{ id: "r1", rect: [4, 4, 10, 10] }] });
  // The page's inline rename replays the region with the SAME rect + a name.
  const renamed = setRegions(ROOT, {
    projectId: project.id,
    elementId: element.id,
    regions: [{ id: "r1", rect: [4, 4, 10, 10], name: "Hero" }],
  });
  assert.deepEqual(renamed.regions[0].rect, [4, 4, 10, 10], "rect unchanged by the rename");
  assert.equal(renamed.regions[0].name, "Hero");

  // Rename is journaled: undo restores the pre-name region (same rect, no name).
  const undone = undoOp(ROOT, { projectId: project.id }).project.elements.find((el) => el.id === element.id);
  assert.deepEqual(undone.regions[0].rect, [4, 4, 10, 10]);
  assert.equal("name" in undone.regions[0], false);
});

test("the '+ Add region' default rect (25% centered) validates in bounds", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Add region" });
  const { element } = addImage(ROOT, project.id, { name: "s.png", bytes: solidPng(40, 30, [10, 20, 30]) });
  // Same computation as actions.addCenteredRegion for a 40x30 source.
  const sw = 40;
  const sh = 30;
  const w = Math.min(sw, Math.max(4, Math.round(sw * 0.25)));
  const h = Math.min(sh, Math.max(4, Math.round(sh * 0.25)));
  const x = Math.round((sw - w) / 2);
  const y = Math.round((sh - h) / 2);
  const result = setRegions(ROOT, { projectId: project.id, elementId: element.id, regions: [{ id: "c1", rect: [x, y, w, h] }] });
  assert.deepEqual(result.regions[0].rect, [15, 11, 10, 8], "centered ~25% default rect, in bounds");
});

test("setRegions rejects out-of-bounds, malformed, missing-id, and duplicate rects", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Reject" });
  const { element } = addImage(ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() }); // 64x48
  const call = (regions) => () => setRegions(ROOT, { projectId: project.id, elementId: element.id, regions });

  assert.throws(call([{ id: "r1", rect: [50, 0, 30, 10] }]), /out of source bounds/); // x+w=80 > 64
  assert.throws(call([{ id: "r1", rect: [0, 40, 10, 20] }]), /out of source bounds/); // y+h=60 > 48
  assert.throws(call([{ id: "r1", rect: [-1, 0, 10, 10] }]), /out of source bounds/);
  assert.throws(call([{ id: "r1", rect: [0, 0, 10] }]), /rect must be \[x, y, w, h\]/);
  assert.throws(call([{ id: "r1", rect: [0, 0, 0, 10] }]), /positive width and height/);
  assert.throws(call([{ rect: [0, 0, 10, 10] }]), /missing an id/);
  assert.throws(call([{ id: "dup", rect: [0, 0, 5, 5] }, { id: "dup", rect: [6, 6, 5, 5] }]), /duplicate region id/);
  assert.throws(call("nope"), /requires a regions array/);
});

// ---- API + CLI parity --------------------------------------------------------

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
      this._resolve({ status: this.statusCode, json: () => JSON.parse(buffer.toString("utf8")) });
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

test("canvas API PUT elements/<eid>/regions replaces the region array", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Region API" })).json().project.id;
  const elementId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "sheet.png",
    bytes_base64: magentaSheetPng().toString("base64"),
  })).json().element.id;

  const put = await invokeApi(handler, "PUT", `/api/canvas/projects/${projectId}/elements/${elementId}/regions`, {
    regions: [{ id: "r1", rect: [4, 4, 10, 10] }],
  });
  assert.equal(put.status, 200);
  assert.deepEqual(put.json().regions.map((r) => r.id), ["r1"]);

  const got = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.deepEqual(got.elements.find((el) => el.id === elementId).regions[0].rect, [4, 4, 10, 10]);

  // Out-of-bounds is rejected with a 400 + clear error.
  const bad = await invokeApi(handler, "PUT", `/api/canvas/projects/${projectId}/elements/${elementId}/regions`, {
    regions: [{ id: "r1", rect: [0, 0, 999, 10] }],
  });
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /out of source bounds/);
});

test("cli regions-set/regions-show round-trip through a JSON file", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-regions-"));
  const env = { ...process.env, CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const runCli = (...args) =>
    JSON.parse(execFileSync(process.execPath, [CLI, ...args], { env, encoding: "utf8" }).trim().split("\n").filter(Boolean).at(-1));

  const pngPath = join(dir, "sheet.png");
  writeFileSync(pngPath, solidPng(40, 30, [10, 20, 30]));
  const projectId = runCli("create", "--title", "CLI Regions").project.id;
  const elementId = runCli("add-image", projectId, "--file", pngPath).element.id;

  const jsonPath = join(dir, "regions.json");
  writeFileSync(jsonPath, JSON.stringify([{ id: "r1", rect: [2, 2, 8, 8] }, { id: "r2", rect: [20, 10, 12, 12] }]));
  const set = runCli("regions-set", projectId, "--element", elementId, "--json", jsonPath);
  assert.equal(set.regions.length, 2);

  const shown = runCli("regions-show", projectId, "--element", elementId);
  assert.deepEqual(shown.regions.map((r) => r.id), ["r1", "r2"]);
});
