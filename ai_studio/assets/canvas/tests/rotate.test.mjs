// T0232 increment 3b -- the interactive rotate handle + rotated hit-test/scale (deferred out
// of increment 3a's data+render+parity packet; see README "Rotation & flip"). Pure math only
// (site/viewport.mjs's angleFromCenter/rotationUpVector/rotationFromDrag/pointInRotatedBox/
// rotatedHandlePoints/resizeRotatedBox): the drag lifecycle (screen-space hit-test, live
// in-memory mutation, one-commit patchElement) lives in site/workspace.js, which imports
// app.js (DOM) and so is verified on the page, not here (same split scale_handles.test.mjs/
// viewport_drag.test.mjs already use for their own drag math). Run:
//   node --test ai_studio/assets/canvas/tests/rotate.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import {
  angleFromCenter,
  pointInRotatedBox,
  resizeBox,
  resizeRotatedBox,
  rotatedHandlePoints,
  rotationFromDrag,
  rotationUpVector,
  SCALE_HANDLES,
} from "../site/viewport.mjs";
import { rotatedCorners } from "../tree.mjs";

const EPS = 1e-6;
function closeTo(actual, expected, msg) {
  assert.ok(Math.abs(actual - expected) < EPS, `${msg}: expected ~${expected}, got ${actual}`);
}

function handle(key) {
  const found = SCALE_HANDLES.find((item) => item.key === key);
  assert.ok(found, `no such handle: ${key}`);
  return found;
}

// ---- angleFromCenter -----------------------------------------------------------

test("angleFromCenter: up/right/down/left map to 0/90/180/270", () => {
  const c = { x: 50, y: 50 };
  closeTo(angleFromCenter(c, { x: 50, y: 0 }), 0, "up");
  closeTo(angleFromCenter(c, { x: 100, y: 50 }), 90, "right");
  closeTo(angleFromCenter(c, { x: 50, y: 100 }), 180, "down");
  closeTo(angleFromCenter(c, { x: 0, y: 50 }), 270, "left");
});

test("angleFromCenter: a degenerate (point === center) vector falls back to 0", () => {
  const c = { x: 10, y: 10 };
  assert.equal(angleFromCenter(c, { x: 10, y: 10 }), 0);
});

// ---- rotationUpVector -- the inverse of angleFromCenter -------------------------

test("rotationUpVector: 0/90/180/270 point up/right/down/left", () => {
  closeTo(rotationUpVector(0).x, 0, "0.x");
  closeTo(rotationUpVector(0).y, -1, "0.y");
  closeTo(rotationUpVector(90).x, 1, "90.x");
  closeTo(rotationUpVector(90).y, 0, "90.y");
  closeTo(rotationUpVector(180).x, 0, "180.x");
  closeTo(rotationUpVector(180).y, 1, "180.y");
  closeTo(rotationUpVector(270).x, -1, "270.x");
  closeTo(rotationUpVector(270).y, 0, "270.y");
});

test("rotationUpVector: is the exact inverse of angleFromCenter for arbitrary angles", () => {
  const c = { x: 0, y: 0 };
  for (const theta of [12, 45, 137.5, 289]) {
    const up = rotationUpVector(theta);
    closeTo(angleFromCenter(c, { x: c.x + up.x, y: c.y + up.y }), theta, `theta=${theta}`);
  }
});

// ---- rotationFromDrag ------------------------------------------------------------

test("rotationFromDrag: start === current -> rotation unchanged from baseRotation", () => {
  const center = { x: 50, y: 50 };
  const p = { x: 50, y: 0 };
  assert.equal(rotationFromDrag(center, p, p, 37), 37);
});

test("rotationFromDrag: a +90 degree drag (up -> right) adds 90 to baseRotation", () => {
  const center = { x: 50, y: 50 };
  const up = { x: 50, y: 0 };
  const right = { x: 100, y: 50 };
  closeTo(rotationFromDrag(center, up, right, 0), 90, "0+90");
  closeTo(rotationFromDrag(center, up, right, 45), 135, "45+90");
});

test("rotationFromDrag: wraps into [0,360) across the 360/0 boundary", () => {
  const center = { x: 0, y: 0 };
  const up = { x: 0, y: -10 }; // angle 0
  const rightish = rotationUpVector(20); // angle 20 -> delta +20
  const point = { x: rightish.x * 10, y: rightish.y * 10 };
  closeTo(rotationFromDrag(center, up, point, 350), 10, "350+20 wraps to 10");
});

