// T0223 multi-gesture op integrity: moveNodes (batched mixed element+group move),
// reorderNodes (multi-node z-order block), ungroupGroup (one-entry ungroup at the former
// z-slot). The law under test: every gesture = exactly ONE journal entry = one undo. Each
// op is exercised directly, over the HTTP adapter, and over the CLI (tool parity). Run:
//   node --test ai_studio/assets/canvas/tests/multi_gesture.test.mjs
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
  assignToGroup,
  createGroup,
  createProject,
  getProject,
  moveNodes,
  redoOp,
  reorderNodes,
  reparentGroup,
  undoOp,
  ungroupGroup,
} from "../ops.mjs";
import { createCanvasApi } from "../api.mjs";
import { orderedChildren } from "../tree.mjs";
import { solidPng } from "./png_fixture.mjs";

const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));
const ROOT = "C:/unused-repo-root"; // store is redirected via CANVAS_PROJECTS_ROOT

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-multi-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

function img(projectId, name, x, y, rgb) {
  return addImage(ROOT, projectId, { name, bytes: solidPng(4, 4, rgb), x, y }).element;
}

// Merged sibling order (back -> front) of a scope as "<kind>:<id>".
const mergedIds = (project, scopeId = null) => orderedChildren(project, scopeId).map((node) => node.id);
const elemNames = (project, scopeId = null) =>
  orderedChildren(project, scopeId).filter((node) => node.kind === "element").map((node) => node.ref.name);

// ---- moveNodes ---------------------------------------------------------------

// Root elements e0,e1; group G@(100,100) with member m + subgroup S (holding sm).
function seedMove() {
  const project = createProject(ROOT, { title: "Move" });
  const pid = project.id;
  const e0 = img(pid, "e0", 0, 0, [10, 0, 0]);
  const e1 = img(pid, "e1", 10, 10, [20, 0, 0]);
  const m = img(pid, "m", 120, 120, [30, 0, 0]);
  const sm = img(pid, "sm", 150, 150, [40, 0, 0]);
  const G = createGroup(ROOT, { projectId: pid, name: "G", x: 100, y: 100, w: 200, h: 200 }).group;
  const S = createGroup(ROOT, { projectId: pid, name: "S", x: 140, y: 140, w: 60, h: 60 }).group;
  assignToGroup(ROOT, { projectId: pid, elementIds: [m.id], groupId: G.id });
  assignToGroup(ROOT, { projectId: pid, elementIds: [sm.id], groupId: S.id });
  reparentGroup(ROOT, { projectId: pid, groupId: S.id, parentId: G.id });
  return { pid, e0, e1, m, sm, G, S };
}

test("moveNodes moves loose elements AND a group (subtree cascades) in ONE journal entry", (t) => {
  tempProjects(t);
  const { pid, e0, e1, m, sm, G, S } = seedMove();
  const seqBefore = getProject(ROOT, pid).history_seq;

  moveNodes(ROOT, { projectId: pid, moves: [
    { nodeId: e0.id, x: 5, y: 5 },
    { nodeId: G.id, x: 200, y: 200 }, // delta (+100,+100) cascades over G's subtree
  ] });

  const after = getProject(ROOT, pid);
  assert.equal(after.history_seq, seqBefore + 1, "exactly one entry for the whole mixed move");
  const byId = new Map([...after.elements.map((e) => [e.id, e]), ...after.groups.map((g) => [g.id, g])]);
  assert.deepEqual([byId.get(e0.id).x, byId.get(e0.id).y], [5, 5], "loose element lands on its target");
  assert.deepEqual([byId.get(e1.id).x, byId.get(e1.id).y], [10, 10], "unselected element untouched");
  assert.deepEqual([byId.get(G.id).x, byId.get(G.id).y], [200, 200], "group frame lands on its target");
  assert.deepEqual([byId.get(m.id).x, byId.get(m.id).y], [220, 220], "member cascades by the group delta");
  assert.deepEqual([byId.get(S.id).x, byId.get(S.id).y], [240, 240], "nested subgroup frame cascades");
  assert.deepEqual([byId.get(sm.id).x, byId.get(sm.id).y], [250, 250], "nested member cascades");

  // One undo restores every position; one redo re-applies.
  const undone = undoOp(ROOT, { projectId: pid }).project;
  const u = new Map([...undone.elements.map((e) => [e.id, e]), ...undone.groups.map((g) => [g.id, g])]);
  assert.deepEqual([u.get(e0.id).x, u.get(G.id).x, u.get(m.id).x, u.get(sm.id).x], [0, 100, 120, 150]);
  assert.equal(undone.history_seq, seqBefore);
  const redone = redoOp(ROOT, { projectId: pid }).project;
  const r = new Map(redone.groups.map((g) => [g.id, g]));
  assert.deepEqual([r.get(G.id).x, r.get(S.id).x], [200, 240]);
});

