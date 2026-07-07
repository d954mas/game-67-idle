// Nested-group tests (increment 3): createGroup parentId + fromElements common-parent
// default, reparentGroup (order normalization + cycle guard + unknown parent),
// patchGroup 2-level move cascade, deleteGroup 2-level subtree, the Ungroup one-level
// dissolve composition, recursive render (nested background + mixed-sibling order), the
// top-level-only exportProject filter, and v1 migration. The render/export tests drive
// render_group.py and skip cleanly when Python/PIL is unavailable. Run:
//   node --test ai_studio/assets/canvas/tests/nesting.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import {
  addImage,
  assignToGroup,
  createGroup,
  createProject,
  deleteGroup,
  exportProject,
  getProject,
  patchElement,
  patchGroup,
  redoOp,
  renderGroup,
  reorderNode,
  reparentGroup,
  undoOp,
} from "../ops.mjs";
import { orderedChildren } from "../tree.mjs";
import { decodePng, encodePng, solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-nesting-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// red@(10,10) 8x8 + green@(30,20) 6x6 wrapped into an OUTER screen group and an INNER
// widget group (outer contains inner contains red+green). Returns the ids.
function seedNested(root) {
  const project = createProject(root, { title: "Nest" });
  const projectId = project.id;
  const red = addImage(root, projectId, { name: "red.png", bytes: encodePng(8, 8, () => [220, 40, 40]) }).element;
  patchElement(root, projectId, red.id, { x: 10, y: 10 });
  const green = addImage(root, projectId, { name: "green.png", bytes: encodePng(6, 6, () => [40, 180, 60]) }).element;
  patchElement(root, projectId, green.id, { x: 30, y: 20 });
  const outer = createGroup(root, { projectId, name: "Screen", x: 0, y: 0, w: 200, h: 200 }).group;
  assignToGroup(root, { projectId, elementIds: [red.id, green.id], groupId: outer.id });
  // fromElements now defaults the new group's parent to the members' common groupId.
  const inner = createGroup(root, { projectId, name: "Widget", fromElements: [red.id, green.id] }).group;
  return { projectId, red, green, outer, inner };
}

function pos(project, id) {
  const element = project.elements.find((e) => e.id === id);
  return { x: element.x, y: element.y };
}
function gpos(project, id) {
  const group = project.groups.find((g) => g.id === id);
  return { x: group.x, y: group.y };
}

// ---- createGroup nesting -------------------------------------------------------

test("createGroup fromElements defaults its parent to the members' common groupId", (t) => {
  tempProjects(t);
  const { projectId, outer, inner } = seedNested(ROOT);
  assert.equal(inner.parentId, outer.id, "widget nested inside the screen its members lived in");
  // The members are now inside the inner group, which is inside the outer group.
  const project = getProject(ROOT, projectId);
  assert.equal(project.groups.find((g) => g.id === inner.id).parentId, outer.id);
});

test("createGroup fromElements with mixed parents defaults to root", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Mixed" });
  const projectId = project.id;
  const a = addImage(ROOT, projectId, { name: "a", bytes: solidPng(4, 4) }).element;
  const b = addImage(ROOT, projectId, { name: "b", bytes: solidPng(4, 4) }).element;
  const outer = createGroup(ROOT, { projectId, name: "Outer", x: 0, y: 0, w: 100, h: 100 }).group;
  assignToGroup(ROOT, { projectId, elementIds: [a.id], groupId: outer.id }); // a in outer, b at root
  const grp = createGroup(ROOT, { projectId, name: "G", fromElements: [a.id, b.id] }).group;
  assert.equal(grp.parentId, undefined, "mixed member scopes -> new group at root");
});

test("createGroup parentId nests explicitly; an unknown parent is a loud error", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Explicit nest" });
  const projectId = project.id;
  const outer = createGroup(ROOT, { projectId, name: "Outer", x: 0, y: 0, w: 100, h: 100 }).group;
  const inner = createGroup(ROOT, { projectId, name: "Inner", x: 10, y: 10, w: 40, h: 40, parentId: outer.id }).group;
  assert.equal(inner.parentId, outer.id);
  assert.throws(
    () => createGroup(ROOT, { projectId, name: "Bad", x: 0, y: 0, w: 10, h: 10, parentId: "nope" }),
    /group not found/,
  );
});

// ---- reparentGroup -------------------------------------------------------------

