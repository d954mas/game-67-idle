// T0271 — group subtree scale (the lead's override of T0232 Q2's shipped frame-only
// default): dragging a group's scale handles now scales its FULL content by default
// (children move/resize proportionally, text font sizes scale); Ctrl+drag keeps the
// original frame-only behavior. Pure target math (tree.scaleGroupMoves) plus the
// journaled op (scaleGroup) — same "compute the whole result purely, ONE commitMutation"
// law every other op here follows. Exercised over the pure tree math, the op directly,
// the HTTP adapter, and the CLI (tool parity). Run:
//   node --test ai_studio/assets/canvas/tests/group_scale.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import {
  addImage,
  addText,
  assignToGroup,
  createGroup,
  createProject,
  getProject,
  redoOp,
  scaleGroup,
  undoOp,
} from "../ops.mjs";
import { scaleGroupMoves } from "../tree.mjs";
import { scaledFontSize } from "../site/viewport.mjs";
import { createCanvasApi } from "../api.mjs";
import { solidPng } from "./png_fixture.mjs";

const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));
// addText reads a REAL fonts.json (readFontsManifest joins `root` with a repo-relative
// path) -- use the real repo root for every op call here (mirrors transform.test.mjs/
// animation.test.mjs); CANVAS_PROJECTS_ROOT (redirected by tempProjects below) is what
// actually decides where project.json lives, independent of this `root` argument
// (groups.test.mjs's own render tests prove the same combination is safe).
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-group-scale-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

function img(pid, name, x, y, w, h, rgb = [10, 20, 30]) {
  return addImage(REPO_ROOT, pid, { name, bytes: solidPng(w, h, rgb), x, y }).element;
}

function byId(project, id) {
  return (
    (project.elements || []).find((item) => item.id === id) ||
    (project.groups || []).find((item) => item.id === id)
  );
}

// G@(0,0,50,50) direct child A(10,10,10,10); NESTED group S@(20,20,20,20, parentId:G)
// with its own child B(25,25,5,5) -- the two-level closure scaleGroupMoves/scaleGroup
// must cascade through (mirrors fitGroup's own 2-level nesting fixture in groups.test.mjs).
function seedNestedScreen() {
  const project = createProject(REPO_ROOT, { title: "NestedScreen" });
  const pid = project.id;
  const A = img(pid, "A", 10, 10, 10, 10);
  const G = createGroup(REPO_ROOT, { projectId: pid, name: "G", x: 0, y: 0, w: 50, h: 50 }).group;
  assignToGroup(REPO_ROOT, { projectId: pid, elementIds: [A.id], groupId: G.id });
  const B = img(pid, "B", 25, 25, 5, 5);
  const S = createGroup(REPO_ROOT, { projectId: pid, name: "S", x: 20, y: 20, w: 20, h: 20, parentId: G.id }).group;
  assignToGroup(REPO_ROOT, { projectId: pid, elementIds: [B.id], groupId: S.id });
  return { pid, A, G, B, S };
}

// ---- scaleGroupMoves (pure) -----------------------------------------------------

test("scaleGroupMoves: doubling a group's frame maps its own frame, a NESTED group, and elements at every level", (t) => {
  tempProjects(t);
  const { pid, A, G, B, S } = seedNestedScreen();
  const project = getProject(REPO_ROOT, pid);

  const patches = scaleGroupMoves(project, G.id, { x: 0, y: 0, w: 100, h: 100 });
  assert.deepEqual(patches, [
    { kind: "group", id: G.id, x: 0, y: 0, w: 100, h: 100 },
    { kind: "group", id: S.id, x: 40, y: 40, w: 40, h: 40 },
    { kind: "element", id: A.id, x: 20, y: 20, w: 20, h: 20 },
    { kind: "element", id: B.id, x: 50, y: 50, w: 10, h: 10 },
  ]);
});

