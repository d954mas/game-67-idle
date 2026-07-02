// Z-order tests: reorderNode (element OR group among MERGED same-scope siblings) and
// its thin reorderElement delegate. Covers merged-sibling index math, lazy per-scope
// normalization (first reorder makes a scope explicit; other scopes untouched), the
// front-order hook on add/assign, render honoring the order, undo, strict vs. forgiving
// index handling, and CLI parity (element-reorder + node-reorder). Run:
//   node --test ai_studio/assets/canvas/tests/reorder.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import {
  addImage,
  assignToGroup,
  createGroup,
  createProject,
  getProject,
  patchElement,
  renderGroup,
  reorderElement,
  reorderNode,
  undoOp,
} from "../ops.mjs";
import { orderedChildren } from "../tree.mjs";
import { decodePng, solidPng } from "./png_fixture.mjs";

const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));
const ROOT = "C:/unused-repo-root"; // store is redirected via CANVAS_PROJECTS_ROOT
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-reorder-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// Three ungrouped elements A,B,C in array order.
function seedThree(root) {
  const project = createProject(root, { title: "Z" });
  const a = addImage(root, project.id, { name: "A", bytes: solidPng(4, 4, [1, 1, 1]) }).element;
  const b = addImage(root, project.id, { name: "B", bytes: solidPng(5, 5, [2, 2, 2]) }).element;
  const c = addImage(root, project.id, { name: "C", bytes: solidPng(6, 6, [3, 3, 3]) }).element;
  return { projectId: project.id, a, b, c };
}

// Computed paint order (back -> front) of a scope's ELEMENT children, by name — the paint
// order the model actually yields (via `order` when explicit, else the v1 array fallback),
// NOT the raw elements[] array, which reorderNode intentionally leaves untouched.
function elemOrder(root, projectId, scopeId = null) {
  return orderedChildren(getProject(root, projectId), scopeId)
    .filter((node) => node.kind === "element")
    .map((node) => node.ref.name);
}

// Merged sibling order (back -> front) of a scope as "<kind>:<label>".
function mergedOrder(root, projectId, scopeId, label) {
  return orderedChildren(getProject(root, projectId), scopeId).map((node) => `${node.kind}:${label(node)}`);
}

// ---- reorderElement delegate (element among merged siblings) ------------------

test("reorderElement moves an element among root siblings (computed order)", (t) => {
  tempProjects(t);
  const { projectId, a } = seedThree(ROOT);
  assert.deepEqual(elemOrder(ROOT, projectId), ["A", "B", "C"]);

  // Send A (index 0) to the front (index 2 = painted last / on top).
  reorderElement(ROOT, { projectId, elementId: a.id, index: 2 });
  assert.deepEqual(elemOrder(ROOT, projectId), ["B", "C", "A"]);

  // Bring A back one step (front -> index 1) now that the scope is explicit.
  reorderElement(ROOT, { projectId, elementId: a.id, index: 1 });
  assert.deepEqual(elemOrder(ROOT, projectId), ["B", "A", "C"]);
});

test("reorderElement delegate clamps an out-of-range index into the sibling range", (t) => {
  tempProjects(t);
  const { projectId, b } = seedThree(ROOT);
  // index 99 clamps to the last sibling slot (front).
  reorderElement(ROOT, { projectId, elementId: b.id, index: 99 });
  assert.deepEqual(elemOrder(ROOT, projectId), ["A", "C", "B"]);
  // index -5 clamps to 0 (back).
  reorderElement(ROOT, { projectId, elementId: b.id, index: -5 });
  assert.deepEqual(elemOrder(ROOT, projectId), ["B", "A", "C"]);
});

test("reorderElement reorders only within a group, leaving other scopes untouched", (t) => {
  tempProjects(t);
  const { projectId, a, b } = seedThree(ROOT);
  // A and B in group G; C stays at root.
  const { group } = createGroup(ROOT, { projectId, name: "G", fromElements: [a.id, b.id] });
  assert.deepEqual(elemOrder(ROOT, projectId, group.id), ["A", "B"]);

  // Send B to the back of its group (index 0) -> group order B,A. Root C untouched.
  reorderElement(ROOT, { projectId, elementId: b.id, index: 0 });
  assert.deepEqual(elemOrder(ROOT, projectId, group.id), ["B", "A"], "group siblings reordered");
  assert.deepEqual(elemOrder(ROOT, projectId, null), ["C"], "root element untouched");
});

test("reorderElement of a same-index no-op writes no journal entry", (t) => {
  tempProjects(t);
  const { projectId, a } = seedThree(ROOT);
  const before = getProject(ROOT, projectId).history_seq;
  reorderElement(ROOT, { projectId, elementId: a.id, index: 0 }); // already at index 0
  assert.equal(getProject(ROOT, projectId).history_seq, before, "no-op adds no history");
});

// ---- reorderNode: elements + groups over merged siblings ----------------------