test("reparentGroup moves a group under a parent and back to root", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Reparent" });
  const projectId = project.id;
  const outer = createGroup(ROOT, { projectId, name: "Outer", x: 0, y: 0, w: 100, h: 100 }).group;
  const widget = createGroup(ROOT, { projectId, name: "Widget", x: 5, y: 5, w: 30, h: 30 }).group;
  assert.equal(orderedChildren(getProject(ROOT, projectId), null).length, 2, "both at root first");

  reparentGroup(ROOT, { projectId, groupId: widget.id, parentId: outer.id });
  let stored = getProject(ROOT, projectId);
  assert.equal(stored.groups.find((g) => g.id === widget.id).parentId, outer.id, "nested under outer");
  assert.deepEqual(orderedChildren(stored, null).map((n) => n.id), [outer.id], "root now holds only outer");
  assert.deepEqual(orderedChildren(stored, outer.id).map((n) => n.id), [widget.id], "widget is outer's child");

  reparentGroup(ROOT, { projectId, groupId: widget.id, parentId: null });
  stored = getProject(ROOT, projectId);
  assert.equal(stored.groups.find((g) => g.id === widget.id).parentId, undefined, "back to root clears parentId");
});

test("reparentGroup with an index places the group among merged destination siblings", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Reparent index" });
  const projectId = project.id;
  const outer = createGroup(ROOT, { projectId, name: "Outer", x: 0, y: 0, w: 200, h: 200 }).group;
  const e1 = addImage(ROOT, projectId, { name: "e1", bytes: solidPng(4, 4) }).element;
  const e2 = addImage(ROOT, projectId, { name: "e2", bytes: solidPng(4, 4) }).element;
  assignToGroup(ROOT, { projectId, elementIds: [e1.id, e2.id], groupId: outer.id });
  const widget = createGroup(ROOT, { projectId, name: "Widget", x: 5, y: 5, w: 30, h: 30 }).group;

  // Move the widget under outer at the BACK (index 0) of its merged siblings [e1, e2].
  reparentGroup(ROOT, { projectId, groupId: widget.id, parentId: outer.id, index: 0 });
  const stored = getProject(ROOT, projectId);
  const label = (node) => (node.kind === "group" ? "W" : node.ref.name);
  assert.deepEqual(orderedChildren(stored, outer.id).map(label), ["W", "e1", "e2"], "widget placed at the back");
});

test("reparentGroup rejects a cycle (self + descendant) and an unknown parent, loudly", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Cycle" });
  const projectId = project.id;
  const outer = createGroup(ROOT, { projectId, name: "Outer", x: 0, y: 0, w: 100, h: 100 }).group;
  const inner = createGroup(ROOT, { projectId, name: "Inner", x: 5, y: 5, w: 40, h: 40, parentId: outer.id }).group;

  assert.throws(() => reparentGroup(ROOT, { projectId, groupId: outer.id, parentId: outer.id }), /cycle/, "self");
  assert.throws(() => reparentGroup(ROOT, { projectId, groupId: outer.id, parentId: inner.id }), /cycle/, "descendant");
  assert.throws(() => reparentGroup(ROOT, { projectId, groupId: outer.id, parentId: "nope" }), /group not found/);
  assert.throws(() => reparentGroup(ROOT, { projectId, groupId: "nope", parentId: null }), /group not found/);
  // Moving inner UNDER a fresh sibling is fine (not a cycle).
  const other = createGroup(ROOT, { projectId, name: "Other", x: 0, y: 0, w: 50, h: 50 }).group;
  reparentGroup(ROOT, { projectId, groupId: inner.id, parentId: other.id });
  assert.equal(getProject(ROOT, projectId).groups.find((g) => g.id === inner.id).parentId, other.id);
});

// ---- patchGroup move cascade over the full descendant closure ------------------

test("patchGroup move translates the 2-level closure (subgroup frame + its elements); one undo restores all", (t) => {
  tempProjects(t);
  const { projectId, red, green, outer, inner } = seedNested(ROOT);
  const innerBefore = gpos(getProject(ROOT, projectId), inner.id);

  patchGroup(ROOT, { projectId, groupId: outer.id, x: 100, y: 50 }); // outer 0,0 -> dx 100 dy 50
  const moved = getProject(ROOT, projectId);
  assert.deepEqual(gpos(moved, outer.id), { x: 100, y: 50 }, "outer frame moved");
  assert.deepEqual(gpos(moved, inner.id), { x: innerBefore.x + 100, y: innerBefore.y + 50 }, "nested frame moved with it");
  assert.deepEqual(pos(moved, red.id), { x: 110, y: 60 }, "nested element moved");
  assert.deepEqual(pos(moved, green.id), { x: 130, y: 70 }, "nested element moved");

  // ONE journal entry: a single undo restores the whole closure.
  const undone = undoOp(ROOT, { projectId }).project;
  assert.deepEqual(gpos(undone, inner.id), innerBefore, "undo restores the nested frame");
  assert.deepEqual(pos(undone, red.id), { x: 10, y: 10 });
  assert.deepEqual(pos(undone, green.id), { x: 30, y: 20 });
  const redone = redoOp(ROOT, { projectId }).project;
  assert.deepEqual(pos(redone, red.id), { x: 110, y: 60 });
});

