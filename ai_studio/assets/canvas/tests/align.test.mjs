// T0232 increment 1 — align / distribute. Pure target math (tree.alignMoves/
// distributeMoves) plus the journaled ops (alignNodes/distributeNodes), which reuse the
// SAME overlap-safe cascade moveNodes uses (extracted as ops.applyNodeMoves) — a moved
// group carries its whole subtree, and a node inside a moved group shifts once, with the
// topmost moved ancestor (never double-shifted). Every gesture = exactly ONE journal
// entry = one undo. Each op is exercised directly, over the pure tree math, over the HTTP
// adapter, and over the CLI (tool parity). Run:
//   node --test ai_studio/assets/canvas/tests/align.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import {
  addImage,
  alignNodes,
  assignToGroup,
  createGroup,
  createProject,
  distributeNodes,
  getProject,
  redoOp,
  reparentGroup,
  undoOp,
} from "../ops.mjs";
import { alignMoves, distributeMoves } from "../tree.mjs";
import { createCanvasApi } from "../api.mjs";
import { solidPng } from "./png_fixture.mjs";

const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));
const ROOT = "C:/unused-repo-root"; // store is redirected via CANVAS_PROJECTS_ROOT

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-align-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// An image element with an EXACT w/h (from the PNG's own pixel size) at a given x/y.
function img(pid, name, x, y, w, h, rgb = [10, 20, 30]) {
  return addImage(ROOT, pid, { name, bytes: solidPng(w, h, rgb), x, y }).element;
}

function byId(project, id) {
  return (
    (project.elements || []).find((item) => item.id === id) ||
    (project.groups || []).find((item) => item.id === id)
  );
}

// ---- alignMoves (pure) ---------------------------------------------------------

// A@(0,0,10,20) B@(50,100,30,10) C@(20,40,5,5) -> union bbox (0,0)-(80,110), w=80 h=110.
function seedAlignTrio() {
  const project = createProject(ROOT, { title: "AlignTrio" });
  const pid = project.id;
  const a = img(pid, "A", 0, 0, 10, 20);
  const b = img(pid, "B", 50, 100, 30, 10);
  const c = img(pid, "C", 20, 40, 5, 5);
  return { pid, a, b, c };
}

test("alignMoves: each align key targets the union bbox of 2+ nodes; already-aligned nodes are omitted", (t) => {
  tempProjects(t);
  const { pid, a, b, c } = seedAlignTrio();
  const project = getProject(ROOT, pid);
  const ids = [a.id, b.id, c.id];

  // A is already flush against the reference's left edge (x=0=minX) -> omitted.
  const left = alignMoves(project, ids, "left");
  assert.equal(left.find((m) => m.nodeId === a.id), undefined, "already-left-aligned node is not in the result");
  const leftById = new Map(left.map((m) => [m.nodeId, m]));
  assert.deepEqual([leftById.get(b.id).x, leftById.get(c.id).x], [0, 0]);

  const hcenter = new Map(alignMoves(project, ids, "hcenter").map((m) => [m.nodeId, m]));
  assert.deepEqual([hcenter.get(a.id).x, hcenter.get(b.id).x, hcenter.get(c.id).x], [35, 25, 37.5]);

  // B is already flush against the reference's right edge (50+30=80=maxX) -> omitted.
  const right = alignMoves(project, ids, "right");
  assert.equal(right.find((m) => m.nodeId === b.id), undefined, "already-right-aligned node is not in the result");
  const rightById = new Map(right.map((m) => [m.nodeId, m]));
  assert.deepEqual([rightById.get(a.id).x, rightById.get(c.id).x], [70, 75]);

  // A is already at the reference's top edge (y=0) -> omitted.
  const top = alignMoves(project, ids, "top");
  assert.equal(top.find((m) => m.nodeId === a.id), undefined, "already-top-aligned node is not in the result");
  const topById = new Map(top.map((m) => [m.nodeId, m]));
  assert.deepEqual([topById.get(b.id).y, topById.get(c.id).y], [0, 0]);

  const vcenter = new Map(alignMoves(project, ids, "vcenter").map((m) => [m.nodeId, m]));
  assert.deepEqual([vcenter.get(a.id).y, vcenter.get(b.id).y, vcenter.get(c.id).y], [45, 50, 52.5]);

  // B is already flush against the reference's bottom edge (100+10=110=maxY) -> omitted.
  const bottom = alignMoves(project, ids, "bottom");
  assert.equal(bottom.find((m) => m.nodeId === b.id), undefined, "already-bottom-aligned node is not in the result");
  const bottomById = new Map(bottom.map((m) => [m.nodeId, m]));
  assert.deepEqual([bottomById.get(a.id).y, bottomById.get(c.id).y], [90, 105]);
});