test("reorderNode moves a GROUP among mixed root siblings; merged order reflects it", (t) => {
  tempProjects(t);
  const { projectId, b } = seedThree(ROOT);
  // Group only B, so root holds elements A, C and group G (anchored at B's index 1).
  const { group } = createGroup(ROOT, { projectId, name: "G", fromElements: [b.id] });
  const label = (node) => (node.kind === "group" ? "G" : node.ref.name);
  assert.deepEqual(mergedOrder(ROOT, projectId, null, label), ["element:A", "group:G", "element:C"]);

  // Send the group to the back (index 0) of its merged root siblings.
  reorderNode(ROOT, { projectId, nodeId: group.id, index: 0 });
  assert.deepEqual(mergedOrder(ROOT, projectId, null, label), ["group:G", "element:A", "element:C"]);

  // Bring the group to the front (index 2).
  reorderNode(ROOT, { projectId, nodeId: group.id, index: 2 });
  assert.deepEqual(mergedOrder(ROOT, projectId, null, label), ["element:A", "element:C", "group:G"]);
});

test("reorderNode of an ELEMENT places it relative to a sibling group in merged order", (t) => {
  tempProjects(t);
  const { projectId, a, b } = seedThree(ROOT);
  // Group B; root = element A, element C, group G. Move A to the front (index 2).
  const { group } = createGroup(ROOT, { projectId, name: "G", fromElements: [b.id] });
  const label = (node) => (node.kind === "group" ? "G" : node.ref.name);
  // Merged root before: [A, G, C]. Move A to the front (index 2) -> [G, C, A].
  reorderNode(ROOT, { projectId, nodeId: a.id, index: 2 });
  assert.deepEqual(mergedOrder(ROOT, projectId, null, label), ["group:G", "element:C", "element:A"]);
  // The group's own scope (B) is untouched by a root-scope reorder.
  assert.deepEqual(elemOrder(ROOT, projectId, group.id), ["B"]);
});

test("reorderNode normalizes ONLY its scope; sibling group's members keep no order", (t) => {
  tempProjects(t);
  const { projectId, a, b, c } = seedThree(ROOT);
  const { group } = createGroup(ROOT, { projectId, name: "G", fromElements: [b, c].map((e) => e.id) });
  // Reorder at ROOT (elements A + group G). Only root siblings get explicit order.
  reorderNode(ROOT, { projectId, nodeId: group.id, index: 0 });
  const project = getProject(ROOT, projectId);
  const rootA = project.elements.find((e) => e.id === a.id);
  const grp = project.groups.find((g) => g.id === group.id);
  const memberB = project.elements.find((e) => e.id === b.id);
  const memberC = project.elements.find((e) => e.id === c.id);
  assert.equal(typeof rootA.order, "number", "root element got explicit order");
  assert.equal(typeof grp.order, "number", "root group got explicit order");
  assert.equal(memberB.order, undefined, "group member untouched (no order)");
  assert.equal(memberC.order, undefined, "group member untouched (no order)");
});

test("second reorder works on the now-explicit scope", (t) => {
  tempProjects(t);
  const { projectId, a, c } = seedThree(ROOT);
  reorderElement(ROOT, { projectId, elementId: a.id, index: 2 }); // makes root explicit: B,C,A
  assert.deepEqual(elemOrder(ROOT, projectId), ["B", "C", "A"]);
  reorderElement(ROOT, { projectId, elementId: c.id, index: 2 }); // C to front over explicit orders
  assert.deepEqual(elemOrder(ROOT, projectId), ["B", "A", "C"]);
});

test("reorderNode is one journal entry; undo restores the previous order fields", (t) => {
  tempProjects(t);
  const { projectId, a } = seedThree(ROOT);
  const seqBefore = Number(getProject(ROOT, projectId).history_seq);
  reorderNode(ROOT, { projectId, nodeId: a.id, index: 2 });
  assert.equal(Number(getProject(ROOT, projectId).history_seq), seqBefore + 1, "exactly one entry");
  assert.deepEqual(elemOrder(ROOT, projectId), ["B", "C", "A"]);

  undoOp(ROOT, { projectId });
  const undone = getProject(ROOT, projectId);
  assert.deepEqual(elemOrder(ROOT, projectId), ["A", "B", "C"], "undo restores order");
  assert.ok(undone.elements.every((e) => e.order === undefined), "undo clears the order fields");
});

test("reorderNode throws on an out-of-range index and an unknown node", (t) => {
  tempProjects(t);
  const { projectId, a } = seedThree(ROOT);
  assert.throws(() => reorderNode(ROOT, { projectId, nodeId: a.id, index: 5 }), /out of range/);
  assert.throws(() => reorderNode(ROOT, { projectId, nodeId: a.id, index: -1 }), /out of range/);
  assert.throws(() => reorderNode(ROOT, { projectId, nodeId: "nope", index: 0 }), /node not found/);
});

// ---- front-order hook: scopes never go half-explicit --------------------------