test("moveNodes is overlap-safe: a group AND a node in its subtree move once (topmost wins)", (t) => {
  tempProjects(t);
  const { pid, sm, G, S } = seedMove();
  // Move G by (+100,+100) and ALSO include S (a descendant of G) at S.orig+(100,100).
  moveNodes(ROOT, { projectId: pid, moves: [
    { nodeId: G.id, x: 200, y: 200 },
    { nodeId: S.id, x: 240, y: 240 },
  ] });
  const after = getProject(ROOT, pid);
  const smAfter = after.elements.find((e) => e.id === sm.id);
  const sAfter = after.groups.find((g) => g.id === S.id);
  // sm and S shift by ONE (100,100), not twice — no double application.
  assert.deepEqual([sAfter.x, sAfter.y], [240, 240], "subgroup shifts once");
  assert.deepEqual([smAfter.x, smAfter.y], [250, 250], "nested member shifts once, not to 350,350");
});

test("moveNodes rejects bad input atomically (no journal entry, no partial write)", (t) => {
  tempProjects(t);
  const { pid, e0 } = seedMove();
  const seqBefore = getProject(ROOT, pid).history_seq;
  assert.throws(() => moveNodes(ROOT, { projectId: pid, moves: [] }), /non-empty moves array/);
  assert.throws(() => moveNodes(ROOT, { projectId: pid }), /non-empty moves array/);
  assert.throws(() => moveNodes(ROOT, { projectId: pid, moves: [{ nodeId: "nope", x: 1, y: 1 }] }), /node not found/);
  assert.throws(() => moveNodes(ROOT, { projectId: pid, moves: [{ nodeId: e0.id, x: 1 }] }), /finite x and y/);
  // A good move followed by a bad one must not apply the good one.
  assert.throws(
    () => moveNodes(ROOT, { projectId: pid, moves: [{ nodeId: e0.id, x: 999, y: 999 }, { nodeId: "nope", x: 0, y: 0 }] }),
    /node not found/,
  );
  const after = getProject(ROOT, pid);
  assert.equal(after.history_seq, seqBefore, "no entry written on a rejected batch");
  assert.deepEqual([after.elements.find((e) => e.id === e0.id).x], [0], "the good element was not mutated");
});

// ---- reorderNodes ------------------------------------------------------------

// Four root elements A,B,C,D (array/back->front order).
function seedFour() {
  const project = createProject(ROOT, { title: "Order" });
  const pid = project.id;
  const a = img(pid, "A", 0, 0, [1, 0, 0]);
  const b = img(pid, "B", 0, 0, [2, 0, 0]);
  const c = img(pid, "C", 0, 0, [3, 0, 0]);
  const d = img(pid, "D", 0, 0, [4, 0, 0]);
  return { pid, a, b, c, d };
}

test("reorderNodes moves a non-contiguous block forward, preserving relative order, one entry", (t) => {
  tempProjects(t);
  const { pid, a, c } = seedFour();
  const seqBefore = getProject(ROOT, pid).history_seq;
  reorderNodes(ROOT, { projectId: pid, nodeIds: [a.id, c.id], direction: "forward" });
  assert.deepEqual(elemNames(getProject(ROOT, pid)), ["B", "A", "D", "C"]);
  assert.equal(getProject(ROOT, pid).history_seq, seqBefore + 1, "one journal entry");
});