test("alignMoves: a single node inside a parent group aligns to the group's frame (Figma-auto); no parent is a loud error", (t) => {
  tempProjects(t);
  const project0 = createProject(ROOT, { title: "SingleFrame" });
  const pid = project0.id;
  const g = createGroup(ROOT, { projectId: pid, name: "Screen", x: 100, y: 100, w: 200, h: 150 }).group;
  const e = img(pid, "widget", 110, 110, 20, 10);
  assignToGroup(ROOT, { projectId: pid, elementIds: [e.id], groupId: g.id });
  const root = img(pid, "root-el", 0, 0, 10, 10); // no parent

  const project = getProject(ROOT, pid);
  const hcenter = alignMoves(project, [e.id], "hcenter");
  // ref = group frame (100,100,200,150); target x = 100 + 100 - 10 = 190.
  assert.deepEqual(hcenter, [{ nodeId: e.id, x: 190, y: 110 }]);

  assert.throws(() => alignMoves(project, [root.id], "left"), /select 2\+ objects, or one object inside a screen/);
  assert.throws(() => alignMoves(project, [e.id], "left", "selection"), /reference "selection" requires 2\+ nodes/);
});

test("alignMoves rejects bad input: empty ids, unknown align key, unknown node id, unknown reference", (t) => {
  tempProjects(t);
  const { pid, a, b } = seedAlignTrio();
  const project = getProject(ROOT, pid);
  assert.throws(() => alignMoves(project, [], "left"), /non-empty nodeIds/);
  assert.throws(() => alignMoves(project, [a.id, b.id], "diagonal"), /align must be one of/);
  assert.throws(() => alignMoves(project, [a.id, "nope"], "left"), /node not found/);
  assert.throws(() => alignMoves(project, [a.id, b.id], "left", "sideways"), /reference must be auto\/selection\/parent/);
});

// ---- distributeMoves (pure) -----------------------------------------------------

test("distributeMoves equalizes gaps sorted by position; endpoints stay fixed; needs 3+ nodes", (t) => {
  tempProjects(t);
  const project0 = createProject(ROOT, { title: "Distribute" });
  const pid = project0.id;
  const a = img(pid, "A", 0, 0, 10, 10);
  const b = img(pid, "B", 20, 0, 10, 10);
  const c = img(pid, "C", 100, 0, 10, 10);
  const project = getProject(ROOT, pid);

  const moves = distributeMoves(project, [a.id, b.id, c.id], "h");
  // span = (100+10)-0 = 110; totalSize = 30; gap = (110-30)/2 = 40.
  // A stays at 0 (endpoint); B -> 0+10+40=50; C stays at 100 (endpoint).
  assert.deepEqual(moves, [{ nodeId: b.id, x: 50, y: 0 }]);
  assert.equal(moves.find((m) => m.nodeId === a.id), undefined, "endpoint A is not in the result");
  assert.equal(moves.find((m) => m.nodeId === c.id), undefined, "endpoint C is not in the result");

  // A selection already evenly spaced -> no moves at all.
  const evenPid = createProject(ROOT, { title: "Even" }).id;
  const e0 = img(evenPid, "e0", 0, 0, 10, 10);
  const e1 = img(evenPid, "e1", 50, 0, 10, 10);
  const e2 = img(evenPid, "e2", 100, 0, 10, 10);
  const evenMoves = distributeMoves(getProject(ROOT, evenPid), [e0.id, e1.id, e2.id], "h");
  assert.deepEqual(evenMoves, [], "already-even spacing produces zero moves");

  assert.throws(() => distributeMoves(project, [a.id, b.id], "h"), /requires 3\+ nodes/);
  assert.throws(() => distributeMoves(project, [a.id, b.id, c.id], "z"), /axis must be "h" or "v"/);
  assert.throws(() => distributeMoves(project, [a.id, b.id, "nope"], "h"), /node not found/);
});

