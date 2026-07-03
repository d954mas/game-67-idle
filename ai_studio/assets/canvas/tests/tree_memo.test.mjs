// T0253 F3: tree.mjs memoizes its derived structure (groupMap / per-scope
// childrenOf+orderedChildren / v1 element index / group adjacency) per PROJECT OBJECT
// IDENTITY (WeakMap), so render()/hitElement()/layersSignature() stop re-deriving the
// whole scene tree from scratch on every call. This file pins the memoization contract
// itself — correctness under repeat calls, correct invalidation on a NEW project object,
// correctness under the one in-place-mutation pattern that exists today (a `visible`
// toggle on a held group reference), and the superlinearity fix (subtreeGroupIds'
// O(G^2) group scan + orderedChildren's per-scope O(E) indexById rebuild). See tree.mjs's
// own "per-project derived-structure cache" header for the full node+site safety audit.
// Run:
//   node --test ai_studio/assets/canvas/tests/tree_memo.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import {
  ancestorsOf,
  buildNodesSpec,
  childrenOf,
  descendantsOf,
  frontOrder,
  isNodeHidden,
  nodeScope,
  orderedChildren,
  wouldCycle,
} from "../tree.mjs";

// A nested fixture spanning both z-order regimes: root is v1 (root_a has no `order`,
// so the whole root scope falls back to array order); g1's OWN scope (e1/e2/g2) is
// fully explicit (every sibling — including the nested group g2 — carries `order`,
// per the "scopes never go half-explicit" rule: ONE order-less sibling would drop the
// whole scope back to v1). g2 has a single v1 member (its own scope's regime is
// independent of its parent's).
function nestedFixture() {
  return {
    elements: [
      { id: "root_a", type: "image" }, // v1 root member (no order)
      { id: "e1", type: "image", groupId: "g1", order: 5 },
      { id: "e2", type: "image", groupId: "g1", order: 1 },
      { id: "leaf", type: "image", groupId: "g2" }, // g2's only member
    ],
    groups: [
      { id: "g1", parentId: null, order: 0, visible: true },
      { id: "g2", parentId: "g1", order: 10, visible: true },
    ],
  };
}

function deepClone(value) {
  return JSON.parse(JSON.stringify(value));
}

// Every exported helper this cache touches, run against the same fixture — used both to
// assert identical-across-repeat-calls and identical-to-a-fresh-object's-first-call.
function snapshotAllHelpers(project) {
  return {
    childrenRoot: childrenOf(project, null),
    childrenG1: childrenOf(project, "g1"),
    childrenG2: childrenOf(project, "g2"),
    orderedRoot: orderedChildren(project, null).map((n) => `${n.kind}:${n.id}`),
    orderedG1: orderedChildren(project, "g1").map((n) => `${n.kind}:${n.id}`),
    orderedG2: orderedChildren(project, "g2").map((n) => n.id),
    descendantsG1: descendantsOf(project, "g1"),
    ancestorsLeaf: ancestorsOf(project, project.elements.find((e) => e.id === "leaf")).map((g) => g.id),
    isHiddenLeaf: isNodeHidden(project, project.elements.find((e) => e.id === "leaf")),
    scopeE1: nodeScope(project, project.elements.find((e) => e.id === "e1")),
    frontOrderG1: frontOrder(project, "g1"),
    wouldCycleG2IntoG1: wouldCycle(project, "g2", "g1"),
    spec: buildNodesSpec(project, ["g1"]),
  };
}

test("memoized results are identical to the fresh (first-call) computation, for every exported helper", () => {
  const project = nestedFixture();
  const first = snapshotAllHelpers(project); // builds the cache
  const second = snapshotAllHelpers(project); // reads it back
  assert.deepEqual(second, first);
  // A third call from a totally independent angle (single-helper) still agrees.
  assert.deepEqual(orderedChildren(project, "g1").map((n) => n.id), ["e2", "e1", "g2"]);
});

test("a fresh project object (even a structurally-identical clone) is a cache miss, not a stale hit", () => {
  const projectA = nestedFixture();
  const resultA = orderedChildren(projectA, "g1").map((n) => n.id); // warms A's cache

  // A same-content clone (fresh identity) computes the identical result — not served
  // from A's cache (proves the key really is object identity, not e.g. some content
  // hash that would coincidentally collide).
  const projectSame = deepClone(projectA);
  assert.deepEqual(orderedChildren(projectSame, "g1").map((n) => n.id), resultA);

  // A clone mutated BEFORE its first tree.mjs read (the real shape every caller uses —
  // ops.mjs's fresh JSON.parse, the site's fresh server response — a new object arrives
  // already in its final state; this is NOT the "mutate a held reference mid-use"
  // pattern, which the cache does not support) computes its OWN, different result.
  const projectMutated = deepClone(projectA);
  projectMutated.elements.find((e) => e.id === "e2").groupId = null; // mutate before first use
  const resultMutated = orderedChildren(projectMutated, "g1").map((n) => n.id); // first (only) read
  assert.notDeepEqual(resultMutated, resultA, "the fresh object's own content is reflected");
  assert.deepEqual(resultMutated, ["e1", "g2"]);

  // A's cache stays completely unaffected by anything done to either clone.
  assert.deepEqual(orderedChildren(projectA, "g1").map((n) => n.id), resultA, "A never sees a clone's mutation");
});