// ---- deleteGroup 2-level subtree + the Ungroup one-level dissolve --------------

test("deleteGroup removes the full 2-level subtree in one entry; undo restores everything", (t) => {
  tempProjects(t);
  const { projectId, red, green, outer, inner } = seedNested(ROOT);
  const seqBefore = Number(getProject(ROOT, projectId).history_seq);

  const result = deleteGroup(ROOT, { projectId, groupId: outer.id });
  const after = getProject(ROOT, projectId);
  assert.equal(after.groups.length, 0, "outer + inner both gone");
  assert.equal(after.elements.length, 0, "nested elements gone");
  assert.equal(Number(after.history_seq), seqBefore + 1, "exactly one journal entry");
  assert.deepEqual(result.removedGroups.sort(), [outer.id, inner.id].sort());
  assert.deepEqual(result.removedElements.sort(), [red.id, green.id].sort());

  const restored = undoOp(ROOT, { projectId }).project;
  assert.equal(restored.groups.length, 2, "undo restores both groups");
  assert.equal(restored.elements.length, 2, "undo restores both elements");
  assert.equal(restored.groups.find((g) => g.id === inner.id).parentId, outer.id, "nesting restored");
});

test("Ungroup composition dissolves ONE level: children move up to the parent, subgroup deleted", (t) => {
  tempProjects(t);
  const { projectId, red, green, outer, inner } = seedNested(ROOT);
  // Ungroup(inner): its child elements move up to inner's parent (outer), then the now
  // empty inner is deleted. deleteGroup must NOT cascade to the already-moved children.
  assignToGroup(ROOT, { projectId, elementIds: [red.id, green.id], groupId: outer.id });
  deleteGroup(ROOT, { projectId, groupId: inner.id });
  const after = getProject(ROOT, projectId);
  assert.equal(after.groups.length, 1, "only outer remains");
  assert.equal(after.groups[0].id, outer.id);
  assert.equal(after.elements.length, 2, "children survived the ungroup (not to root, to the parent)");
  assert.equal(after.elements.every((e) => e.groupId === outer.id), true, "children now live in the parent scope");
});

// ---- mixed-sibling z-order feeds the render -----------------------------------

test("reorderNode reorders a subgroup against a sibling element within a parent scope", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Mixed order" });
  const projectId = project.id;
  const outer = createGroup(ROOT, { projectId, name: "Outer", x: 0, y: 0, w: 200, h: 200 }).group;
  const e = addImage(ROOT, projectId, { name: "e", bytes: solidPng(4, 4) }).element;
  assignToGroup(ROOT, { projectId, elementIds: [e.id], groupId: outer.id });
  const sub = createGroup(ROOT, { projectId, name: "Sub", x: 5, y: 5, w: 30, h: 30, parentId: outer.id }).group;
  const label = (node) => (node.kind === "group" ? "S" : node.ref.name);
  // Merged children of outer start as [e, S] (e added first). Move S to the back.
  reorderNode(ROOT, { projectId, nodeId: sub.id, index: 0 });
  assert.deepEqual(orderedChildren(getProject(ROOT, projectId), outer.id).map(label), ["S", "e"]);
});

// ---- recursive render ----------------------------------------------------------