test("distributeMoves on the v axis; a mixed element+group selection treats a group's frame like any node's box", (t) => {
  tempProjects(t);
  const project0 = createProject(ROOT, { title: "DistributeV" });
  const pid = project0.id;
  const a = img(pid, "A", 0, 0, 10, 10);
  const g = createGroup(ROOT, { projectId: pid, name: "G", x: 0, y: 20, w: 20, h: 10 }).group;
  const c = img(pid, "C", 0, 100, 10, 10);
  const project = getProject(ROOT, pid);

  const moves = distributeMoves(project, [a.id, g.id, c.id], "v");
  // span = (100+10)-0 = 110; totalSize (h) = 10+10+10 = 30; gap = 40.
  // A stays at y=0; G -> 0+10+40=50; C stays at y=100.
  assert.deepEqual(moves, [{ nodeId: g.id, x: 0, y: 50 }]);
});

// ---- alignNodes / distributeNodes (op layer) -------------------------------------

test("alignNodes commits ONE journal entry; undo restores every node byte-exact", (t) => {
  tempProjects(t);
  const { pid, a, b, c } = seedAlignTrio();
  const before = getProject(ROOT, pid);
  const seqBefore = before.history_seq;

  const result = alignNodes(ROOT, { projectId: pid, nodeIds: [a.id, b.id, c.id], align: "hcenter" });
  assert.equal(result.moved.length, 3, "every node in this trio moves on hcenter");

  const after = getProject(ROOT, pid);
  assert.equal(after.history_seq, seqBefore + 1, "exactly one entry for the whole align gesture");
  assert.deepEqual([byId(after, a.id).x, byId(after, b.id).x, byId(after, c.id).x], [35, 25, 37.5]);

  const undone = undoOp(ROOT, { projectId: pid }).project;
  assert.deepEqual(undone.elements, before.elements, "undo restores every element byte-exact");
  assert.equal(undone.history_seq, seqBefore);

  const redone = redoOp(ROOT, { projectId: pid }).project;
  assert.deepEqual([byId(redone, a.id).x, byId(redone, b.id).x, byId(redone, c.id).x], [35, 25, 37.5]);
});

test("alignNodes: an already-aligned selection is a no-op (no journal entry)", (t) => {
  tempProjects(t);
  const project0 = createProject(ROOT, { title: "NoOpAlign" });
  const pid = project0.id;
  const a = img(pid, "A", 0, 0, 10, 10);
  const b = img(pid, "B", 0, 50, 10, 10); // already left-aligned with A
  const seqBefore = getProject(ROOT, pid).history_seq;

  const result = alignNodes(ROOT, { projectId: pid, nodeIds: [a.id, b.id], align: "left" });
  assert.deepEqual(result.moved, []);
  assert.equal(getProject(ROOT, pid).history_seq, seqBefore, "already-aligned selection writes no entry");
});

test("alignNodes: <2 nodes (no parent) and an unknown align/id are loud, atomic, and write no entry", (t) => {
  tempProjects(t);
  const { pid, a } = seedAlignTrio();
  const seqBefore = getProject(ROOT, pid).history_seq;
  assert.throws(() => alignNodes(ROOT, { projectId: pid, nodeIds: [a.id], align: "left" }), /select 2\+ objects/);
  assert.throws(() => alignNodes(ROOT, { projectId: pid, nodeIds: [a.id, "nope"], align: "left" }), /node not found/);
  assert.throws(() => alignNodes(ROOT, { projectId: pid, nodeIds: [a.id, "nope"], align: "sideways" }), /align must be one of/);
  assert.equal(getProject(ROOT, pid).history_seq, seqBefore, "no entry written on any rejected call");
});

test("distributeNodes: <3 nodes is a loud error; one journal entry on success; undo restores byte-exact", (t) => {
  tempProjects(t);
  const project0 = createProject(ROOT, { title: "DistributeOp" });
  const pid = project0.id;
  const a = img(pid, "A", 0, 0, 10, 10);
  const b = img(pid, "B", 20, 0, 10, 10);
  const c = img(pid, "C", 100, 0, 10, 10);
  const before = getProject(ROOT, pid);
  const seqBefore = before.history_seq;

  assert.throws(() => distributeNodes(ROOT, { projectId: pid, nodeIds: [a.id, b.id], axis: "h" }), /requires 3\+ nodes/);
  assert.equal(getProject(ROOT, pid).history_seq, seqBefore, "the rejected call wrote nothing");

  const result = distributeNodes(ROOT, { projectId: pid, nodeIds: [a.id, b.id, c.id], axis: "h" });
  assert.deepEqual(result.moved, [b.id]);
  const after = getProject(ROOT, pid);
  assert.equal(after.history_seq, seqBefore + 1, "exactly one entry for the whole distribute gesture");
  assert.equal(byId(after, b.id).x, 50);

  const undone = undoOp(ROOT, { projectId: pid }).project;
  assert.deepEqual(undone.elements, before.elements, "undo restores every element byte-exact");
});

