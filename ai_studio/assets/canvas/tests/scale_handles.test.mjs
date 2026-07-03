// T0232 increment 2 -- transform gizmo skeleton (move + 8 scale handles + reset-to-source,
// NO rotation/flip/rotate-handle -- those are increments 3a/3b). Pure box math only
// (site/viewport.mjs's resizeBox/mapItemBox/scaledFontSize/SCALE_HANDLES): the drag
// lifecycle (screen-space hit-test, live in-memory mutation, one-batch commit) lives in
// site/workspace.js, which imports app.js (DOM) and so is verified on the page, not here
// (same split viewport.test.mjs/snap.test.mjs already use for their own drag math). Run:
//   node --test ai_studio/assets/canvas/tests/scale_handles.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mapItemBox, resizeBox, scaledFontSize, SCALE_HANDLES } from "../site/viewport.mjs";
import { unionBBox } from "../tree.mjs";

function handle(key) {
  const found = SCALE_HANDLES.find((item) => item.key === key);
  assert.ok(found, `no such handle: ${key}`);
  return found;
}

// ---- SCALE_HANDLES shape -------------------------------------------------------

test("SCALE_HANDLES: 8 handles, corners at 0/1 and edges at 0.5, each with a resize cursor", () => {
  assert.equal(SCALE_HANDLES.length, 8);
  const keys = SCALE_HANDLES.map((h) => h.key).sort();
  assert.deepEqual(keys, ["e", "n", "ne", "nw", "s", "se", "sw", "w"].sort());
  for (const h of SCALE_HANDLES) {
    assert.ok([0, 0.5, 1].includes(h.fx), `${h.key} fx out of range`);
    assert.ok([0, 0.5, 1].includes(h.fy), `${h.key} fy out of range`);
    assert.ok(typeof h.cursor === "string" && h.cursor.endsWith("-resize"));
  }
});

// ---- resizeBox: corner handles (free / unlocked) --------------------------------

test("resizeBox: dragging the SE corner grows w/h and anchors the opposite (NW) corner", () => {
  const box = { x: 10, y: 20, w: 100, h: 50 };
  const result = resizeBox(box, handle("se"), { dx: 20, dy: 10 }, {});
  assert.deepEqual(result, { x: 10, y: 20, w: 120, h: 60 });
});

test("resizeBox: dragging the NW corner outward anchors the opposite (SE) corner", () => {
  const box = { x: 10, y: 20, w: 100, h: 50 };
  // Dragging NW up-and-left (negative delta) grows the box while the SE corner (110,70)
  // stays exactly where it was.
  const result = resizeBox(box, handle("nw"), { dx: -15, dy: -5 }, {});
  assert.deepEqual(result, { x: -5, y: 15, w: 115, h: 55 });
  assert.equal(result.x + result.w, 110, "SE corner x stayed anchored");
  assert.equal(result.y + result.h, 70, "SE corner y stayed anchored");
});

// ---- resizeBox: edge handles are ALWAYS 1-axis, even with proportional:true ----

test("resizeBox: an edge handle (e) only changes width -- height is untouched", () => {
  const box = { x: 10, y: 20, w: 100, h: 50 };
  const result = resizeBox(box, handle("e"), { dx: 30, dy: 999 }, {});
  assert.deepEqual(result, { x: 10, y: 20, w: 130, h: 50 });
});

test("resizeBox: edge handle stays 1-axis even when proportional is requested (design doc: edge = 1-axis, always)", () => {
  const box = { x: 10, y: 20, w: 100, h: 50 };
  const result = resizeBox(box, handle("e"), { dx: 20, dy: 999 }, { proportional: true });
  assert.deepEqual(result, { x: 10, y: 20, w: 120, h: 50 });
});

// ---- resizeBox: minSize floor never inverts the box -----------------------------

test("resizeBox: minSize clamps a shrink so the box never inverts", () => {
  const box = { x: 10, y: 0, w: 10, h: 10 };
  // Dragging the WEST edge far past the opposite (east) edge would invert the box; the
  // anchor (east edge, x=20) must stay fixed and w floors at minSize.
  const result = resizeBox(box, handle("w"), { dx: 50, dy: 0 }, { minSize: 1 });
  assert.equal(result.w, 1);
  assert.equal(result.x + result.w, 20, "the anchored (east) edge never moved");
});

// ---- resizeBox: proportional lock (corner only) ---------------------------------

test("resizeBox: proportional corner drag preserves aspect ratio (dominant axis drives both dims)", () => {
  const box = { x: 10, y: 20, w: 100, h: 50 }; // aspect 2:1
  const free = resizeBox(box, handle("se"), { dx: 20, dy: 5 }, { proportional: false });
  const locked = resizeBox(box, handle("se"), { dx: 20, dy: 5 }, { proportional: true });
  // Free resize: w=120, h=55 -- axes move independently (proves the two paths differ).
  assert.deepEqual(free, { x: 10, y: 20, w: 120, h: 55 });
  // Proportional: the wider delta (dx=20) dominates -> w=120, h derived from the SAME 2:1
  // aspect ratio (60), not the raw dy.
  assert.deepEqual(locked, { x: 10, y: 20, w: 120, h: 60 });
  assert.equal(locked.w / locked.h, box.w / box.h, "aspect ratio preserved exactly");
});

