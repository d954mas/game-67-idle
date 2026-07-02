// Canvas clipboard ops (T0227): pasteNodes / duplicateNodes / deleteNodes. Each is ONE
// journal entry (one undo); paste re-mints every id, preserves nesting + relative z-order,
// survives deletion of the source (immutable content-addressed files/), and validates the
// spec loudly before any write. deleteNodes removes mixed element+group-subtree selections
// in one entry and deep-restores z-slots on undo. Run:
//   node --test ai_studio/assets/canvas/tests/clipboard.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import {
  addImage,
  assignToGroup,
  createGroup,
  createProject,
  deleteNodes,
  duplicateNodes,
  getProject,
  pasteNodes,
  redoOp,
  reorderNode,
  undoOp,
} from "../ops.mjs";
import { buildNodesSpec, orderedChildren } from "../tree.mjs";
import { solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-clip-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// Computed back -> front order of a scope, labelled (groups prefixed "G:").
function names(project, scope = null) {
  return orderedChildren(project, scope).map((node) => (node.kind === "group" ? `G:${node.ref.name}` : node.ref.name));
}

test("pasteNodes instantiates a copied element with a NEW id at +offset; one entry; undo removes it", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Paste" });
  const el = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng(4, 4, [10, 20, 30]), x: 5, y: 7 }).element;
  const spec = buildNodesSpec(getProject(ROOT, project.id), [el.id]);
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const result = pasteNodes(ROOT, { projectId: project.id, spec, dx: 16, dy: 16, scopeId: null });
  assert.equal(result.count, 1);
  assert.equal(result.elementIds.length, 1);
  const newId = result.elementIds[0];
  assert.notEqual(newId, el.id, "pasted element has a fresh id");

  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "one journal entry");
  assert.equal(after.elements.length, 2);
  const pasted = after.elements.find((e) => e.id === newId);
  assert.deepEqual([pasted.x, pasted.y], [21, 23], "shifted by +16,+16");
  assert.equal(pasted.src, el.src, "references the SAME immutable file");

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.elements.length, 1, "one undo removes the paste");
  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.length, 2, "redo re-applies the paste");
});

test("pasteNodes deep-copies a group subtree with fresh ids and preserved nesting; one undo removes all", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Deep" });
  const pid = project.id;
  const red = addImage(ROOT, pid, { name: "red", bytes: solidPng(4, 4, [200, 0, 0]) }).element;
  const green = addImage(ROOT, pid, { name: "green", bytes: solidPng(4, 4, [0, 200, 0]) }).element;
  const outer = createGroup(ROOT, { projectId: pid, name: "Outer", x: 0, y: 0, w: 100, h: 100 }).group;
  const inner = createGroup(ROOT, { projectId: pid, name: "Inner", x: 10, y: 10, w: 40, h: 40, parentId: outer.id }).group;
  assignToGroup(ROOT, { projectId: pid, elementIds: [red.id], groupId: outer.id });
  assignToGroup(ROOT, { projectId: pid, elementIds: [green.id], groupId: inner.id });

  const spec = buildNodesSpec(getProject(ROOT, pid), [outer.id]);
  const seqBefore = getProject(ROOT, pid).history_seq;
  const result = pasteNodes(ROOT, { projectId: pid, spec, dx: 16, dy: 16, scopeId: null });
  assert.equal(result.groupIds.length, 1);
  const newOuter = result.groupIds[0];
  assert.notEqual(newOuter, outer.id, "pasted outer has a fresh id");

  const after = getProject(ROOT, pid);
  assert.equal(after.history_seq, seqBefore + 1, "one entry for the whole subtree");
  assert.equal(after.groups.length, 4, "outer + inner duplicated");
  assert.equal(after.elements.length, 4, "red + green duplicated");
  const newInner = after.groups.find((g) => g.parentId === newOuter);
  assert.ok(newInner && newInner.id !== inner.id, "pasted inner nested under pasted outer with a new id");
  const pastedRed = after.elements.find((e) => e.groupId === newOuter && e.name === "red");
  const pastedGreen = after.elements.find((e) => e.groupId === newInner.id && e.name === "green");
  assert.ok(pastedRed && pastedRed.id !== red.id, "red re-created under the pasted outer");
  assert.ok(pastedGreen && pastedGreen.id !== green.id, "green re-created under the pasted inner");

  const undone = undoOp(ROOT, { projectId: pid }).project;
  assert.equal(undone.groups.length, 2, "one undo removes the whole pasted subtree");
  assert.equal(undone.elements.length, 2);
});

