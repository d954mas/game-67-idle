// Pure scene-tree math (tree.mjs). No store / Python / DOM — hand-written project
// fixtures only (nesting ops don't exist yet, so parentId is written by hand). Run:
//   node --test ai_studio/assets/canvas/tests/tree.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import {
  ancestorsOf,
  buildNodesSpec,
  childrenOf,
  descendantsOf,
  isNodeHidden,
  isNodeTransformed,
  nodeAABB,
  orderedChildren,
  rotatedCorners,
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

// ---- rotatedCorners / nodeAABB / isNodeTransformed (T0232 increment 3a) --------

test("rotatedCorners is the identity box when rotation is absent/0", () => {
  const node = { x: 10, y: 20, w: 30, h: 40 };
  assert.deepEqual(rotatedCorners(node), [
    { x: 10, y: 20 },
    { x: 40, y: 20 },
    { x: 40, y: 60 },
    { x: 10, y: 60 },
  ]);
  assert.deepEqual(rotatedCorners({ ...node, rotation: 0 }), rotatedCorners(node));
  const box = nodeAABB(node);
  assert.deepEqual(box, { minX: 10, minY: 20, maxX: 40, maxY: 60, x: 10, y: 20, w: 30, h: 40 });
});

test("rotatedCorners rotates 90 CW about the box center (matches the render parity contract)", () => {
  // A 24x16 box at (0,0), center (12,8). 90 CW: local (dx,dy) -> (-dy,dx).
  // TL local (-12,-8) -> (8,-12) -> world (12+8, 8-12) = (20,-4): the new TOP-RIGHT corner.
  const node = { x: 0, y: 0, w: 24, h: 16, rotation: 90 };
  const corners = rotatedCorners(node);
  const round = (p) => ({ x: Math.round(p.x), y: Math.round(p.y) });
  assert.deepEqual(round(corners[0]), { x: 20, y: -4 }, "TL -> new top-right");
  assert.deepEqual(round(corners[1]), { x: 20, y: 20 }, "TR -> new bottom-right");
  assert.deepEqual(round(corners[2]), { x: 4, y: 20 }, "BR -> new bottom-left");
  assert.deepEqual(round(corners[3]), { x: 4, y: -4 }, "BL -> new top-left");
  // Dims swap (24x16 -> 16x24), still centered on the SAME center (12,8).
  const box = nodeAABB(node);
  assert.equal(Math.round(box.w), 16);
  assert.equal(Math.round(box.h), 24);
  assert.equal(Math.round(box.x + box.w / 2), 12);
  assert.equal(Math.round(box.y + box.h / 2), 8);
});

test("nodeAABB wraps a rotated element's footprint so createGroup/fitGroup padding never clips it", () => {
  // A 45-degree rotation of a square box has a LARGER AABB than the unrotated box.
  const node = { x: 0, y: 0, w: 20, h: 20, rotation: 45 };
  const box = nodeAABB(node);
  const diag = 20 * Math.SQRT2;
  assert.ok(Math.abs(box.w - diag) < 1e-9, `expected diagonal AABB width ~${diag}, got ${box.w}`);
  assert.ok(Math.abs(box.h - diag) < 1e-9, `expected diagonal AABB height ~${diag}, got ${box.h}`);
  // Flip never changes the AABB (it mirrors pixels WITHIN the box, not the box itself).
  assert.deepEqual(nodeAABB({ x: 5, y: 5, w: 10, h: 6, flipH: true, flipV: true }), nodeAABB({ x: 5, y: 5, w: 10, h: 6 }));
});

// ---- buildNodesSpec / serializeNode (T0239-3 fix: ids are KEPT, not stripped) --------

test("buildNodesSpec KEEPS element.id and group.id (T0239-3 fix — was stripped before); still strips groupId/parentId/order", () => {
  const project = {
    elements: [{ id: "el_a", type: "image", groupId: "grp_outer", order: 7, name: "a" }],
    groups: [{ id: "grp_outer", parentId: null, order: 3, name: "Outer" }],
  };
  const spec = buildNodesSpec(project, ["grp_outer"]);
  assert.equal(spec.nodes.length, 1);
  const groupNode = spec.nodes[0];
  assert.equal(groupNode.kind, "group");
  // id KEPT (the fix); parentId/order (placement) still stripped.
  assert.equal(groupNode.group.id, "grp_outer");
  assert.equal("parentId" in groupNode.group, false);
  assert.equal("order" in groupNode.group, false);
  assert.equal(groupNode.group.name, "Outer");

  assert.equal(groupNode.children.length, 1);
  const elementNode = groupNode.children[0];
  // id KEPT; groupId/order (placement) still stripped.
  assert.equal(elementNode.element.id, "el_a");
  assert.equal("groupId" in elementNode.element, false);
  assert.equal("order" in elementNode.element, false);
  assert.equal(elementNode.element.name, "a");
});

test("buildNodesSpec keeps ids through a nested subtree (deep children carry their own original ids too)", () => {
  const project = {
    elements: [
      { id: "el_leaf", type: "image", groupId: "grp_inner" },
    ],
    groups: [
      { id: "grp_outer", parentId: null },
      { id: "grp_inner", parentId: "grp_outer" },
    ],
  };
  const spec = buildNodesSpec(project, ["grp_outer"]);
  const outerNode = spec.nodes[0];
  assert.equal(outerNode.group.id, "grp_outer");
  const innerNode = outerNode.children[0];
  assert.equal(innerNode.kind, "group");
  assert.equal(innerNode.group.id, "grp_inner");
  const leafNode = innerNode.children[0];
  assert.equal(leafNode.element.id, "el_leaf");
});

test("buildNodesSpec: a single plain element root also keeps its id", () => {
  const project = {
    elements: [{ id: "el_solo", type: "image", name: "solo" }],
    groups: [],
  };
  const spec = buildNodesSpec(project, ["el_solo"]);
  assert.equal(spec.nodes[0].element.id, "el_solo");
});

test("isNodeTransformed is true for a nonzero rotation or either flip flag, false for a plain box", () => {
  assert.equal(isNodeTransformed({ x: 0, y: 0, w: 1, h: 1 }), false);
  assert.equal(isNodeTransformed({ x: 0, y: 0, w: 1, h: 1, rotation: 0 }), false);
  assert.equal(isNodeTransformed({ x: 0, y: 0, w: 1, h: 1, flipH: false, flipV: false }), false);
  assert.equal(isNodeTransformed({ x: 0, y: 0, w: 1, h: 1, rotation: 90 }), true);
  assert.equal(isNodeTransformed({ x: 0, y: 0, w: 1, h: 1, flipH: true }), true);
  assert.equal(isNodeTransformed({ x: 0, y: 0, w: 1, h: 1, flipV: true }), true);
  assert.equal(isNodeTransformed(null), false);
});