test("orderedChildren/childrenOf/descendantsOf/ancestorsOf differ between two distinct project objects with different structure", () => {
  const base = nestedFixture();
  const grown = deepClone(base);
  grown.elements.push({ id: "e3", type: "image", groupId: "g1", order: 20 }); // > g2's order (10), so it lands last

  assert.deepEqual(orderedChildren(base, "g1").map((n) => n.id), ["e2", "e1", "g2"]);
  assert.deepEqual(orderedChildren(grown, "g1").map((n) => n.id), ["e2", "e1", "g2", "e3"], "the new object reflects its own extra element");
  assert.equal(childrenOf(base, "g1").elements.length, 2);
  assert.equal(childrenOf(grown, "g1").elements.length, 3);
});

test("memoized cache still observes an in-place `visible` toggle on a held group reference (the one real mutation pattern)", () => {
  // Mirrors tree.test.mjs's "isNodeHidden cascades" case, but explicitly documents the
  // memoization safety contract: the cache holds live OBJECT REFERENCES (groupMap,
  // ancestor chains), never a memoized isNodeHidden boolean, so an in-place field
  // mutation on an object already read into the cache is still seen immediately.
  const project = {
    elements: [{ id: "leaf", type: "image", groupId: "inner" }],
    groups: [
      { id: "outer", parentId: null, visible: false },
      { id: "inner", parentId: "outer" },
    ],
  };
  const leaf = project.elements[0];
  // Warm the cache (groupMap, ancestor chain) BEFORE the mutation.
  assert.equal(isNodeHidden(project, leaf), true, "hidden via the (cached) grandparent group");
  assert.deepEqual(ancestorsOf(project, leaf).map((g) => g.id), ["inner", "outer"]);
  // Mutate the SAME live group object the cache already resolved.
  project.groups[0].visible = true;
  assert.equal(isNodeHidden(project, leaf), false, "cache still reflects the live visible flag, not a stale boolean");
});

test("wouldCycle / descendantsOf stay correct through the adjacency-indexed subtreeGroupIds on a deeper nest", () => {
  const project = {
    elements: [
      { id: "a", groupId: "l3" },
      { id: "b", groupId: "l1" },
    ],
    groups: [
      { id: "l1", parentId: null },
      { id: "l2", parentId: "l1" },
      { id: "l3", parentId: "l2" },
      { id: "other", parentId: null },
    ],
  };
  assert.equal(wouldCycle(project, "l1", "l3"), true, "l3 is a descendant of l1 (3 levels down)");
  assert.equal(wouldCycle(project, "l1", "other"), false);
  const descendants = descendantsOf(project, "l1");
  assert.deepEqual(descendants.groups.map((g) => g.id).sort(), ["l2", "l3"]);
  assert.deepEqual(descendants.elements.map((e) => e.id).sort(), ["a", "b"]);
});

// ---- superlinearity pin (T0253 F3: subtreeGroupIds O(G^2) + per-scope O(E) indexById) --

// 80 groups (1/3 top-level "screens", the rest nested one level under a screen) + 1000
// v1-style elements (no `order` — every orderedChildren call takes the indexById
// fallback branch, the worst case the review measured) spread across all groups + root.
function bigSyntheticProject(numElements, numGroups) {
  const groups = [];
  const screens = Math.max(1, Math.ceil(numGroups / 3));
  for (let i = 0; i < numGroups; i += 1) {
    const id = `g${i}`;
    if (i < screens) groups.push({ id, name: `Screen ${i}`, x: 0, y: 0, w: 800, h: 600, visible: true });
    else groups.push({ id, name: `Widget ${i}`, x: 0, y: 0, w: 100, h: 100, visible: true, parentId: `g${i % screens}` });
  }
  const elements = [];
  for (let i = 0; i < numElements; i += 1) {
    const element = { id: `e${i}`, type: "image", src: `files/e${i}.png`, x: 0, y: 0, w: 32, h: 32, visible: true };
    if (i % 5 !== 0) element.groupId = `g${i % numGroups}`; // ~80% grouped, 20% at root
    elements.push(element);
  }
  return { schema: "ai_studio.canvas.project.v1", elements, groups };
}

test("1000 elements / 80 groups: a full orderedChildren walk over every scope stays fast, repeated 100x (pins the O(G^2)/O(E)-per-scope fix)", () => {
  const project = bigSyntheticProject(1000, 80);
  const scopes = [null, ...project.groups.map((g) => g.id)];

  const start = performance.now();
  for (let iteration = 0; iteration < 100; iteration += 1) {
    for (const scope of scopes) orderedChildren(project, scope);
  }
  const elapsed = performance.now() - start;

  // Generous bound (not a tight perf assert — CI boxes vary): the point is pinning the
  // SUPERLINEAR blowup, not chasing precision. Pre-fix this shape (v1 fallback,
  // 80 groups) measured single-digit-to-tens of ms PER WALK; 100 walks would blow well
  // past this bound. Post-fix it's sub-millisecond per walk after the first (cache-fill)
  // pass, so 100 walks finishes in low tens of ms.
  assert.ok(elapsed < 500, `expected 100x full-project orderedChildren walk under 500ms, took ${elapsed.toFixed(1)}ms`);
});

test("bigSyntheticProject fixture itself is structurally sane (sanity check for the perf test above)", () => {
  const project = bigSyntheticProject(1000, 80);
  const root = childrenOf(project, null);
  // Every element lands somewhere: either at root or inside a resolvable group.
  const total = project.groups.reduce((sum, g) => sum + childrenOf(project, g.id).elements.length, 0) + root.elements.length;
  assert.equal(total, 1000);
});