test("scaleGroupMoves: non-proportional scale (sx != sy) maps a direct child's box on each axis independently", (t) => {
  tempProjects(t);
  const project0 = createProject(REPO_ROOT, { title: "NonProportional" });
  const pid = project0.id;
  const C = img(pid, "C", 10, 20, 10, 10);
  const G3 = createGroup(REPO_ROOT, { projectId: pid, name: "G3", x: 0, y: 0, w: 50, h: 100 }).group;
  assignToGroup(REPO_ROOT, { projectId: pid, elementIds: [C.id], groupId: G3.id });
  const project = getProject(REPO_ROOT, pid);

  // sx = 150/50 = 3, sy = 50/100 = 0.5.
  const patches = scaleGroupMoves(project, G3.id, { x: 0, y: 0, w: 150, h: 50 });
  const cPatch = patches.find((p) => p.id === C.id);
  assert.deepEqual(cPatch, { kind: "element", id: C.id, x: 30, y: 10, w: 30, h: 5 });
});

test("scaleGroupMoves: a TEXT descendant's fontSize scales by the mapping's sy -- the SAME value scaledFontSize gives; its box is never stretched (no w/h in the patch)", (t) => {
  tempProjects(t);
  const project0 = createProject(REPO_ROOT, { title: "TextScale" });
  const pid = project0.id;
  const G2 = createGroup(REPO_ROOT, { projectId: pid, name: "G2", x: 0, y: 0, w: 100, h: 50 }).group;
  const T = addText(REPO_ROOT, pid, { x: 10, y: 10, content: "Hi", style: { fontSize: 24 }, groupId: G2.id }).element;
  const project = getProject(REPO_ROOT, pid);

  // sx = 100/100 = 1, sy = 75/50 = 1.5.
  const patches = scaleGroupMoves(project, G2.id, { x: 0, y: 0, w: 100, h: 75 });
  const tPatch = patches.find((p) => p.id === T.id);
  assert.deepEqual(tPatch, { kind: "element", id: T.id, x: 10, y: 15, fontSize: scaledFontSize(24, 1.5) });
  assert.equal(scaledFontSize(24, 1.5), 36);
  assert.equal(tPatch.w, undefined, "a text patch never carries w -- PIL re-measures from content+fontSize");
  assert.equal(tPatch.h, undefined, "a text patch never carries h either");
});

test("scaleGroupMoves: a same-frame call maps every patch back to its current box exactly (sx=sy=1)", (t) => {
  tempProjects(t);
  const { pid, A, G, B, S } = seedNestedScreen();
  const project = getProject(REPO_ROOT, pid);

  const patches = scaleGroupMoves(project, G.id, { x: G.x, y: G.y, w: G.w, h: G.h });
  assert.deepEqual(patches, [
    { kind: "group", id: G.id, x: 0, y: 0, w: 50, h: 50 },
    { kind: "group", id: S.id, x: 20, y: 20, w: 20, h: 20 },
    { kind: "element", id: A.id, x: 10, y: 10, w: 10, h: 10 },
    { kind: "element", id: B.id, x: 25, y: 25, w: 5, h: 5 },
  ]);
});

test("scaleGroupMoves rejects an unknown group, non-finite x/y, and non-positive w/h", (t) => {
  tempProjects(t);
  const { pid, G } = seedNestedScreen();
  const project = getProject(REPO_ROOT, pid);

  assert.throws(() => scaleGroupMoves(project, "nope", { x: 0, y: 0, w: 10, h: 10 }), /group not found/);
  assert.throws(() => scaleGroupMoves(project, G.id, { x: NaN, y: 0, w: 10, h: 10 }), /finite newFrame x\/y/);
  assert.throws(() => scaleGroupMoves(project, G.id, { y: 0, w: 10, h: 10 }), /finite newFrame x\/y/); // missing x
  assert.throws(() => scaleGroupMoves(project, G.id, { x: 0, y: 0, w: 0, h: 10 }), /positive newFrame w\/h/);
  assert.throws(() => scaleGroupMoves(project, G.id, { x: 0, y: 0, w: -5, h: 10 }), /positive newFrame w\/h/);
});

// ---- scaleGroup (op layer) -------------------------------------------------------