test("paste works AFTER the source is deleted (immutable files/ keeps the ref valid)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "AfterDelete" });
  const pid = project.id;
  const el = addImage(ROOT, pid, { name: "a.png", bytes: solidPng(4, 4, [1, 2, 3]), x: 0, y: 0 }).element;
  const spec = buildNodesSpec(getProject(ROOT, pid), [el.id]);

  deleteNodes(ROOT, { projectId: pid, nodeIds: [el.id] });
  assert.equal(getProject(ROOT, pid).elements.length, 0, "source deleted");

  const result = pasteNodes(ROOT, { projectId: pid, spec, dx: 16, dy: 16, scopeId: null });
  const after = getProject(ROOT, pid);
  assert.equal(after.elements.length, 1, "paste re-creates the element after the source was deleted");
  assert.equal(after.elements[0].id, result.elementIds[0]);
  assert.equal(after.elements[0].src, el.src, "references the same immutable file");
});

test("deleteNodes deletes a mixed element+group selection in ONE entry; undo deep-restores z-order", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "MixedDelete" });
  const pid = project.id;
  const a = addImage(ROOT, pid, { name: "a", bytes: solidPng(4, 4, [10, 0, 0]) }).element;
  const b = addImage(ROOT, pid, { name: "b", bytes: solidPng(4, 4, [0, 10, 0]) }).element;
  const c = addImage(ROOT, pid, { name: "c", bytes: solidPng(4, 4, [0, 0, 10]) }).element;
  const g = createGroup(ROOT, { projectId: pid, name: "G", x: 0, y: 0, w: 50, h: 50 }).group;
  assignToGroup(ROOT, { projectId: pid, elementIds: [c.id], groupId: g.id });
  // Make the root scope explicit (real permutation) so z-order is carried in `order` fields.
  reorderNode(ROOT, { projectId: pid, nodeId: b.id, index: 2 });
  const before = getProject(ROOT, pid);
  const rootOrderBefore = names(before);
  const seqBefore = before.history_seq;

  const result = deleteNodes(ROOT, { projectId: pid, nodeIds: [b.id, g.id] });
  assert.deepEqual(result.removedGroups, [g.id]);
  assert.deepEqual(result.removedElements.slice().sort(), [b.id, c.id].sort(), "group subtree element removed too");

  const afterDel = getProject(ROOT, pid);
  assert.equal(afterDel.history_seq, seqBefore + 1, "one entry");
  assert.equal(afterDel.groups.length, 0);
  assert.equal(afterDel.elements.length, 1, "only a survives");

  const undone = undoOp(ROOT, { projectId: pid }).project;
  assert.equal(undone.groups.length, 1, "group restored");
  assert.equal(undone.elements.length, 3, "b and c restored");
  assert.deepEqual(names(undone), rootOrderBefore, "root z-order restored exactly");
  assert.equal(undone.elements.find((e) => e.id === c.id).groupId, g.id, "c restored into its group");
});

test("duplicateNodes duplicates live nodes in place (+16) into the originals' scope; one entry", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Dup" });
  const pid = project.id;
  const g = createGroup(ROOT, { projectId: pid, name: "Frame", x: 0, y: 0, w: 80, h: 80 }).group;
  const a = addImage(ROOT, pid, { name: "a", bytes: solidPng(4, 4, [9, 9, 9]), x: 3, y: 4 }).element;
  assignToGroup(ROOT, { projectId: pid, elementIds: [a.id], groupId: g.id });
  const seqBefore = getProject(ROOT, pid).history_seq;

  const result = duplicateNodes(ROOT, { projectId: pid, nodeIds: [a.id] });
  assert.equal(result.count, 1);
  const after = getProject(ROOT, pid);
  assert.equal(after.history_seq, seqBefore + 1, "one entry");
  const dup = after.elements.find((e) => e.id === result.elementIds[0]);
  assert.equal(dup.groupId, g.id, "duplicate lands in the same scope as the source (default)");
  assert.deepEqual([dup.x, dup.y], [19, 20], "offset +16,+16 by default");
});