test("reorderNodes front/back/backward block moves match Figma semantics", (t) => {
  tempProjects(t);
  let s = seedFour();
  reorderNodes(ROOT, { projectId: s.pid, nodeIds: [s.a.id, s.b.id], direction: "front" });
  assert.deepEqual(elemNames(getProject(ROOT, s.pid)), ["C", "D", "A", "B"]);

  s = seedFour();
  reorderNodes(ROOT, { projectId: s.pid, nodeIds: [s.b.id, s.d.id], direction: "back" });
  assert.deepEqual(elemNames(getProject(ROOT, s.pid)), ["B", "D", "A", "C"]);

  s = seedFour();
  reorderNodes(ROOT, { projectId: s.pid, nodeIds: [s.b.id, s.d.id], direction: "backward" });
  assert.deepEqual(elemNames(getProject(ROOT, s.pid)), ["B", "A", "D", "C"]);
});

test("reorderNodes with an absolute index inserts the block among the unselected siblings", (t) => {
  tempProjects(t);
  const { pid, a } = seedFour();
  reorderNodes(ROOT, { projectId: pid, nodeIds: [a.id], index: 2 }); // others [B,C,D], insert A at 2
  assert.deepEqual(elemNames(getProject(ROOT, pid)), ["B", "C", "A", "D"]);
});

test("reorderNodes across scopes applies per scope but stays ONE journal entry", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "CrossScope" });
  const pid = project.id;
  const a = img(pid, "A", 0, 0, [1, 0, 0]);
  img(pid, "B", 0, 0, [2, 0, 0]);
  const c = img(pid, "C", 0, 0, [3, 0, 0]);
  const d = img(pid, "D", 0, 0, [4, 0, 0]);
  const G = createGroup(ROOT, { projectId: pid, name: "G", x: 0, y: 0, w: 50, h: 50 }).group;
  assignToGroup(ROOT, { projectId: pid, elementIds: [c.id, d.id], groupId: G.id });
  const seqBefore = getProject(ROOT, pid).history_seq;

  // A lives at root (siblings A,B + group G); C lives in G (siblings C,D). Bring both to front.
  reorderNodes(ROOT, { projectId: pid, nodeIds: [a.id, c.id], direction: "front" });
  const after = getProject(ROOT, pid);
  assert.equal(after.history_seq, seqBefore + 1, "cross-scope reorder is still ONE entry");
  assert.deepEqual(elemNames(after, null), ["B", "A"], "root: A to front of its scope");
  assert.deepEqual(elemNames(after, G.id), ["D", "C"], "group: C to front of its scope");
});

test("reorderNodes undo restores the previous order; validation is loud", (t) => {
  tempProjects(t);
  const { pid, a, c } = seedFour();
  reorderNodes(ROOT, { projectId: pid, nodeIds: [a.id, c.id], direction: "front" });
  assert.deepEqual(elemNames(getProject(ROOT, pid)), ["B", "D", "A", "C"]);
  undoOp(ROOT, { projectId: pid });
  assert.deepEqual(elemNames(getProject(ROOT, pid)), ["A", "B", "C", "D"], "undo restores the order");

  assert.throws(() => reorderNodes(ROOT, { projectId: pid, nodeIds: [] }), /non-empty nodeIds/);
  assert.throws(() => reorderNodes(ROOT, { projectId: pid, nodeIds: [a.id] }), /exactly one of direction/);
  assert.throws(() => reorderNodes(ROOT, { projectId: pid, nodeIds: [a.id], direction: "front", index: 0 }), /exactly one of direction/);
  assert.throws(() => reorderNodes(ROOT, { projectId: pid, nodeIds: [a.id], direction: "sideways" }), /blockReorder requires|direction/);
  assert.throws(() => reorderNodes(ROOT, { projectId: pid, nodeIds: ["nope"], direction: "front" }), /node not found/);
});

test("reorderNodes writes NO entry when no scope's order changes (block already at the edge)", (t) => {
  tempProjects(t);
  const { pid, c, d } = seedFour();
  const seqBefore = getProject(ROOT, pid).history_seq;
  // C,D are already the front block; bringing them forward changes nothing.
  reorderNodes(ROOT, { projectId: pid, nodeIds: [c.id, d.id], direction: "forward" });
  assert.equal(getProject(ROOT, pid).history_seq, seqBefore, "a no-op reorder writes no journal entry");
  assert.deepEqual(elemNames(getProject(ROOT, pid)), ["A", "B", "C", "D"]);
});