// ---- overlap-safe cascade (shared with moveNodes) --------------------------------

// Root e0@(0,0,10,10). Group G@(100,0,50,50) with member m@(120,0,5,5). Nested group
// S@(110,0,20,20) (child of G) with member sm@(115,0,5,5). Mirrors multi_gesture.test.mjs's
// seedMove() nested fixture.
function seedNestedAlign() {
  const project = createProject(ROOT, { title: "NestedAlign" });
  const pid = project.id;
  const e0 = img(pid, "e0", 0, 0, 10, 10);
  const m = img(pid, "m", 120, 0, 5, 5);
  const sm = img(pid, "sm", 115, 0, 5, 5);
  const G = createGroup(ROOT, { projectId: pid, name: "G", x: 100, y: 0, w: 50, h: 50 }).group;
  const S = createGroup(ROOT, { projectId: pid, name: "S", x: 110, y: 0, w: 20, h: 20 }).group;
  assignToGroup(ROOT, { projectId: pid, elementIds: [m.id], groupId: G.id });
  assignToGroup(ROOT, { projectId: pid, elementIds: [sm.id], groupId: S.id });
  reparentGroup(ROOT, { projectId: pid, groupId: S.id, parentId: G.id });
  return { pid, e0, m, sm, G, S };
}

test("alignNodes: a group AND its own nested subgroup in the selection shift ONCE (topmost wins), never double-shifted", (t) => {
  tempProjects(t);
  const { pid, e0, m, sm, G, S } = seedNestedAlign();
  const seqBefore = getProject(ROOT, pid).history_seq;

  // Selection = e0, G, S. Reference = union bbox of e0(0-10), G(100-150), S(110-130):
  // minX=0, maxX=150, w=150, cx=75. hcenter targets: e0->70, G->50, S->65 (S's OWN
  // target, but S is a descendant of G, which is ALSO moving -> S must instead follow
  // G's cascade delta, landing at 110 + (G.target-G.orig) = 110 + (50-100) = 60).
  alignNodes(ROOT, { projectId: pid, nodeIds: [e0.id, G.id, S.id], align: "hcenter" });
  const after = getProject(ROOT, pid);

  assert.equal(after.history_seq, seqBefore + 1, "one entry for the whole align gesture");
  assert.equal(byId(after, e0.id).x, 70, "e0 lands on its own hcenter target");
  assert.equal(byId(after, G.id).x, 50, "G lands on its own hcenter target");
  assert.equal(byId(after, S.id).x, 60, "S follows G's cascade delta (-50), NOT its own target (65)");
  assert.equal(byId(after, m.id).x, 70, "G's direct member cascades by G's delta (120-50=70)");
  assert.equal(byId(after, sm.id).x, 65, "S's member cascades by the TOPMOST (G's) delta once: 115-50=65, not 115-45-50");
});

test("distributeNodes: a distributed group's member (not itself selected) cascades by the group's delta", (t) => {
  tempProjects(t);
  const project0 = createProject(ROOT, { title: "DistributeCascade" });
  const pid = project0.id;
  const e0 = img(pid, "e0", 0, 0, 10, 10);
  const G = createGroup(ROOT, { projectId: pid, name: "G", x: 50, y: 0, w: 20, h: 20 }).group;
  const m = img(pid, "m", 60, 0, 5, 5);
  assignToGroup(ROOT, { projectId: pid, elementIds: [m.id], groupId: G.id });
  const e1 = img(pid, "e1", 200, 0, 10, 10);
  const seqBefore = getProject(ROOT, pid).history_seq;

  // span=(200+10)-0=210; totalSize=10+20+10=40; gap=85. e0 stays 0; G -> 0+10+85=95
  // (delta +45); e1 stays 200.
  distributeNodes(ROOT, { projectId: pid, nodeIds: [e0.id, G.id, e1.id], axis: "h" });
  const after = getProject(ROOT, pid);

  assert.equal(after.history_seq, seqBefore + 1);
  assert.equal(byId(after, G.id).x, 95);
  assert.equal(byId(after, m.id).x, 105, "G's member cascades by G's delta (+45) though m was never in nodeIds");
});