test("renderGroup composites a nested subgroup background + element inside the parent (skips without Python)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Recursive BG" });
  const projectId = project.id;
  const red = addImage(REPO_ROOT, projectId, { name: "red", bytes: solidPng(4, 4, [220, 40, 40]) }).element;
  patchElement(REPO_ROOT, projectId, red.id, { x: 12, y: 12 });
  const outer = createGroup(REPO_ROOT, { projectId, name: "Outer", x: 0, y: 0, w: 40, h: 40 }).group;
  const inner = createGroup(REPO_ROOT, { projectId, name: "Inner", x: 10, y: 10, w: 20, h: 20, parentId: outer.id }).group;
  assignToGroup(REPO_ROOT, { projectId, elementIds: [red.id], groupId: inner.id });
  patchGroup(REPO_ROOT, { projectId, groupId: outer.id, background: { type: "color", color: "#112233" } });
  patchGroup(REPO_ROOT, { projectId, groupId: inner.id, background: { type: "color", color: "#445566" } });

  let result;
  try {
    result = await renderGroup(REPO_ROOT, { projectId, groupId: outer.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  assert.equal(result.members, 1, "one leaf image element in the subtree");
  const png = decodePng(readFileSync(result.path));
  assert.deepEqual([png.width, png.height], [40, 40]);
  assert.deepEqual(png.at(1, 1), [17, 34, 51, 255], "outer background at a corner");
  assert.deepEqual(png.at(11, 11), [68, 85, 102, 255], "nested background band inside outer");
  assert.deepEqual(png.at(13, 13), [220, 40, 40, 255], "nested element on top of both backgrounds");
});

test("renderGroup prunes a hidden nested subtree (skips without Python)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Hidden subtree" });
  const projectId = project.id;
  const red = addImage(REPO_ROOT, projectId, { name: "red", bytes: solidPng(6, 6, [220, 40, 40]) }).element;
  patchElement(REPO_ROOT, projectId, red.id, { x: 12, y: 12 });
  const outer = createGroup(REPO_ROOT, { projectId, name: "Outer", x: 0, y: 0, w: 40, h: 40 }).group;
  const inner = createGroup(REPO_ROOT, { projectId, name: "Inner", x: 10, y: 10, w: 20, h: 20, parentId: outer.id }).group;
  assignToGroup(REPO_ROOT, { projectId, elementIds: [red.id], groupId: inner.id });
  patchGroup(REPO_ROOT, { projectId, groupId: inner.id, visible: false }); // hide the nested subtree

  let result;
  try {
    result = await renderGroup(REPO_ROOT, { projectId, groupId: outer.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  assert.equal(result.members, 0, "hidden nested subtree pruned from the render");
  const png = decodePng(readFileSync(result.path));
  assert.equal(png.at(13, 13)[3], 0, "the hidden nested element is absent (transparent)");
});

// ---- exportProject: only top-level visible groups ------------------------------

test("exportProject renders only top-level groups; a nested group is not a separate screen (skips without Python)", async (t) => {
  tempProjects(t);
  const { projectId, outer } = seedNested(REPO_ROOT); // outer (top-level) + inner (nested)
  // T0332 B1: the export opt-in flag — seedNested's plain groups carry no screen by
  // default, so the outer screen must be ticked explicitly for this export to find it.
  patchGroup(REPO_ROOT, { projectId, groupId: outer.id, screen: true });
  let result;
  try {
    result = await exportProject(REPO_ROOT, { projectId });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  assert.equal(result.screens.length, 1, "only the top-level screen is exported");
  assert.equal(result.screens[0].name, "Screen");
});

// ---- v1 migration: groups with no parentId are all top-level -------------------

test("a v1-shaped project (no parentId) treats every group as a top-level screen", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "V1 shape" });
  const projectId = project.id;
  const a = addImage(REPO_ROOT, projectId, { name: "a", bytes: solidPng(6, 6, [10, 20, 30]) }).element;
  const b = addImage(REPO_ROOT, projectId, { name: "b", bytes: solidPng(6, 6, [40, 50, 60]) }).element;
  // Two groups created with explicit bounds carry no parentId (top-level by absence).
  const g1 = createGroup(REPO_ROOT, { projectId, name: "One", x: 0, y: 0, w: 30, h: 30 }).group;
  const g2 = createGroup(REPO_ROOT, { projectId, name: "Two", x: 40, y: 0, w: 30, h: 30 }).group;
  assignToGroup(REPO_ROOT, { projectId, elementIds: [a.id], groupId: g1.id });
  assignToGroup(REPO_ROOT, { projectId, elementIds: [b.id], groupId: g2.id });
  assert.equal(g1.parentId, undefined);
  assert.equal(g2.parentId, undefined);
  // T0332 B1: screen:true is orthogonal to top-level-ness — flag both so this test still
  // exercises its own point (parentId-less v1 groups both count as top-level candidates).
  patchGroup(REPO_ROOT, { projectId, groupId: g1.id, screen: true });
  patchGroup(REPO_ROOT, { projectId, groupId: g2.id, screen: true });

  let result;
  try {
    result = await exportProject(REPO_ROOT, { projectId });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  assert.equal(result.screens.length, 2, "both parentId-less groups export as top-level screens");
});
