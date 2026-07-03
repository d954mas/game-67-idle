// T0244 increment 1 -- pure smart-guide/snap math (snap.mjs). No store / DOM -- hand-written
// plain project fixtures only, mirroring tree.test.mjs / viewport_drag.test.mjs. Run:
//   node --test ai_studio/assets/canvas/tests/snap.test.mjs
import { strict as assert } from "node:assert";
import test from "node:test";
import { collectSnapCandidates, SNAP_SCREEN_PX, snapDelta } from "../site/snap.mjs";
import { unionBBox } from "../tree.mjs";

// ---- collectSnapCandidates ------------------------------------------------------

test("collectSnapCandidates: a single root sibling emits exactly 3 vertical + 3 horizontal lines at left/hcenter/right/top/vcenter/bottom", () => {
  const project = {
    elements: [
      { id: "A", x: 0, y: 0, w: 10, h: 10 },
      { id: "B", x: 100, y: 50, w: 20, h: 8 },
    ],
    groups: [],
  };
  const { vertical, horizontal } = collectSnapCandidates(project, ["A"]);
  assert.deepEqual(vertical, [
    { pos: 100, min: 50, max: 58 },
    { pos: 110, min: 50, max: 58 },
    { pos: 120, min: 50, max: 58 },
  ]);
  assert.deepEqual(horizontal, [
    { pos: 50, min: 100, max: 120 },
    { pos: 54, min: 100, max: 120 },
    { pos: 58, min: 100, max: 120 },
  ]);
});

test("collectSnapCandidates: a dragged id and every descendant of a dragged group are absent from candidates", () => {
  const project = {
    elements: [
      { id: "c1", x: 5, y: 5, w: 2, h: 2, groupId: "G" },
      { id: "c2", x: 8, y: 8, w: 2, h: 2, groupId: "G" },
      { id: "B", x: 100, y: 0, w: 10, h: 10 },
    ],
    groups: [{ id: "G", x: 0, y: 0, w: 20, h: 20 }],
  };
  const result = collectSnapCandidates(project, ["G"]);
  // Only B (root sibling) survives: G itself is the dragged id, c1/c2 are its descendants.
  assert.deepEqual(result, {
    vertical: [
      { pos: 100, min: 0, max: 10 },
      { pos: 105, min: 0, max: 10 },
      { pos: 110, min: 0, max: 10 },
    ],
    horizontal: [
      { pos: 0, min: 100, max: 110 },
      { pos: 5, min: 100, max: 110 },
      { pos: 10, min: 100, max: 110 },
    ],
  });
});

test("collectSnapCandidates: a directly-hidden root sibling is excluded", () => {
  const project = {
    elements: [
      { id: "A", x: 0, y: 0, w: 10, h: 10 },
      { id: "B", x: 50, y: 0, w: 10, h: 10, visible: false },
    ],
    groups: [],
  };
  assert.deepEqual(collectSnapCandidates(project, ["A"]), { vertical: [], horizontal: [] });
});

test("collectSnapCandidates: a node under a hidden ancestor group is excluded, and the hidden group contributes no frame", () => {
  const project = {
    elements: [
      { id: "In", x: 0, y: 0, w: 10, h: 10, groupId: "H" },
      { id: "D", x: 20, y: 0, w: 10, h: 10, groupId: "H" }, // not itself hidden; its ancestor H is
    ],
    groups: [{ id: "H", x: 0, y: 0, w: 40, h: 20, visible: false }],
  };
  // In is dragged; D would otherwise be a same-scope sibling but is hidden via ancestor H;
  // H itself is hidden so it contributes no frame lines either.
  assert.deepEqual(collectSnapCandidates(project, ["In"]), { vertical: [], horizontal: [] });
});

test("collectSnapCandidates: dragging inside a group scopes candidates to that group's children + its own frame, excluding outside nodes", () => {
  const project = {
    elements: [
      { id: "R0", x: 0, y: 0, w: 10, h: 10 }, // root, outside G
      { id: "E1", x: 110, y: 100, w: 10, h: 10, groupId: "G" }, // dragged, inside G
      { id: "E2", x: 130, y: 100, w: 10, h: 10, groupId: "G" }, // sibling, inside G
    ],
    groups: [{ id: "G", x: 100, y: 100, w: 50, h: 30 }],
  };
  const { vertical, horizontal } = collectSnapCandidates(project, ["E1"]);
  // 3 lines from sibling E2 + 3 lines from G's own frame; nothing from R0 (out of scope).
  assert.deepEqual(vertical, [
    { pos: 130, min: 100, max: 110 },
    { pos: 135, min: 100, max: 110 },
    { pos: 140, min: 100, max: 110 },
    { pos: 100, min: 100, max: 130 },
    { pos: 125, min: 100, max: 130 },
    { pos: 150, min: 100, max: 130 },
  ]);
  assert.deepEqual(horizontal, [
    { pos: 100, min: 130, max: 140 },
    { pos: 105, min: 130, max: 140 },
    { pos: 110, min: 130, max: 140 },
    { pos: 100, min: 100, max: 150 },
    { pos: 115, min: 100, max: 150 },
    { pos: 130, min: 100, max: 150 },
  ]);
});