// ---- cross-scope selection --------------------------------------------------------

test("alignNodes: a cross-scope selection (a root node + a node inside a different group) aligns in absolute space", (t) => {
  tempProjects(t);
  const project0 = createProject(ROOT, { title: "CrossScope" });
  const pid = project0.id;
  const root = img(pid, "root", 0, 0, 10, 10);
  const g = createGroup(ROOT, { projectId: pid, name: "G", x: 200, y: 200, w: 100, h: 100 }).group;
  const inGroup = img(pid, "inGroup", 220, 220, 10, 10);
  assignToGroup(ROOT, { projectId: pid, elementIds: [inGroup.id], groupId: g.id });

  const result = alignNodes(ROOT, { projectId: pid, nodeIds: [root.id, inGroup.id], align: "top" });
  assert.deepEqual(result.moved, [inGroup.id]);
  const after = getProject(ROOT, pid);
  assert.equal(byId(after, root.id).y, 0, "root node already at the reference top");
  assert.equal(byId(after, inGroup.id).y, 0, "cross-scope node aligns in absolute space, ignoring its parent group");
});

// ---- HTTP adapter parity -----------------------------------------------------

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

test("HTTP routes: nodes-align and nodes-distribute are each one entry; bad input is a loud 400", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const { pid, a, b, c } = seedAlignTrio();
  const seq0 = getProject(ROOT, pid).history_seq;

  const al = await invokeApi(handler, "POST", `/api/canvas/projects/${pid}/nodes-align`, {
    nodeIds: [a.id, b.id, c.id], align: "hcenter",
  });
  assert.equal(al.status, 200);
  assert.equal(getProject(ROOT, pid).history_seq, seq0 + 1);
  assert.equal(byId(getProject(ROOT, pid), a.id).x, 35, "align applied over HTTP");

  const seq1 = getProject(ROOT, pid).history_seq;
  const di = await invokeApi(handler, "POST", `/api/canvas/projects/${pid}/nodes-distribute`, {
    nodeIds: [a.id, b.id, c.id], axis: "v",
  });
  assert.equal(di.status, 200);
  assert.equal(getProject(ROOT, pid).history_seq, seq1 + 1);

  const bad = await invokeApi(handler, "POST", `/api/canvas/projects/${pid}/nodes-align`, { nodeIds: [a.id], align: "left" });
  assert.equal(bad.status, 400);
});

// ---- CLI parity ----------------------------------------------------------------

test("cli nodes-align / nodes-distribute parity", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-align-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const run = (...args) => {
    const out = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(out.trim().split("\n").filter(Boolean).at(-1));
  };

  const pa = join(dir, "a.png");
  const pb = join(dir, "b.png");
  const pc = join(dir, "c.png");
  writeFileSync(pa, solidPng(10, 10, [1, 1, 1]));
  writeFileSync(pb, solidPng(10, 10, [2, 2, 2]));
  writeFileSync(pc, solidPng(10, 10, [3, 3, 3]));

  const projectId = run("create", "--title", "CLI Align").project.id;
  const a = run("add-image", projectId, "--file", pa).element.id;
  run("move", projectId, "--element", a, "--x", "0", "--y", "0");
  const b = run("add-image", projectId, "--file", pb).element.id;
  run("move", projectId, "--element", b, "--x", "20", "--y", "5");
  const c = run("add-image", projectId, "--file", pc).element.id;
  run("move", projectId, "--element", c, "--x", "100", "--y", "10");

  const al = run("nodes-align", projectId, "--nodes", `${a},${b},${c}`, "--align", "top");
  assert.equal(al.nodeIds.length, 3);
  const afterAlign = run("show", projectId).project;
  assert.deepEqual(afterAlign.elements.map((e) => e.y), [0, 0, 0]);

  const di = run("nodes-distribute", projectId, "--nodes", `${a},${b},${c}`, "--axis", "h");
  assert.deepEqual(di.moved, [b]);
  const afterDist = run("show", projectId).project;
  assert.equal(afterDist.elements.find((e) => e.id === b).x, 50);

  // Loud failures: <3 for distribute, unknown align key.
  assert.throws(() => run("nodes-distribute", projectId, "--nodes", `${a},${b}`, "--axis", "h"));
  assert.throws(() => run("nodes-align", projectId, "--nodes", `${a},${b}`, "--align", "diagonal"));
});