test("reorderNodes rejects an index on a cross-scope selection", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "IdxCross" });
  const pid = project.id;
  const a = img(pid, "A", 0, 0, [1, 0, 0]);
  const c = img(pid, "C", 0, 0, [3, 0, 0]);
  const G = createGroup(ROOT, { projectId: pid, name: "G", x: 0, y: 0, w: 50, h: 50 }).group;
  assignToGroup(ROOT, { projectId: pid, elementIds: [c.id], groupId: G.id });
  assert.throws(() => reorderNodes(ROOT, { projectId: pid, nodeIds: [a.id, c.id], index: 0 }), /single-scope/);
});

// ---- ungroupGroup ------------------------------------------------------------

// Root: A, [G: C, D, [H: E]], B. G sits in the MIDDLE of root; H is nested in G.
function seedUngroup() {
  const project = createProject(ROOT, { title: "Ungroup" });
  const pid = project.id;
  const a = img(pid, "A", 0, 0, [1, 0, 0]);
  const c = img(pid, "C", 0, 0, [2, 0, 0]);
  const d = img(pid, "D", 0, 0, [3, 0, 0]);
  const e = img(pid, "E", 0, 0, [4, 0, 0]);
  const b = img(pid, "B", 0, 0, [5, 0, 0]);
  const H = createGroup(ROOT, { projectId: pid, name: "H", fromElements: [e.id] }).group;
  const G = createGroup(ROOT, { projectId: pid, name: "G", fromElements: [c.id, d.id] }).group;
  reparentGroup(ROOT, { projectId: pid, groupId: H.id, parentId: G.id });
  return { pid, a, b, c, d, e, G, H };
}

test("ungroupGroup drops children (elements + subgroups) at the group's former z-slot, one entry", (t) => {
  tempProjects(t);
  const { pid, c, d, e, G, H } = seedUngroup();
  const before = getProject(ROOT, pid);
  const seqBefore = before.history_seq;

  // Expected root order = parent siblings with G replaced by G's direct children (internal order).
  const parentSiblings = mergedIds(before, null);
  const children = mergedIds(before, G.id);
  const slot = parentSiblings.indexOf(G.id);
  const expected = [...parentSiblings.slice(0, slot), ...children, ...parentSiblings.slice(slot + 1)];

  ungroupGroup(ROOT, { projectId: pid, groupId: G.id });
  const after = getProject(ROOT, pid);
  assert.equal(after.history_seq, seqBefore + 1, "ungroup is ONE journal entry");
  assert.deepEqual(mergedIds(after, null), expected, "children land at the group's exact former slot, internal order kept");
  assert.equal(after.groups.some((g) => g.id === G.id), false, "the empty group is removed");
  assert.equal(after.elements.find((x) => x.id === c.id).groupId ?? null, null, "direct element reparented to root");
  assert.equal(after.elements.find((x) => x.id === d.id).groupId ?? null, null, "direct element reparented to root");
  assert.equal(after.groups.find((g) => g.id === H.id).parentId ?? null, null, "direct subgroup reparented to root");
  assert.equal(after.elements.find((x) => x.id === e.id).groupId, H.id, "grandchild stays under the surviving subgroup");

  // One undo restores the group EXACTLY (deep-equal elements + groups to the pre-ungroup state).
  const undone = undoOp(ROOT, { projectId: pid }).project;
  assert.deepEqual(undone.elements, before.elements, "undo restores every element's scope + order exactly");
  assert.deepEqual(undone.groups, before.groups, "undo restores the group exactly");
});

test("ungroupGroup on a top-level group lands children at root; unknown group throws", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "TopUngroup" });
  const pid = project.id;
  const c = img(pid, "C", 0, 0, [2, 0, 0]);
  const d = img(pid, "D", 0, 0, [3, 0, 0]);
  const G = createGroup(ROOT, { projectId: pid, name: "G", fromElements: [c.id, d.id] }).group;
  ungroupGroup(ROOT, { projectId: pid, groupId: G.id });
  const after = getProject(ROOT, pid);
  assert.equal(after.groups.length, 0, "group dissolved");
  assert.deepEqual(after.elements.map((x) => x.groupId ?? null), [null, null], "children now at root");
  assert.throws(() => ungroupGroup(ROOT, { projectId: pid, groupId: "grp_missing" }), /group not found/);
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