test("scaleGroup scales the group's frame + a NESTED group + every element in ONE journal entry; undo restores everything byte-exact, redo reapplies", (t) => {
  tempProjects(t);
  const { pid, A, G, B, S } = seedNestedScreen();
  const before = getProject(REPO_ROOT, pid);
  const seqBefore = before.history_seq;

  const result = scaleGroup(REPO_ROOT, { projectId: pid, groupId: G.id, x: 0, y: 0, w: 100, h: 100 });
  assert.deepEqual({ x: result.group.x, y: result.group.y, w: result.group.w, h: result.group.h }, { x: 0, y: 0, w: 100, h: 100 });

  const after = getProject(REPO_ROOT, pid);
  assert.equal(after.history_seq, seqBefore + 1, "exactly one entry for the whole subtree scale");
  assert.deepEqual({ x: byId(after, S.id).x, y: byId(after, S.id).y, w: byId(after, S.id).w, h: byId(after, S.id).h }, { x: 40, y: 40, w: 40, h: 40 });
  assert.deepEqual({ x: byId(after, A.id).x, y: byId(after, A.id).y, w: byId(after, A.id).w, h: byId(after, A.id).h }, { x: 20, y: 20, w: 20, h: 20 });
  assert.deepEqual({ x: byId(after, B.id).x, y: byId(after, B.id).y, w: byId(after, B.id).w, h: byId(after, B.id).h }, { x: 50, y: 50, w: 10, h: 10 });

  const undone = undoOp(REPO_ROOT, { projectId: pid }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.deepEqual(undone.elements, before.elements, "undo restores every element byte-exact");
  assert.deepEqual(undone.groups, before.groups, "undo restores every group (the frame AND the nested subgroup) byte-exact");

  const redone = redoOp(REPO_ROOT, { projectId: pid }).project;
  assert.deepEqual({ x: byId(redone, S.id).x, y: byId(redone, S.id).y }, { x: 40, y: 40 });
});

test("scaleGroup: a same-frame call is a no-op (no journal entry)", (t) => {
  tempProjects(t);
  const { pid, G } = seedNestedScreen();
  const seqBefore = getProject(REPO_ROOT, pid).history_seq;

  scaleGroup(REPO_ROOT, { projectId: pid, groupId: G.id, x: G.x, y: G.y, w: G.w, h: G.h });
  assert.equal(getProject(REPO_ROOT, pid).history_seq, seqBefore, "identical frame writes no entry");
});

test("scaleGroup rejects bad input atomically: unknown group, non-finite x/y, non-positive w/h -- no write on any rejected call", (t) => {
  tempProjects(t);
  const { pid, G } = seedNestedScreen();
  const seqBefore = getProject(REPO_ROOT, pid).history_seq;

  assert.throws(() => scaleGroup(REPO_ROOT, { projectId: pid, groupId: "nope", x: 0, y: 0, w: 10, h: 10 }), /group not found/);
  assert.throws(() => scaleGroup(REPO_ROOT, { projectId: pid, groupId: G.id, x: NaN, y: 0, w: 10, h: 10 }), /finite newFrame x\/y/);
  assert.throws(() => scaleGroup(REPO_ROOT, { projectId: pid, groupId: G.id, x: 0, y: 0, w: -1, h: 10 }), /positive newFrame w\/h/);
  assert.throws(() => scaleGroup(REPO_ROOT, {}), /requires projectId/);
  assert.throws(() => scaleGroup(REPO_ROOT, { projectId: pid }), /requires groupId/);

  assert.equal(getProject(REPO_ROOT, pid).history_seq, seqBefore, "no rejected call wrote anything");
});

// ---- HTTP adapter parity ---------------------------------------------------------

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
    if (body !== undefined) req.emit("data", Buffer.from(JSON.stringify(body)));
    req.emit("end");
  });
  return done;
}

