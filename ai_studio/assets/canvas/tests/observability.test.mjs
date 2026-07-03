// Observability tests: duration_ms on journal entries, per-project errors.jsonl on
// an induced failure (via the real HTTP adapter), the project-not-found no-op, and
// the ops-stats rollup (op + API route). No Python needed. Run:
//   node --test ai_studio/assets/canvas/tests/observability.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL } from "node:url";
import { createCanvasApi } from "../api.mjs";
import {
  addImage,
  createProject,
  opsStats,
  patchElement,
  readHistory,
  recordOpFailure,
  redoOp,
  undoOp,
} from "../ops.mjs";
import { solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-obs-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// In-process HTTP invoke harness (mirrors api.test.mjs): no port binding.
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
      this._resolve({ status: this.statusCode, headers: this.headers, json: () => JSON.parse(buffer.toString("utf8")) });
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

function readErrorsFile(dir, id) {
  const path = join(dir, id, "errors.jsonl");
  if (!existsSync(path)) return [];
  return readFileSync(path, "utf8").split("\n").filter(Boolean).map((line) => JSON.parse(line));
}

test("every journaled entry (mutations + markers) carries a numeric duration_ms", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Timing" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  patchElement(ROOT, project.id, element.id, { x: 5 });
  undoOp(ROOT, { projectId: project.id });
  redoOp(ROOT, { projectId: project.id });

  const history = readHistory(ROOT, { projectId: project.id });
  assert.deepEqual(history.entries.map((e) => e.op), ["addImage", "patchElement", "undo", "redo"]);
  for (const entry of history.entries) {
    assert.equal(typeof entry.duration_ms, "number", `${entry.op} entry has duration_ms`);
    assert.ok(entry.duration_ms >= 0);
  }
});

test("API mutating responses add duration_ms without dropping existing fields", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Resp" });
  assert.equal(created.status, 201);
  assert.equal(typeof created.json().duration_ms, "number");
  assert.ok(created.json().project.id, "project field preserved alongside duration_ms");
});

test("a project-resolvable API failure appends to errors.jsonl; project-not-found does not", async (t) => {
  const dir = tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Err" })).json().project.id;

  // Induce an op failure on an existing project: patch a missing element. T0254 Tier 1
  // #2: statusForError maps "not found" errors to 404, not the old catch-all 400.
  const failed = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/nope`, { x: 1 });
  assert.equal(failed.status, 404);
  assert.match(failed.json().error, /element not found/);

  const errors = readErrorsFile(dir, projectId);
  assert.equal(errors.length, 1, "one error row written");
  assert.match(errors[0].error, /element not found/);
  assert.equal(typeof errors[0].duration_ms, "number");
  assert.ok(errors[0].at && errors[0].op, "error row carries at + op");

  // A failure on an unresolvable project can't be logged (no folder to write to).
  const missing = await invokeApi(handler, "PATCH", "/api/canvas/projects/ghost-xyz/elements/e1", { x: 1 });
  assert.equal(missing.status, 404);
  assert.equal(existsSync(join(dir, "ghost-xyz")), false, "no folder created for a missing project");
});

test("ops-stats rolls up per-op count/median/p95 + error count (op + API route)", async (t) => {
  const dir = tempProjects(t);
  const project = createProject(ROOT, { title: "Stats" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  patchElement(ROOT, project.id, element.id, { x: 1 });
  patchElement(ROOT, project.id, element.id, { x: 2 });
  patchElement(ROOT, project.id, element.id, { x: 3 });
  // Seed an error row the same way the clients do.
  recordOpFailure(ROOT, project.id, { op: "patchElement", error: new Error("boom"), duration_ms: 1.5 });

  const stats = opsStats(ROOT, { projectId: project.id });
  const byOp = Object.fromEntries(stats.ops.map((o) => [o.op, o]));
  assert.equal(byOp.addImage.count, 1);
  assert.equal(byOp.patchElement.count, 3);
  assert.equal(typeof byOp.patchElement.median_ms, "number");
  assert.equal(typeof byOp.patchElement.p95_ms, "number");
  assert.equal(stats.errors.count, 1);

  // Parity: the GET /ops-stats route returns the same shape.
  const handler = createCanvasApi(ROOT);
  const viaApi = await invokeApi(handler, "GET", `/api/canvas/projects/${project.id}/ops-stats`);
  assert.equal(viaApi.status, 200);
  assert.equal(viaApi.json().errors.count, 1);
  assert.ok(Array.isArray(viaApi.json().ops));
});