test("HTTP routes: nodes-move, nodes-reorder, groups/<id>/ungroup are each one entry", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const { pid, e0, G, m } = seedMove();
  const seq0 = getProject(ROOT, pid).history_seq;

  const mv = await invokeApi(handler, "POST", `/api/canvas/projects/${pid}/nodes-move`, {
    moves: [{ nodeId: e0.id, x: 7, y: 7 }, { nodeId: G.id, x: 200, y: 200 }],
  });
  assert.equal(mv.status, 200);
  const afterMove = getProject(ROOT, pid);
  assert.equal(afterMove.history_seq, seq0 + 1);
  assert.deepEqual([afterMove.elements.find((e) => e.id === m.id).x], [220], "group member cascaded over HTTP");

  const four = createProject(ROOT, { title: "HttpZ" });
  const a = img(four.id, "A", 0, 0, [1, 0, 0]);
  img(four.id, "B", 0, 0, [2, 0, 0]);
  const cc = img(four.id, "C", 0, 0, [3, 0, 0]);
  const seqZ = getProject(ROOT, four.id).history_seq;
  const rz = await invokeApi(handler, "POST", `/api/canvas/projects/${four.id}/nodes-reorder`, {
    nodeIds: [a.id, cc.id], direction: "front",
  });
  assert.equal(rz.status, 200);
  assert.equal(getProject(ROOT, four.id).history_seq, seqZ + 1);
  assert.deepEqual(elemNames(getProject(ROOT, four.id)), ["B", "A", "C"]);

  const ug = await invokeApi(handler, "POST", `/api/canvas/projects/${pid}/groups/${G.id}/ungroup`, {});
  assert.equal(ug.status, 200);
  assert.equal(getProject(ROOT, pid).groups.some((g) => g.id === G.id), false, "group dissolved over HTTP");

  // A bad move is a loud 400, not a silent 200.
  const bad = await invokeApi(handler, "POST", `/api/canvas/projects/${pid}/nodes-move`, { moves: [] });
  assert.equal(bad.status, 400);
});

// ---- CLI parity --------------------------------------------------------------

test("cli nodes-move / nodes-reorder / group-ungroup parity", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-multi-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const run = (...args) => {
    const out = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(out.trim().split("\n").filter(Boolean).at(-1));
  };
  const names = (project, scopeId = null) =>
    orderedChildren(project, scopeId).filter((node) => node.kind === "element").map((node) => node.ref.name);

  const pa = join(dir, "a.png");
  const pb = join(dir, "b.png");
  const pc = join(dir, "c.png");
  writeFileSync(pa, solidPng(4, 4, [1, 1, 1]));
  writeFileSync(pb, solidPng(5, 5, [2, 2, 2]));
  writeFileSync(pc, solidPng(6, 6, [3, 3, 3]));

  const projectId = run("create", "--title", "CLI Multi").project.id;
  const a = run("add-image", projectId, "--file", pa).element.id;
  const b = run("add-image", projectId, "--file", pb).element.id;
  const c = run("add-image", projectId, "--file", pc).element.id;

  // nodes-move: move two elements at once via a --json moves file.
  const movesPath = join(dir, "moves.json");
  writeFileSync(movesPath, JSON.stringify([{ nodeId: a, x: 50, y: 60 }, { nodeId: b, x: 70, y: 80 }]));
  const mv = run("nodes-move", projectId, "--json", movesPath);
  assert.equal(mv.count, 2);
  const afterMove = run("show", projectId).project;
  assert.deepEqual([afterMove.elements.find((e) => e.id === a).x, afterMove.elements.find((e) => e.id === b).y], [50, 80]);

  // nodes-reorder: bring A and C to the front as a block (element names are file basenames).
  run("nodes-reorder", projectId, "--nodes", `${a},${c}`, "--direction", "front");
  assert.deepEqual(names(run("show", projectId).project), ["b.png", "a.png", "c.png"]);

  // group-ungroup: group A,B then dissolve; children return to root.
  const G = run("group-create", projectId, "--name", "G", "--elements", `${a},${b}`).group.id;
  const ug = run("group-ungroup", projectId, "--group", G);
  assert.equal(ug.ungrouped, G);
  assert.equal(run("show", projectId).project.groups.length, 0, "group dissolved via CLI");
});