test("rotationFromDrag: opts.snap15 rounds the result to the nearest 15-degree step", () => {
  const center = { x: 0, y: 0 };
  const up = { x: 0, y: -10 }; // angle 0, grabbed
  const target = rotationUpVector(47); // delta +47 from baseRotation 0
  const point = { x: target.x * 10, y: target.y * 10 };
  const raw = rotationFromDrag(center, up, point, 0);
  closeTo(raw, 47, "raw (unsnapped) angle");
  const snapped = rotationFromDrag(center, up, point, 0, { snap15: true });
  assert.equal(snapped, 45);
});

test("rotationFromDrag: opts.snap15 wraps 360 down to 0", () => {
  const center = { x: 0, y: 0 };
  const up = { x: 0, y: -10 };
  const target = rotationUpVector(10); // delta +10
  const point = { x: target.x * 10, y: target.y * 10 };
  // baseRotation 352 + delta 10 = 362 -> wraps to 2 -> snapped round(2/15)*15 = 0.
  const snapped = rotationFromDrag(center, up, point, 352, { snap15: true });
  assert.equal(snapped, 0);
});

// ---- pointInRotatedBox ------------------------------------------------------------

test("pointInRotatedBox: rotation 0 is the plain AABB test", () => {
  const box = { x: 10, y: 10, w: 20, h: 10 };
  assert.equal(pointInRotatedBox({ x: 15, y: 15 }, box), true);
  assert.equal(pointInRotatedBox({ x: 5, y: 15 }, box), false);
});

test("pointInRotatedBox: a 90-degree rotated box hits points OUTSIDE its stale AABB and misses points INSIDE it", () => {
  const box = { x: 0, y: 0, w: 100, h: 50, rotation: 90 };
  // (50, -20): outside the unrotated AABB (y<0) but inside the rotated footprint.
  assert.equal(pointInRotatedBox({ x: 50, y: -20 }, box), true);
  // (50, 90): clearly outside the rotated footprint too (world-y range is [-25,75]).
  assert.equal(pointInRotatedBox({ x: 50, y: 90 }, box), false);
  // (5, 25): inside the stale unrotated AABB but OUTSIDE the rotated footprint
  // (world-x range after a 90-degree rotation is [25,75]).
  assert.equal(pointInRotatedBox({ x: 5, y: 25 }, box), false);
});

test("pointInRotatedBox: the box center is always inside, at any rotation", () => {
  const box = { x: 10, y: 20, w: 40, h: 30, rotation: 123.4 };
  assert.equal(pointInRotatedBox({ x: 30, y: 35 }, box), true);
});

// ---- rotatedHandlePoints -----------------------------------------------------------

test("rotatedHandlePoints: rotation 0 matches the plain AABB fan (box.x + box.w*fx)", () => {
  const box = { x: 10, y: 20, w: 100, h: 50 };
  const points = rotatedHandlePoints(box);
  for (const handle of SCALE_HANDLES) {
    const p = points.find((item) => item.key === handle.key);
    closeTo(p.x, box.x + box.w * handle.fx, `${handle.key}.x`);
    closeTo(p.y, box.y + box.h * handle.fy, `${handle.key}.y`);
  }
});

test("rotatedHandlePoints: corners match tree.rotatedCorners exactly; edges are their midpoints", () => {
  const box = { x: 0, y: 0, w: 100, h: 50, rotation: 37 };
  const [tl, tr, br, bl] = rotatedCorners(box);
  const points = rotatedHandlePoints(box);
  const at = (key) => points.find((item) => item.key === key);
  closeTo(at("nw").x, tl.x, "nw.x");
  closeTo(at("nw").y, tl.y, "nw.y");
  closeTo(at("ne").x, tr.x, "ne.x");
  closeTo(at("se").x, br.x, "se.x");
  closeTo(at("sw").x, bl.x, "sw.x");
  closeTo(at("n").x, (tl.x + tr.x) / 2, "n.x is the tl/tr midpoint");
  closeTo(at("n").y, (tl.y + tr.y) / 2, "n.y is the tl/tr midpoint");
  closeTo(at("e").x, (tr.x + br.x) / 2, "e.x is the tr/br midpoint");
});