test("pasteNodes validates loudly BEFORE any write (empty / unknown file / unknown kind)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Bad" });
  const pid = project.id;
  const el = addImage(ROOT, pid, { name: "a.png", bytes: solidPng(4, 4, [1, 2, 3]) }).element;
  const good = buildNodesSpec(getProject(ROOT, pid), [el.id]);
  const seqBefore = getProject(ROOT, pid).history_seq;

  assert.throws(() => pasteNodes(ROOT, { projectId: pid, spec: { nodes: [] } }), /no nodes to paste/);
  assert.throws(() => pasteNodes(ROOT, { projectId: pid, spec: {} }), /no nodes to paste/);
  const badFile = {
    schema: good.schema,
    nodes: [{ kind: "element", element: { type: "image", x: 0, y: 0, w: 4, h: 4, src: "files/deadbeef.png", name: "x" } }],
  };
  assert.throws(() => pasteNodes(ROOT, { projectId: pid, spec: badFile }), /unknown file/);
  assert.throws(() => pasteNodes(ROOT, { projectId: pid, spec: { nodes: [{ kind: "widget" }] } }), /unknown kind/);

  assert.equal(getProject(ROOT, pid).history_seq, seqBefore, "no journal entry on a rejected paste");
  assert.equal(getProject(ROOT, pid).elements.length, 1, "no element added on a rejected paste");
});

test("pasteNodes lands the roots on top of an explicit scope, preserving their relative order", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Order" });
  const pid = project.id;
  const a = addImage(ROOT, pid, { name: "a", bytes: solidPng(4, 4, [1, 0, 0]) }).element;
  const b = addImage(ROOT, pid, { name: "b", bytes: solidPng(4, 4, [0, 1, 0]) }).element;
  // Real permutation makes root explicit: [a,b] -> [b,a].
  reorderNode(ROOT, { projectId: pid, nodeId: a.id, index: 1 });
  assert.deepEqual(names(getProject(ROOT, pid)), ["b", "a"]);

  const spec = buildNodesSpec(getProject(ROOT, pid), [a.id, b.id]);
  assert.deepEqual(spec.nodes.map((n) => n.element.name), ["b", "a"], "roots captured back -> front");

  pasteNodes(ROOT, { projectId: pid, spec, dx: 16, dy: 16, scopeId: null });
  const order = names(getProject(ROOT, pid));
  assert.equal(order.length, 4);
  assert.deepEqual(order.slice(0, 2), ["b", "a"], "existing nodes stay behind");
  assert.deepEqual(order.slice(2), ["b", "a"], "pasted block on top, relative order preserved");
});

test("pasteNodes into a group scope parents the roots under that group", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Scope" });
  const pid = project.id;
  const g = createGroup(ROOT, { projectId: pid, name: "Dest", x: 0, y: 0, w: 60, h: 60 }).group;
  const a = addImage(ROOT, pid, { name: "a", bytes: solidPng(4, 4, [5, 5, 5]) }).element;
  const spec = buildNodesSpec(getProject(ROOT, pid), [a.id]);

  const result = pasteNodes(ROOT, { projectId: pid, spec, dx: 0, dy: 0, scopeId: g.id });
  const after = getProject(ROOT, pid);
  const pasted = after.elements.find((e) => e.id === result.elementIds[0]);
  assert.equal(pasted.groupId, g.id, "pasted into the destination group scope");
});

test("deleteNodes rejects an unknown id atomically and an empty list loudly", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "DelGuard" });
  const pid = project.id;
  const a = addImage(ROOT, pid, { name: "a", bytes: solidPng(4, 4, [1, 1, 1]) }).element;
  const seqBefore = getProject(ROOT, pid).history_seq;
  assert.throws(() => deleteNodes(ROOT, { projectId: pid, nodeIds: [] }), /non-empty nodeIds/);
  assert.throws(() => deleteNodes(ROOT, { projectId: pid, nodeIds: [a.id, "el_missing"] }), /node not found/);
  assert.equal(getProject(ROOT, pid).history_seq, seqBefore, "no entry on a rejected delete");
  assert.equal(getProject(ROOT, pid).elements.length, 1, "no element deleted");
});