test("HTTP route POST .../groups/<gid>/scale cascades a NESTED group + its elements in one entry; bad dims are a loud 400", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const { pid, A, G, B, S } = seedNestedScreen();
  const seq0 = getProject(REPO_ROOT, pid).history_seq;

  const scaled = await invokeApi(handler, "POST", `/api/canvas/projects/${pid}/groups/${G.id}/scale`, { x: 0, y: 0, w: 100, h: 100 });
  assert.equal(scaled.status, 200);
  assert.deepEqual(
    { x: scaled.json().group.x, y: scaled.json().group.y, w: scaled.json().group.w, h: scaled.json().group.h },
    { x: 0, y: 0, w: 100, h: 100 },
  );
  const after = getProject(REPO_ROOT, pid);
  assert.equal(after.history_seq, seq0 + 1, "one entry over HTTP too");
  assert.deepEqual({ x: byId(after, S.id).x, y: byId(after, S.id).y }, { x: 40, y: 40 }, "nested group cascaded over HTTP");
  assert.deepEqual({ x: byId(after, A.id).x, y: byId(after, A.id).y }, { x: 20, y: 20 });
  assert.deepEqual({ x: byId(after, B.id).x, y: byId(after, B.id).y }, { x: 50, y: 50 });

  const bad = await invokeApi(handler, "POST", `/api/canvas/projects/${pid}/groups/${G.id}/scale`, { x: 0, y: 0, w: 0, h: 10 });
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /positive newFrame w\/h/);

  const badGroup = await invokeApi(handler, "POST", `/api/canvas/projects/${pid}/groups/nope/scale`, { x: 0, y: 0, w: 10, h: 10 });
  assert.equal(badGroup.status, 404);
});

// ---- CLI parity -------------------------------------------------------------------

test("cli group-scale scales a group's full subtree (nested group + elements); missing dims / unknown group fail loudly", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-group-scale-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const run = (...args) => {
    const out = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(out.trim().split("\n").filter(Boolean).at(-1));
  };

  const a = join(dir, "a.png");
  const b = join(dir, "b.png");
  writeFileSync(a, solidPng(10, 10, [1, 1, 1]));
  writeFileSync(b, solidPng(5, 5, [2, 2, 2]));

  const projectId = run("create", "--title", "CLI Group Scale").project.id;
  const elA = run("add-image", projectId, "--file", a).element.id;
  run("move", projectId, "--element", elA, "--x", "10", "--y", "10");
  const groupId = run("group-create", projectId, "--name", "G", "--x", "0", "--y", "0", "--w", "50", "--h", "50").group.id;
  run("group-assign", projectId, "--elements", elA, "--group", groupId);

  const elB = run("add-image", projectId, "--file", b).element.id;
  run("move", projectId, "--element", elB, "--x", "25", "--y", "25");
  const nestedId = run("group-create", projectId, "--name", "S", "--x", "20", "--y", "20", "--w", "20", "--h", "20", "--parent", groupId).group.id;
  run("group-assign", projectId, "--elements", elB, "--group", nestedId);

  const scaled = run("group-scale", projectId, "--group", groupId, "--x", "0", "--y", "0", "--w", "100", "--h", "100");
  assert.deepEqual({ x: scaled.group.x, y: scaled.group.y, w: scaled.group.w, h: scaled.group.h }, { x: 0, y: 0, w: 100, h: 100 });

  const shown = run("show", projectId).project;
  const nested = shown.groups.find((group) => group.id === nestedId);
  assert.deepEqual({ x: nested.x, y: nested.y, w: nested.w, h: nested.h }, { x: 40, y: 40, w: 40, h: 40 });
  assert.deepEqual({ x: shown.elements.find((e) => e.id === elA).x, y: shown.elements.find((e) => e.id === elA).y }, { x: 20, y: 20 });
  assert.deepEqual({ x: shown.elements.find((e) => e.id === elB).x, y: shown.elements.find((e) => e.id === elB).y }, { x: 50, y: 50 });

  // Missing dims and an unknown group both fail loudly (no partial write).
  assert.throws(() => run("group-scale", projectId, "--group", groupId));
  assert.throws(() => run("group-scale", projectId, "--group", "nope", "--x", "0", "--y", "0", "--w", "10", "--h", "10"));
});