test("rotatedHandlePoints: every entry carries the SAME fx/fy/cursor SCALE_HANDLES itself does", () => {
  const points = rotatedHandlePoints({ x: 0, y: 0, w: 10, h: 10, rotation: 45 });
  for (const handle of SCALE_HANDLES) {
    const p = points.find((item) => item.key === handle.key);
    assert.equal(p.fx, handle.fx);
    assert.equal(p.fy, handle.fy);
    assert.equal(p.cursor, handle.cursor);
  }
});

// ---- resizeRotatedBox --------------------------------------------------------------

test("resizeRotatedBox: rotation 0 is numerically identical to resizeBox (plus sx/sy)", () => {
  const box = { x: 10, y: 20, w: 100, h: 50 };
  const delta = { dx: 20, dy: 10 };
  const plain = resizeBox(box, handle("se"), delta, {});
  const rotated = resizeRotatedBox(box, handle("se"), delta, {});
  assert.deepEqual({ x: rotated.x, y: rotated.y, w: rotated.w, h: rotated.h }, plain);
  assert.equal(rotated.sx, plain.w / box.w);
  assert.equal(rotated.sy, plain.h / box.h);
});

test("resizeRotatedBox: a 90-degree box's 'e' handle grows it in its OWN local frame, anchored on the opposite (local) edge", () => {
  const box = { x: 0, y: 0, w: 100, h: 50, rotation: 90 };
  const result = resizeRotatedBox(box, handle("e"), { dx: 0, dy: 10 }, {});
  closeTo(result.x, -5, "x");
  closeTo(result.y, 5, "y");
  closeTo(result.w, 110, "w");
  closeTo(result.h, 50, "h");
  closeTo(result.sx, 1.1, "sx");
  closeTo(result.sy, 1, "sy");
});

// Property test: for a CORNER handle (non-fromCenter), the ROTATED anchor corner -- the
// corner OPPOSITE the dragged one -- stays at the exact same WORLD point before and after
// the resize, at several rotations. This is the geometric invariant the whole function
// exists to preserve (R3/R4: "recompute x/y so the rotated opposite corner stays put in
// world space").
test("resizeRotatedBox: the rotated anchor corner never moves in world space, at several rotations", () => {
  const origBox = { x: 0, y: 0, w: 100, h: 50 };
  for (const rotation of [30, 90, 137, 250]) {
    // handle "se" (fx=1,fy=1) anchors the OPPOSITE corner: index 0 ("nw"/tl) in
    // rotatedCorners' [tl,tr,br,bl] order.
    const box = { ...origBox, rotation };
    const anchorBefore = rotatedCorners(box)[0];
    const resized = resizeRotatedBox(box, handle("se"), { dx: 15, dy: -8 }, {});
    const anchorAfter = rotatedCorners({ ...resized, rotation })[0];
    closeTo(anchorAfter.x, anchorBefore.x, `rotation=${rotation} anchor.x`);
    closeTo(anchorAfter.y, anchorBefore.y, `rotation=${rotation} anchor.y`);
  }
});

test("resizeRotatedBox: fromCenter (Alt) keeps the box's rotation CENTER fixed regardless of rotation", () => {
  const box = { x: 0, y: 0, w: 100, h: 100, rotation: 61 };
  const centerBefore = { x: box.x + box.w / 2, y: box.y + box.h / 2 };
  const resized = resizeRotatedBox(box, handle("se"), { dx: 10, dy: 10 }, { fromCenter: true });
  closeTo(resized.x + resized.w / 2, centerBefore.x, "center.x unchanged");
  closeTo(resized.y + resized.h / 2, centerBefore.y, "center.y unchanged");
});

test("resizeRotatedBox: proportional lock preserves aspect ratio in the local (rotated) frame too", () => {
  const box = { x: 0, y: 0, w: 100, h: 50, rotation: 45 }; // aspect 2:1
  const resized = resizeRotatedBox(box, handle("se"), { dx: 20, dy: 5 }, { proportional: true });
  closeTo(resized.w / resized.h, 2, "aspect ratio preserved");
});

test("resizeRotatedBox: an edge handle stays 1-axis in the local frame even under rotation", () => {
  const box = { x: 0, y: 0, w: 100, h: 50, rotation: 45 };
  const resized = resizeRotatedBox(box, handle("e"), { dx: 0, dy: 0 }, {});
  // Zero world delta -> zero local delta at any rotation -> the box is untouched.
  closeTo(resized.w, 100, "w unchanged at zero delta");
  closeTo(resized.h, 50, "h unchanged at zero delta");
});
