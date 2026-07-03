// T0236 world-anchored drag math: a move/pan drag stores its grab point in IMAGE (world)
// space and re-derives the delta from the CURRENT pointer through the CURRENT viewport every
// frame. The invariant this guards: a wheel-zoom in the MIDDLE of a drag keeps the grabbed
// point exactly under the cursor (the old screen-anchor path re-interpreted the whole
// accumulated delta at the new scale and drifted). Pure math, no DOM — runs in node.
//   node --test ai_studio/assets/canvas/tests/viewport_drag.test.mjs
import { strict as assert } from "node:assert";
import test from "node:test";
import {
  dragWorldDelta,
  imageToScreenPoint,
  panOffsetFor,
  screenToImagePoint,
  zoomViewportAt,
} from "../site/viewport.mjs";

// The exact expression the OLD (buggy) move path used: raw screen delta / CURRENT scale.
// Kept here only to PROVE the new path diverges from it under a mid-drag zoom.
const legacyScreenDelta = (startScreen, screen, viewport) => ({
  dx: (screen.x - startScreen.x) / viewport.scale,
  dy: (screen.y - startScreen.y) / viewport.scale,
});

test("dragWorldDelta keeps the grabbed element point under the cursor after a mid-drag zoom", () => {
  // Drag starts at scale 1. Grab an element at world (90,90); the press lands at screen
  // (100,100) -> world (100,100), i.e. 10px into the element.
  const vp1 = { scale: 1, offsetX: 0, offsetY: 0 };
  const startScreen = { x: 100, y: 100 };
  const grabWorld = screenToImagePoint(startScreen, vp1);
  const orig = { x: 90, y: 90 };
  const grabOffset = { x: grabWorld.x - orig.x, y: grabWorld.y - orig.y }; // (10,10)

  // Move a little BEFORE zooming (sanity: matches the legacy path while scale is unchanged).
  const mid = { x: 140, y: 140 };
  const dMid = dragWorldDelta(grabWorld, mid, vp1);
  assert.deepEqual(dMid, legacyScreenDelta(startScreen, mid, vp1), "no-zoom delta unchanged");

  // Now wheel-zoom x2 AT THE CURRENT POINTER (like a real mid-drag wheel), then move on.
  const vp2 = zoomViewportAt(vp1, 2, mid);
  assert.equal(vp2.scale, 2);
  const after = { x: 180, y: 180 };

  const d = dragWorldDelta(grabWorld, after, vp2);
  const elementPos = { x: orig.x + d.dx, y: orig.y + d.dy };
  const grabbedPoint = { x: elementPos.x + grabOffset.x, y: elementPos.y + grabOffset.y };
  const cursorWorld = screenToImagePoint(after, vp2);
  assert.deepEqual(grabbedPoint, cursorWorld, "grabbed point stays glued to the cursor under zoom");

  // And demonstrate the OLD path would have drifted (proves the bug + the fix are real).
  const legacy = legacyScreenDelta(startScreen, after, vp2);
  const legacyGrabbed = { x: orig.x + legacy.dx + grabOffset.x, y: orig.y + legacy.dy + grabOffset.y };
  assert.notDeepEqual(legacyGrabbed, cursorWorld, "old screen-anchor path drifts off the cursor");
});

test("dragWorldDelta is scale/pan agnostic: same cursor world -> same element position", () => {
  const grabWorld = { x: 200, y: 150 };
  const orig = { x: 180, y: 130 };
  // Two totally different viewports that both place the cursor over the SAME world point.
  const vpA = { scale: 1, offsetX: 0, offsetY: 0 };
  const vpB = { scale: 3.5, offsetX: -400, offsetY: 220 };
  const worldTarget = { x: 260, y: 190 };
  const screenA = imageToScreenPoint(worldTarget, vpA);
  const screenB = imageToScreenPoint(worldTarget, vpB);

  const dA = dragWorldDelta(grabWorld, screenA, vpA);
  const dB = dragWorldDelta(grabWorld, screenB, vpB);
  assert.deepEqual({ x: orig.x + dA.dx, y: orig.y + dA.dy }, { x: orig.x + dB.dx, y: orig.y + dB.dy });
});

test("panOffsetFor pins the grabbed world point under the cursor across a mid-pan zoom", () => {
  // Pan grabs world (200,200) at screen (200,200), scale 1.
  const vp1 = { scale: 1, offsetX: 0, offsetY: 0 };
  const startScreen = { x: 200, y: 200 };
  const grabWorld = screenToImagePoint(startScreen, vp1);

  // At the grab instant the offset is unchanged.
  const at0 = panOffsetFor(grabWorld, startScreen, vp1);
  assert.deepEqual({ x: at0.offsetX, y: at0.offsetY }, { x: 0, y: 0 });

  // Pan to screen (250,250) with no zoom == the legacy "origOffset + screen delta".
  const at1 = panOffsetFor(grabWorld, { x: 250, y: 250 }, vp1);
  assert.deepEqual({ x: at1.offsetX, y: at1.offsetY }, { x: 50, y: 50 });

  // Now a wheel-zoom happens mid-pan; onWheel rebases grabWorld to the wheel anchor, so the
  // pan continues from the zoomed viewport. Model that: zoom x2 at the current pointer, then
  // rebase the anchor exactly as onWheel does, then keep panning.
  const cursor = { x: 250, y: 250 };
  const zoomed = zoomViewportAt(vp1, 2, cursor);
  const rebased = screenToImagePoint(cursor, zoomed); // onWheel: drag.grabWorld = image point under pointer
  // The instant after the rebase, panOffsetFor reproduces the wheel's own offset (zero snap).
  const seam = panOffsetFor(rebased, cursor, zoomed);
  assert.ok(Math.abs(seam.offsetX - zoomed.offsetX) < 1e-9, "no offset jump at the wheel instant");
  assert.ok(Math.abs(seam.offsetY - zoomed.offsetY) < 1e-9);

  // Keep panning after the zoom: the rebased world point stays under the moving cursor.
  const moved = { x: 300, y: 280 };
  const at2 = panOffsetFor(rebased, moved, zoomed);
  const backToWorld = screenToImagePoint(moved, { ...zoomed, offsetX: at2.offsetX, offsetY: at2.offsetY });
  assert.ok(Math.abs(backToWorld.x - rebased.x) < 1e-9, "grabbed world point tracks the cursor");
  assert.ok(Math.abs(backToWorld.y - rebased.y) < 1e-9);
  assert.equal(at2.scale, 2, "pan preserves the (zoomed) scale");
});

test("panOffsetFor matches the legacy pan formula while no zoom happens", () => {
  const vp = { scale: 1.5, offsetX: 30, offsetY: -10 };
  const startScreen = { x: 120, y: 90 };
  const grabWorld = screenToImagePoint(startScreen, vp);
  for (const screen of [{ x: 120, y: 90 }, { x: 160, y: 130 }, { x: 40, y: 200 }]) {
    const next = panOffsetFor(grabWorld, screen, vp);
    // Legacy: origOffset + (screen - start), scale fixed.
    assert.ok(Math.abs(next.offsetX - (vp.offsetX + (screen.x - startScreen.x))) < 1e-9);
    assert.ok(Math.abs(next.offsetY - (vp.offsetY + (screen.y - startScreen.y))) < 1e-9);
  }
});
