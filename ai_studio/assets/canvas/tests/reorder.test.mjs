// Z-order (reorderElement) tests: sibling reorder within root and within a group,
// index clamping, interleaved-scope preservation, undo restores the exact order,
// and CLI parity (element-reorder). No Python. Run:
//   node --test ai_studio/assets/canvas/tests/reorder.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import {
  addImage,
  assignToGroup,
  createGroup,
  createProject,
  getProject,
  reorderElement,
  undoOp,
} from "../ops.mjs";
import { solidPng } from "./png_fixture.mjs";

const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

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

// Three ungrouped elements A,B,C in array order, plus a helper to read the order.
function seedThree(root) {
  const project = createProject(root, { title: "Z" });
  const a = addImage(root, project.id, { name: "A", bytes: solidPng(4, 4, [1, 1, 1]) }).element;
  const b = addImage(root, project.id, { name: "B", bytes: solidPng(5, 5, [2, 2, 2]) }).element;
  const c = addImage(root, project.id, { name: "C", bytes: solidPng(6, 6, [3, 3, 3]) }).element;
  return { projectId: project.id, a, b, c };
}

const order = (root, projectId, filter = () => true) =>
  getProject(root, projectId).elements.filter(filter).map((e) => e.name);

const ROOT = "C:/unused-repo-root"; // store is redirected via CANVAS_PROJECTS_ROOT

test("reorderElement moves an element among root siblings", (t) => {
  tempProjects(t);
  const { projectId, a } = seedThree(ROOT);
  assert.deepEqual(order(ROOT, projectId), ["A", "B", "C"]);

  // Send A (index 0) to the front (index 2 = painted last / on top).
  reorderElement(ROOT, { projectId, elementId: a.id, index: 2 });
  assert.deepEqual(order(ROOT, projectId), ["B", "C", "A"]);

  // Bring A back one step (front -> index 1).
  reorderElement(ROOT, { projectId, elementId: a.id, index: 1 });
  assert.deepEqual(order(ROOT, projectId), ["B", "A", "C"]);
});

test("reorderElement clamps an out-of-range index into the sibling range", (t) => {
  tempProjects(t);
  const { projectId, b } = seedThree(ROOT);
  // index 99 clamps to the last sibling slot (front).
  reorderElement(ROOT, { projectId, elementId: b.id, index: 99 });
  assert.deepEqual(order(ROOT, projectId), ["A", "C", "B"]);
  // index -5 clamps to 0 (back).
  reorderElement(ROOT, { projectId, elementId: b.id, index: -5 });
  assert.deepEqual(order(ROOT, projectId), ["B", "A", "C"]);
});

test("reorderElement reorders only within a group, leaving other scopes untouched", (t) => {
  tempProjects(t);
  const { projectId, a, b, c } = seedThree(ROOT);
  // Put A and B in a group; C stays at root. Array order after assign is A,B,C.
  createGroup(ROOT, { projectId, name: "G", fromElements: [a.id, b.id] });
  assert.deepEqual(order(ROOT, projectId), ["A", "B", "C"]);

  // Reorder B to the front of its group (siblings A,B -> B is index 1 already);
  // send B to back (index 0) so the group order becomes B,A. C (root) unmoved.
  reorderElement(ROOT, { projectId, elementId: b.id, index: 0 });
  const groupOrder = order(ROOT, projectId, (e) => e.groupId);
  const rootOrder = order(ROOT, projectId, (e) => !e.groupId);
  assert.deepEqual(groupOrder, ["B", "A"], "group siblings reordered");
  assert.deepEqual(rootOrder, ["C"], "root element untouched");
});

test("reorderElement preserves interleaved non-sibling positions (pour into slots)", (t) => {
  tempProjects(t);
  const { projectId, a, b, c } = seedThree(ROOT);
  // Group only B (the middle one), so the flat array is [A(root), B(grp), C(root)]:
  // root siblings A,C occupy slots 0 and 2, with a group member interleaved at slot 1.
  createGroup(ROOT, { projectId, name: "G", fromElements: [b.id] });
  assert.deepEqual(order(ROOT, projectId), ["A", "B", "C"]);

  // Send root sibling C (root index 1) to the back (index 0). Only the root
  // subsequence [A,C] -> [C,A] permutes; B keeps its absolute slot between them.
  reorderElement(ROOT, { projectId, elementId: c.id, index: 0 });
  assert.deepEqual(order(ROOT, projectId), ["C", "B", "A"]);
  // B is still the single group member, unmoved in scope.
  assert.deepEqual(order(ROOT, projectId, (e) => e.groupId), ["B"]);
});

test("reorderElement is one journal entry; undo restores the exact previous order", (t) => {
  tempProjects(t);
  const { projectId, a } = seedThree(ROOT);
  assert.deepEqual(order(ROOT, projectId), ["A", "B", "C"]);
  reorderElement(ROOT, { projectId, elementId: a.id, index: 2 });
  assert.deepEqual(order(ROOT, projectId), ["B", "C", "A"]);
  const undone = undoOp(ROOT, { projectId }).project;
  assert.deepEqual(undone.elements.map((e) => e.name), ["A", "B", "C"], "undo restores order");
});

test("reorderElement of a same-index no-op writes no journal entry", (t) => {
  tempProjects(t);
  const { projectId, a } = seedThree(ROOT);
  const before = getProject(ROOT, projectId).history_seq;
  reorderElement(ROOT, { projectId, elementId: a.id, index: 0 }); // already at index 0
  assert.equal(getProject(ROOT, projectId).history_seq, before, "no-op adds no history");
});

test("cli element-reorder parity round-trips a z-order change", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-reorder-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const run = (...args) => {
    const out = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(out.trim().split("\n").filter(Boolean).at(-1));
  };
  const pa = join(dir, "a.png");
  const pb = join(dir, "b.png");
  writeFileSync(pa, solidPng(4, 4, [1, 1, 1]));
  writeFileSync(pb, solidPng(5, 5, [2, 2, 2]));

  const projectId = run("create", "--title", "CLI Z").project.id;
  const elA = run("add-image", projectId, "--file", pa).element.id;
  run("add-image", projectId, "--file", pb);
  // A at index 0 -> move to front (index 1).
  const moved = run("element-reorder", projectId, "--element", elA, "--index", "1");
  assert.equal(moved.index, 1);
  assert.deepEqual(run("show", projectId).project.elements.map((e) => e.name), ["b.png", "a.png"]);
});
