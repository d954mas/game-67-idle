// T0272 -- pure aspect-ratio-lock math lives inside site/inspector.js (boxGrid's W/H
// "keep proportions" toggle). inspector.js is otherwise DOM page code with no test
// harness, but it has no top-level document/window/localStorage access (only inside
// function bodies), so importing it in plain Node to reach these two pure exports is
// safe -- confirmed by a standalone dry-run import before writing this file. Run:
//   node --test ai_studio/assets/canvas/tests/aspect_lock.test.mjs
import { strict as assert } from "node:assert";
import test from "node:test";
import { defaultAspectLock, linkedDimension } from "../site/inspector.js";

// ---- linkedDimension ----------------------------------------------------------

test("linkedDimension: editing w computes h from the CURRENT w/h ratio", () => {
  // current 100x50 (2:1) -> w edited to 200 -> h = round(200 * 50/100) = 100
  assert.equal(linkedDimension(100, 50, "w", 200), 100);
});

test("linkedDimension: editing h computes w from the CURRENT w/h ratio (symmetric)", () => {
  // current 100x50 (2:1) -> h edited to 25 -> w = round(25 * 100/50) = 50
  assert.equal(linkedDimension(100, 50, "h", 25), 50);
});

test("linkedDimension: rounds to the nearest integer pixel", () => {
  // current 100x30 -> w edited to 70 -> h = 70 * 30/100 = 21 exactly
  assert.equal(linkedDimension(100, 30, "w", 70), 21);
  // current 90x40 -> w edited to 50 -> h = 50 * 40/90 = 22.222... -> rounds to 22
  assert.equal(linkedDimension(90, 40, "w", 50), 22);
});

test("linkedDimension: degenerate current ratio (w or h <= 0) returns null, never divides by zero", () => {
  assert.equal(linkedDimension(0, 50, "w", 200), null);
  assert.equal(linkedDimension(100, 0, "h", 25), null);
  assert.equal(linkedDimension(-10, 50, "w", 200), null);
});

test("linkedDimension: a non-finite or non-positive new value returns null (caller falls back to a free edit)", () => {
  assert.equal(linkedDimension(100, 50, "w", NaN), null);
  assert.equal(linkedDimension(100, 50, "w", Infinity), null);
  assert.equal(linkedDimension(100, 50, "w", 0), null);
  assert.equal(linkedDimension(100, 50, "w", -5), null);
});

test("linkedDimension: an unrecognized edited key returns null", () => {
  assert.equal(linkedDimension(100, 50, "x", 200), null);
});

// ---- defaultAspectLock ----------------------------------------------------------

test("defaultAspectLock: no source dims (group/note) defaults ON", () => {
  assert.equal(defaultAspectLock({ w: 100, h: 40 }), true);
  assert.equal(defaultAspectLock({ w: 100, h: 40, source_w: 0, source_h: 0 }), true);
});

test("defaultAspectLock: current ratio matches source ratio -> ON", () => {
  assert.equal(defaultAspectLock({ w: 200, h: 100, source_w: 400, source_h: 200 }), true);
});

test("defaultAspectLock: current ratio within the small rounding tolerance of source -> ON", () => {
  // source ratio 2:1 exactly; current 201/100 = 2.01 -- within 1% of 2.
  assert.equal(defaultAspectLock({ w: 201, h: 100, source_w: 400, source_h: 200 }), true);
});

test("defaultAspectLock: current ratio distorted relative to source -> OFF", () => {
  // source ratio 2:1; current ratio 1:1 -- well outside tolerance.
  assert.equal(defaultAspectLock({ w: 100, h: 100, source_w: 400, source_h: 200 }), false);
});

test("defaultAspectLock: degenerate current box (w or h <= 0) with source dims present defaults ON", () => {
  assert.equal(defaultAspectLock({ w: 0, h: 100, source_w: 400, source_h: 200 }), true);
  assert.equal(defaultAspectLock({ w: 100, h: 0, source_w: 400, source_h: 200 }), true);
});