test("collectSnapCandidates: dragging a root node sees root siblings (including a root-level group's own box) but adds NO frame line (no parent)", () => {
  const project = {
    elements: [
      { id: "R0", x: 0, y: 0, w: 10, h: 10 }, // dragged
      { id: "R1", x: 20, y: 0, w: 10, h: 10 }, // root sibling
      { id: "E1", x: 110, y: 100, w: 10, h: 10, groupId: "G" },
      { id: "E2", x: 130, y: 100, w: 10, h: 10, groupId: "G" },
    ],
    groups: [{ id: "G", x: 100, y: 100, w: 50, h: 30 }],
  };
  const { vertical, horizontal } = collectSnapCandidates(project, ["R0"]);
  // R1's 3 lines + G's 3 lines as an ORDINARY sibling box (not a bonus "frame" -- there is
  // no second, extra contribution for G at root scope since scope === null skips the frame
  // step entirely). Nothing from E1/E2 (different scope).
  assert.deepEqual(vertical, [
    { pos: 20, min: 0, max: 10 },
    { pos: 25, min: 0, max: 10 },
    { pos: 30, min: 0, max: 10 },
    { pos: 100, min: 100, max: 130 },
    { pos: 125, min: 100, max: 130 },
    { pos: 150, min: 100, max: 130 },
  ]);
  assert.equal(vertical.length, 6);
  assert.equal(horizontal.length, 6);
});

// ---- snapDelta --------------------------------------------------------------

test("snapDelta: left / hcenter / right each detect their own probe on the vertical axis", () => {
  const dragBBox = { x: 0, y: 0, w: 10, h: 10 }; // minX=0, cx=5, maxX=10
  const tol = 1.5;

  const left = snapDelta(dragBBox, { vertical: [{ pos: 1, min: 0, max: 10 }], horizontal: [] }, tol);
  assert.equal(left.dx, 1);
  assert.equal(left.dy, 0);
  assert.deepEqual(left.guides, [{ axis: "x", pos: 1, min: 0, max: 10 }]);

  const hcenter = snapDelta(dragBBox, { vertical: [{ pos: 6, min: 0, max: 10 }], horizontal: [] }, tol);
  assert.equal(hcenter.dx, 1); // 6 - cx(5)

  const right = snapDelta(dragBBox, { vertical: [{ pos: 11, min: 0, max: 10 }], horizontal: [] }, tol);
  assert.equal(right.dx, 1); // 11 - maxX(10)
});

test("snapDelta: top / vcenter / bottom each detect their own probe on the horizontal axis", () => {
  const dragBBox = { x: 0, y: 0, w: 10, h: 10 }; // minY=0, cy=5, maxY=10
  const tol = 1.5;

  const top = snapDelta(dragBBox, { vertical: [], horizontal: [{ pos: 1, min: 0, max: 10 }] }, tol);
  assert.equal(top.dy, 1);
  assert.deepEqual(top.guides, [{ axis: "y", pos: 1, min: 0, max: 10 }]);

  const vcenter = snapDelta(dragBBox, { vertical: [], horizontal: [{ pos: 6, min: 0, max: 10 }] }, tol);
  assert.equal(vcenter.dy, 1); // 6 - cy(5)

  const bottom = snapDelta(dragBBox, { vertical: [], horizontal: [{ pos: 11, min: 0, max: 10 }] }, tol);
  assert.equal(bottom.dy, 1); // 11 - maxY(10)
});

test("snapDelta: x and y snap independently -- one axis matches, the other does not", () => {
  const dragBBox = { x: 0, y: 0, w: 10, h: 10 };
  const candidates = {
    vertical: [{ pos: 1, min: 0, max: 10 }], // within tol of minX
    horizontal: [{ pos: 100, min: 0, max: 10 }], // far outside tol
  };
  const result = snapDelta(dragBBox, candidates, 2);
  assert.deepEqual(result, { dx: 1, dy: 0, guides: [{ axis: "x", pos: 1, min: 0, max: 10 }] });
});

test("snapDelta: nearest candidate wins among 2+ in-tolerance lines", () => {
  // w/h huge so cx/maxX probes are far away and only minX(0) is in play; two lines both
  // within tolerance of minX, but pos:3 is nearer than pos:-4.
  const dragBBox = { x: 0, y: 0, w: 1000, h: 1000 };
  const candidates = {
    vertical: [
      { pos: -4, min: 0, max: 1000 },
      { pos: 3, min: 0, max: 1000 },
    ],
    horizontal: [],
  };
  const result = snapDelta(dragBBox, candidates, 5);
  assert.equal(result.dx, 3);
});