test("resizeBox: proportional lock also applies when the Y delta dominates", () => {
  const box = { x: 0, y: 0, w: 100, h: 50 }; // aspect 2:1
  const locked = resizeBox(box, handle("se"), { dx: 4, dy: 30 }, { proportional: true });
  // dy=30 dominates -> h=80, w derived from aspect (160).
  assert.deepEqual(locked, { x: 0, y: 0, w: 160, h: 80 });
});

// ---- resizeBox: Alt / fromCenter --------------------------------------------------

test("resizeBox: fromCenter (Alt) grows symmetrically about the box center", () => {
  const box = { x: 0, y: 0, w: 100, h: 100 };
  const result = resizeBox(box, handle("se"), { dx: 10, dy: 10 }, { fromCenter: true });
  // Both edges move by the same delta -> total growth is 2x on each axis, centered.
  assert.deepEqual(result, { x: -10, y: -10, w: 120, h: 120 });
  assert.equal(result.x + result.w / 2, 50, "center x unchanged");
  assert.equal(result.y + result.h / 2, 50, "center y unchanged");
});

test("resizeBox: fromCenter combined with proportional keeps aspect AND the center fixed", () => {
  const box = { x: 0, y: 0, w: 100, h: 50 };
  const result = resizeBox(box, handle("se"), { dx: 10, dy: 2 }, { fromCenter: true, proportional: true });
  assert.equal(result.w / result.h, 2, "aspect ratio preserved");
  assert.equal(result.x + result.w / 2, 50, "center x unchanged");
  assert.equal(result.y + result.h / 2, 25, "center y unchanged");
});

// ---- mapItemBox: multi-block scale (the math behind ONE batched patchElements) -----

test("mapItemBox: a single node's box maps directly onto the new AABB (degenerate 1-node case)", () => {
  const origAABB = { x: 10, y: 20, w: 100, h: 50 };
  const newAABB = resizeBox(origAABB, handle("se"), { dx: 20, dy: 10 }, {});
  const mapped = mapItemBox(origAABB, origAABB, newAABB);
  assert.deepEqual({ x: mapped.x, y: mapped.y, w: mapped.w, h: mapped.h }, newAABB);
});

test("mapItemBox: multi-block scale remaps every selected node proportionally within the resized AABB -- the exact per-node math behind ONE batched elements-set patches[] array", () => {
  // Two elements side by side inside a selection; dragging the SE handle doubles the block.
  const a = { x: 0, y: 0, w: 20, h: 10 };
  const b = { x: 40, y: 10, w: 20, h: 20 };
  const origAABB = unionBBox([a, b]); // {x:0,y:0,w:60,h:30}
  const newAABB = resizeBox(origAABB, handle("se"), { dx: 60, dy: 30 }, {}); // {x:0,y:0,w:120,h:60}
  assert.deepEqual(newAABB, { x: 0, y: 0, w: 120, h: 60 });

  const mappedA = mapItemBox(a, origAABB, newAABB);
  const mappedB = mapItemBox(b, origAABB, newAABB);
  // Both axes doubled (2x block scale) -> each node's box doubles too, in place.
  assert.deepEqual({ x: mappedA.x, y: mappedA.y, w: mappedA.w, h: mappedA.h }, { x: 0, y: 0, w: 40, h: 20 });
  assert.deepEqual({ x: mappedB.x, y: mappedB.y, w: mappedB.w, h: mappedB.h }, { x: 80, y: 20, w: 40, h: 40 });
  // This is exactly the per-node {x,y,w,h} pair a caller batches into ONE
  // elements-set {patches:[{elementId:a.id,...mappedA}, {elementId:b.id,...mappedB}]} call --
  // one journal entry / one undo for the whole gesture (verified end-to-end on the page per
  // the packet's manual checklist; ops.mjs/patchElements itself is untouched by this
  // increment).
});

test("mapItemBox: a degenerate (zero-size) AABB axis does not divide by zero", () => {
  const origAABB = { x: 0, y: 0, w: 0, h: 10 };
  const newAABB = { x: 0, y: 0, w: 0, h: 20 };
  const mapped = mapItemBox({ x: 0, y: 2, w: 0, h: 5 }, origAABB, newAABB);
  assert.equal(mapped.sx, 1, "zero-width AABB falls back to an identity x-scale");
  assert.equal(mapped.sy, 2);
});

// ---- scaledFontSize: text mapping (never stretch the box; scale the font instead) -----

test("scaledFontSize: fontSize scales by the block's height ratio (sy), matching mapItemBox's sy", () => {
  const origAABB = { x: 0, y: 0, w: 100, h: 40 };
  const newAABB = resizeBox(origAABB, handle("se"), { dx: 0, dy: 20 }, {}); // h: 40 -> 60, ratio 1.5
  const mapped = mapItemBox(origAABB, origAABB, newAABB);
  assert.equal(scaledFontSize(24, mapped.sy), 36);
});

test("scaledFontSize: floors at minSize and never returns a non-finite/negative size", () => {
  assert.equal(scaledFontSize(10, 0.01), 1); // floors at the default minSize (1)
  assert.equal(scaledFontSize(24, 0), 24); // ratio 0 is invalid -> falls back to *1 (unchanged)
  assert.equal(scaledFontSize(24, -2), 24); // negative ratio is invalid -> falls back to *1
  assert.equal(scaledFontSize(24, NaN), 24);
});