test("addImage into an explicitly-ordered scope gets a FRONT order (scope stays explicit)", (t) => {
  tempProjects(t);
  const { projectId, a } = seedThree(ROOT);
  reorderElement(ROOT, { projectId, elementId: a.id, index: 2 }); // root explicit: B,C,A

  const added = addImage(ROOT, projectId, { name: "D", bytes: solidPng(3, 3, [9, 9, 9]) }).element;
  const project = getProject(ROOT, projectId);
  const stored = project.elements.find((e) => e.id === added.id);
  assert.equal(typeof stored.order, "number", "fresh image got an explicit order");
  // Scope stayed explicit (still sorts by order), with D at the FRONT.
  assert.equal(project.elements.every((e) => typeof e.order === "number"), true, "no half-explicit scope");
  assert.deepEqual(elemOrder(ROOT, projectId), ["B", "C", "A", "D"], "new image lands at the front");
});

test("assignToGroup into an explicit scope assigns front order; into an implicit scope drops order", (t) => {
  tempProjects(t);
  const { projectId, a, b, c } = seedThree(ROOT);
  // Make root explicit, then a target group whose (empty) scope is implicit.
  reorderElement(ROOT, { projectId, elementId: a.id, index: 2 });
  const { group } = createGroup(ROOT, { projectId, name: "G", x: 0, y: 0, w: 50, h: 50 });

  // Move C into the empty (implicit) group: its stale root order is dropped.
  assignToGroup(ROOT, { projectId, elementIds: [c.id], groupId: group.id });
  let project = getProject(ROOT, projectId);
  assert.equal(project.elements.find((e) => e.id === c.id).order, undefined, "order dropped into implicit scope");

  // Now reorder inside root again so root stays explicit, then move B back to root.
  reorderElement(ROOT, { projectId, elementId: b.id, index: 0 });
  assignToGroup(ROOT, { projectId, elementIds: [c.id], groupId: null }); // C back to explicit root
  project = getProject(ROOT, projectId);
  assert.equal(typeof project.elements.find((e) => e.id === c.id).order, "number", "front order into explicit root");
});

// ---- render honors the computed order -----------------------------------------

test("renderGroup composites members in the reordered z-order (skips without Python)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Overlap" });
  const projectId = project.id;
  // Two fully-overlapping opaque members at (0,0): red then green (green on top by default).
  const red = addImage(REPO_ROOT, projectId, { name: "red", bytes: solidPng(8, 8, [220, 40, 40]) }).element;
  const green = addImage(REPO_ROOT, projectId, { name: "green", bytes: solidPng(8, 8, [40, 180, 60]) }).element;
  patchElement(REPO_ROOT, projectId, red.id, { x: 0, y: 0 });
  patchElement(REPO_ROOT, projectId, green.id, { x: 0, y: 0 });
  const { group } = createGroup(REPO_ROOT, { projectId, name: "Ov", fromElements: [red.id, green.id] });

  let first;
  try {
    first = await renderGroup(REPO_ROOT, { projectId, groupId: group.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  // The members sit at world (0,0); the group frame is padded 24px, so (0,0) maps to
  // output (24,24). Sample (26,26) — inside the overlapping 8x8 members.
  // Default order (array): green is painted last -> on top over the overlap.
  let png = decodePng(readFileSync(first.path));
  assert.deepEqual(png.at(26, 26).slice(0, 3), [40, 180, 60], "green on top by default");

  // Send red to the front (index 1) of the group's members; re-render.
  reorderNode(REPO_ROOT, { projectId, nodeId: red.id, index: 1 });
  const second = await renderGroup(REPO_ROOT, { projectId, groupId: group.id, scale: 1 });
  png = decodePng(readFileSync(second.path));
  assert.deepEqual(png.at(26, 26).slice(0, 3), [220, 40, 40], "red on top after reorder");
});

// ---- CLI parity ---------------------------------------------------------------

test("cli element-reorder + node-reorder parity round-trip a z-order change", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-reorder-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const run = (...args) => {
    const out = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(out.trim().split("\n").filter(Boolean).at(-1));
  };
  const computed = (project, scopeId = null) =>
    orderedChildren(project, scopeId).filter((node) => node.kind === "element").map((node) => node.ref.name);

  const pa = join(dir, "a.png");
  const pb = join(dir, "b.png");
  writeFileSync(pa, solidPng(4, 4, [1, 1, 1]));
  writeFileSync(pb, solidPng(5, 5, [2, 2, 2]));

  const projectId = run("create", "--title", "CLI Z").project.id;
  const elA = run("add-image", projectId, "--file", pa).element.id;
  run("add-image", projectId, "--file", pb);

  // element-reorder: A (index 0) -> front (index 1).
  const moved = run("element-reorder", projectId, "--element", elA, "--index", "1");
  assert.equal(moved.index, 1);
  assert.deepEqual(computed(run("show", projectId).project), ["b.png", "a.png"]);

  // node-reorder the same element back to the back (index 0).
  const back = run("node-reorder", projectId, "--node", elA, "--index", "0");
  assert.equal(back.index, 0);
  assert.deepEqual(computed(run("show", projectId).project), ["a.png", "b.png"]);
});