test("snapDelta: tolerance boundary -- delta == tol snaps, delta > tol does not", () => {
  const dragBBox = { x: 0, y: 0, w: 1000, h: 1000 };
  const atBoundary = snapDelta(dragBBox, { vertical: [{ pos: 5, min: 0, max: 1000 }], horizontal: [] }, 5);
  assert.equal(atBoundary.dx, 5, "delta exactly == tolerance snaps");

  const overBoundary = snapDelta(dragBBox, { vertical: [{ pos: 5.0001, min: 0, max: 1000 }], horizontal: [] }, 5);
  assert.deepEqual(overBoundary, { dx: 0, dy: 0, guides: [] }, "delta > tolerance does not snap");
});

test("snapDelta: an already-aligned probe (delta 0) still emits a guide", () => {
  const dragBBox = { x: 0, y: 0, w: 10, h: 10 };
  const result = snapDelta(dragBBox, { vertical: [{ pos: 0, min: 0, max: 10 }], horizontal: [] }, 2);
  assert.equal(result.dx, 0);
  assert.deepEqual(result.guides, [{ axis: "x", pos: 0, min: 0, max: 10 }]);
});

test("snapDelta: no candidates on either axis -> {dx:0, dy:0, guides:[]}", () => {
  const dragBBox = { x: 0, y: 0, w: 10, h: 10 };
  assert.deepEqual(snapDelta(dragBBox, { vertical: [], horizontal: [] }, 6), { dx: 0, dy: 0, guides: [] });
  // Also robust to a candidates object missing the arrays entirely (an empty scope's result).
  assert.deepEqual(snapDelta(dragBBox, {}, 6), { dx: 0, dy: 0, guides: [] });
});

test("snapDelta: a multi-node selection's UNION bbox snaps by its outer edges/center, not by either member's own edges", () => {
  const m1 = { x: 0, y: 0, w: 10, h: 10 };
  const m2 = { x: 50, y: 0, w: 10, h: 10 };
  const union = unionBBox([m1, m2]); // minX=0, maxX=60, cx=30 -- neither member's own cx (5, 55)
  const dragBBox = { x: union.minX, y: union.minY, w: union.w, h: union.h };
  const candidates = { vertical: [{ pos: 32, min: 0, max: 10 }], horizontal: [] };
  const result = snapDelta(dragBBox, candidates, 5);
  assert.equal(result.dx, 2, "32 snaps to the UNION center (30), not to either member's own center");
});

test("snapDelta: guide extents union the source line's own extent with the SNAPPED dragged bbox's perpendicular extent (both axes resolved first)", () => {
  const dragBBox = { x: 0, y: 0, w: 10, h: 20 }; // minX=0,maxX=10 ; minY=0,maxY=20
  const candidates = {
    vertical: [{ pos: 1, min: 100, max: 150 }], // matches minX probe (dx=1); source Y-extent 100-150
    horizontal: [{ pos: 23, min: 0, max: 10 }], // matches maxY probe (dy=3); source X-extent 0-10
  };
  const result = snapDelta(dragBBox, candidates, 5);
  assert.equal(result.dx, 1);
  assert.equal(result.dy, 3); // 23 - maxY(20)
  const byAxis = Object.fromEntries(result.guides.map((g) => [g.axis, g]));
  // x-guide's perpendicular (Y) span unions the source's [100,150] with the SNAPPED dragged
  // Y extent [minY+dy, maxY+dy] = [3, 23] -> [min(100,3), max(150,23)] = [3, 150].
  assert.deepEqual(byAxis.x, { axis: "x", pos: 1, min: 3, max: 150 });
  // y-guide's perpendicular (X) span unions the source's [0,10] with the SNAPPED dragged X
  // extent [minX+dx, maxX+dx] = [1, 11] -> [min(0,1), max(10,11)] = [0, 11].
  assert.deepEqual(byAxis.y, { axis: "y", pos: 23, min: 0, max: 11 });
});

test("snapDelta: zoom tolerance flip -- same geometry snaps at a generous (zoomed-out) tolerance but not at a tight (zoomed-in) one", () => {
  const dragBBox = { x: 0, y: 0, w: 10, h: 10 }; // maxX = 10
  const candidates = { vertical: [{ pos: 20, min: 0, max: 10 }], horizontal: [] }; // distance 10 from maxX
  const tightWorld = SNAP_SCREEN_PX / 12; // max zoom-in (scale clamp 12) -> 0.5px world tolerance
  const looseWorld = SNAP_SCREEN_PX / 0.05; // max zoom-out (scale clamp 0.05) -> 120px world tolerance

  assert.deepEqual(snapDelta(dragBBox, candidates, tightWorld), { dx: 0, dy: 0, guides: [] });
  const loose = snapDelta(dragBBox, candidates, looseWorld);
  assert.equal(loose.dx, 10);
});
