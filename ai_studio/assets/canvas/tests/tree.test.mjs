// Pure scene-tree math (tree.mjs). No store / Python / DOM — hand-written project
// fixtures only (nesting ops don't exist yet, so parentId is written by hand). Run:
//   node --test ai_studio/assets/canvas/tests/tree.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import {
  ancestorsOf,
  childrenOf,
  descendantsOf,
  isNodeHidden,
  orderedChildren,
  wouldCycle,
} from "../tree.mjs";

// A v1-shaped project: flat ungrouped elements, no order/parentId fields at all.
function v1Project() {
  return {
    elements: [
      { id: "a", type: "image" },
      { id: "b", type: "image" },
      { id: "c", type: "image" },
    ],
    groups: [],
  };
}

test("orderedChildren fallback == elements[] order for a v1 project", () => {
  const project = v1Project();
  const ordered = orderedChildren(project, null);
  assert.deepEqual(ordered.map((node) => node.id), ["a", "b", "c"]);
  assert.ok(ordered.every((node) => node.kind === "element"));
});

test("canvas identity: computed root order equals elements[] order", () => {
  const project = v1Project();
  assert.deepEqual(
    orderedChildren(project, null).map((node) => node.id),
    project.elements.map((element) => element.id),
  );
});

test("v1 fallback anchors a group at its backmost member and interleaves with root elements", () => {
  // elements[] order: a(0) in group g, b(1) at root, c(2) in group g.
  const project = {
    elements: [
      { id: "a", type: "image", groupId: "g" },
      { id: "b", type: "image" },
      { id: "c", type: "image", groupId: "g" },
    ],
    groups: [{ id: "g", parentId: null }],
  };
  // Root: group g anchored at min member index 0, root element b at index 1 -> g, b.
  assert.deepEqual(
    orderedChildren(project, null).map((node) => `${node.kind}:${node.id}`),
    ["group:g", "element:b"],
  );
  // Inside g: a then c (elements[] order preserved within a scope).
  assert.deepEqual(orderedChildren(project, "g").map((node) => node.id), ["a", "c"]);
});

test("explicit numeric order sorts merged element+group siblings", () => {
  const project = {
    elements: [
      { id: "e1", type: "image", order: 30 },
      { id: "e2", type: "image", order: 10 },
    ],
    groups: [{ id: "g1", parentId: null, order: 20 }],
  };
  assert.deepEqual(orderedChildren(project, null).map((node) => node.id), ["e2", "g1", "e1"]);
});

test("a partial order set falls back (order used only when EVERY sibling has it)", () => {
  const project = {
    elements: [
      { id: "e1", type: "image", order: 5 }, // has order
      { id: "e2", type: "image" }, // no order -> whole scope falls back to array index
    ],
    groups: [],
  };
  assert.deepEqual(orderedChildren(project, null).map((node) => node.id), ["e1", "e2"]);
});

test("empty group sorts to front in the v1 fallback", () => {
  const project = {
    elements: [{ id: "a", type: "image" }],
    groups: [{ id: "empty", parentId: null }],
  };
  // Root element a (key 0) then the empty group (no members -> front / on top).
  assert.deepEqual(
    orderedChildren(project, null).map((node) => `${node.kind}:${node.id}`),
    ["element:a", "group:empty"],
  );
});

test("childrenOf splits elements and groups by resolved scope", () => {
  const project = {
    elements: [
      { id: "a", type: "image", groupId: "g" },
      { id: "b", type: "image" },
    ],
    groups: [
      { id: "g", parentId: null },
      { id: "sub", parentId: "g" },
    ],
  };
  const root = childrenOf(project, null);
  assert.deepEqual(root.elements.map((element) => element.id), ["b"]);
  assert.deepEqual(root.groups.map((group) => group.id), ["g"]);
  const inside = childrenOf(project, "g");
  assert.deepEqual(inside.elements.map((element) => element.id), ["a"]);
  assert.deepEqual(inside.groups.map((group) => group.id), ["sub"]);
});

test("a dangling groupId resolves to root (element still renders, never vanishes)", () => {
  const project = {
    elements: [{ id: "orphan", type: "image", groupId: "gone" }],
    groups: [],
  };
  assert.deepEqual(childrenOf(project, null).elements.map((element) => element.id), ["orphan"]);
});

test("isNodeHidden cascades through a nested parent chain", () => {
  const project = {
    elements: [{ id: "leaf", type: "image", groupId: "inner" }],
    groups: [
      { id: "outer", parentId: null, visible: false },
      { id: "inner", parentId: "outer" },
    ],
  };
  const leaf = project.elements[0];
  assert.equal(isNodeHidden(project, leaf), true, "hidden via grandparent group");
  assert.equal(isNodeHidden(project, project.groups[1]), true, "inner hidden via outer");
  project.groups[0].visible = true;
  assert.equal(isNodeHidden(project, leaf), false, "visible once the ancestor is shown");
  assert.equal(isNodeHidden(project, { id: "x", visible: false }), true, "own flag wins");
});

test("ancestorsOf returns the group chain nearest-first", () => {
  const project = {
    elements: [{ id: "leaf", type: "image", groupId: "inner" }],
    groups: [
      { id: "outer", parentId: null },
      { id: "inner", parentId: "outer" },
    ],
  };
  assert.deepEqual(ancestorsOf(project, project.elements[0]).map((group) => group.id), ["inner", "outer"]);
});

test("descendantsOf collects the whole subtree (nested groups + elements)", () => {
  const project = {
    elements: [
      { id: "a", type: "image", groupId: "inner" },
      { id: "b", type: "image", groupId: "outer" },
      { id: "c", type: "image" },
    ],
    groups: [
      { id: "outer", parentId: null },
      { id: "inner", parentId: "outer" },
    ],
  };
  const descendants = descendantsOf(project, "outer");
  assert.deepEqual(descendants.groups.map((group) => group.id), ["inner"]);
  assert.deepEqual(descendants.elements.map((element) => element.id).sort(), ["a", "b"]);
});

test("wouldCycle is true for self and any descendant, false otherwise", () => {
  const project = {
    elements: [],
    groups: [
      { id: "outer", parentId: null },
      { id: "inner", parentId: "outer" },
      { id: "other", parentId: null },
    ],
  };
  assert.equal(wouldCycle(project, "outer", "outer"), true, "self");
  assert.equal(wouldCycle(project, "outer", "inner"), true, "descendant");
  assert.equal(wouldCycle(project, "outer", "other"), false, "unrelated");
  assert.equal(wouldCycle(project, "outer", null), false, "root");
  assert.equal(wouldCycle(project, "inner", "outer"), false, "under an ancestor is fine");
});

test("a corrupt parent cycle does not hang isNodeHidden / orderedChildren / wouldCycle", () => {
  const project = {
    elements: [{ id: "leaf", type: "image", groupId: "g1" }],
    groups: [
      { id: "g1", parentId: "g2" },
      { id: "g2", parentId: "g1" },
    ],
  };
  // All must terminate (cycle-capped) and not throw.
  assert.equal(typeof isNodeHidden(project, project.elements[0]), "boolean");
  assert.deepEqual(orderedChildren(project, "g1").map((node) => node.id).sort(), ["g2", "leaf"]);
  assert.equal(wouldCycle(project, "g1", "g2"), true, "g2 is in g1's (cyclic) subtree");
});
