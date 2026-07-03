// Canvas viewport math: pure pan/zoom/transform helpers shared by the site
// modules (workspace, actions, dnd, regions). Screen<->image point conversion,
// fit-to-frame centering, and zoom-at-cursor that keeps the point under the
// cursor stable. No DOM. Rect/polygon editing geometry is NOT here -- the region
// workbench owns that in regions.js. Moved into the canvas module from the
// retired asset_tools editor so the canvas owns its own viewport code.
//
// Imports tree.mjs's rotatedCorners for the rotate-gizmo section near the end of this file
// (T0232 increment 3b) -- tree.mjs itself has zero imports, so this stays acyclic.
import { rotatedCorners } from "../tree.mjs";

const minScale = 0.05;
const maxScale = 12;

export function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

export function fitViewport({ imageWidth, imageHeight, frameWidth, frameHeight, padding = 0 }) {
  if (!imageWidth || !imageHeight || !frameWidth || !frameHeight) {
    return { scale: 1, offsetX: 0, offsetY: 0 };
  }
  const availableWidth = Math.max(1, frameWidth - padding * 2);
  const availableHeight = Math.max(1, frameHeight - padding * 2);
  const scale = clamp(Math.min(availableWidth / imageWidth, availableHeight / imageHeight), minScale, maxScale);
  return {
    scale,
    offsetX: (frameWidth - imageWidth * scale) / 2,
    offsetY: (frameHeight - imageHeight * scale) / 2,
  };
}

export function zoomViewportAt(viewport, factor, screenPoint) {
  const nextScale = clamp(viewport.scale * factor, minScale, maxScale);
  const imagePoint = screenToImagePoint(screenPoint, viewport);
  return {
    scale: nextScale,
    offsetX: screenPoint.x - imagePoint.x * nextScale,
    offsetY: screenPoint.y - imagePoint.y * nextScale,
  };
}

export function imageToScreenPoint(point, viewport) {
  return {
    x: point.x * viewport.scale + viewport.offsetX,
    y: point.y * viewport.scale + viewport.offsetY,
  };
}

export function screenToImagePoint(point, viewport) {
  return {
    x: (point.x - viewport.offsetX) / viewport.scale,
    y: (point.y - viewport.offsetY) / viewport.scale,
  };
}

// The world-space delta of a move drag: the CURRENT pointer's image point minus the
// image point grabbed when the drag began. The anchor is stored ONCE in world space, and
// the pointer is re-projected through the LIVE viewport every move, so a mid-drag zoom or
// pan is absorbed automatically — the grabbed image point stays under the cursor. (The old
// path stored a SCREEN anchor and divided the raw screen delta by the current scale, which
// re-interpreted the whole accumulated delta the instant the viewport changed.)
export function dragWorldDelta(grabWorld, screenPoint, viewport) {
  const current = screenToImagePoint(screenPoint, viewport);
  return { dx: current.x - grabWorld.x, dy: current.y - grabWorld.y };
}

// The viewport offset that pins `grabWorld` under `screenPoint` at the viewport's current
// scale — the pan equivalent of the world anchor. Panning is screen-space by nature, so its
// anchor is re-solved every move from the grabbed world point; a mid-pan zoom (which changes
// scale + offset) is therefore corrected on the next move without the pan handler having to
// know a wheel happened. Keeps the rest of the viewport (scale) untouched.
export function panOffsetFor(grabWorld, screenPoint, viewport) {
  return {
    ...viewport,
    offsetX: screenPoint.x - grabWorld.x * viewport.scale,
    offsetY: screenPoint.y - grabWorld.y * viewport.scale,
  };
}

// ---- scale gizmo box math (T0232 increment 2 -- skeleton, NO rotation) --------
//
// Pure element/group RESIZE geometry, mirroring this file's pan/zoom math: no DOM, world-
// space in/out, shared by workspace.js's live drag preview AND its own headless test
// (tests/scale_handles.test.mjs). This is a DIFFERENT gesture family from a translate drag
// (dragWorldDelta/panOffsetFor above) -- resize never runs the T0244 snap path (site/snap.mjs);
// a scale drag's delta only ever feeds resizeBox/mapItemBox below.
//
// Eight handles as fractional positions on a box, back-compatible in shape with regions.js's
// REGION_HANDLES (same fx/fy/cursor convention: fx/fy 0/1 = a corner, 0.5 = "doesn't move on
// this axis" i.e. an edge) but kept as its own constant -- regions.js already imports FROM
// this module (viewport.mjs), so importing back would be circular.
export const SCALE_HANDLES = [
  { key: "nw", fx: 0, fy: 0, cursor: "nwse-resize" },
  { key: "n", fx: 0.5, fy: 0, cursor: "ns-resize" },
  { key: "ne", fx: 1, fy: 0, cursor: "nesw-resize" },
  { key: "e", fx: 1, fy: 0.5, cursor: "ew-resize" },
  { key: "se", fx: 1, fy: 1, cursor: "nwse-resize" },
  { key: "s", fx: 0.5, fy: 1, cursor: "ns-resize" },
  { key: "sw", fx: 0, fy: 1, cursor: "nesw-resize" },
  { key: "w", fx: 0, fy: 0.5, cursor: "ew-resize" },
];

// One axis (x+w, or y+h) of a handle drag. `edge` is the handle's fx (for the x axis) or fy
// (for the y axis): 0 = the near/origin edge is being dragged (the far edge is the anchor),
// 1 = the far edge is being dragged (the origin is the anchor), 0.5 = this axis doesn't move
// at all (the OTHER axis's edge value drives it, e.g. dragging "n" only ever touches y/h).
// `fromCenter` (Alt) grows/shrinks symmetrically about the box's CURRENT center instead of
// anchoring the opposite edge -- both edges move, so the delta counts double.
function resizeAxis(origin, size, delta, edge, fromCenter, minSize) {
  if (edge === 0.5) return { origin, size };
  if (fromCenter) {
    const growth = (edge === 0 ? -delta : delta) * 2;
    const newSize = Math.max(minSize, size + growth);
    return { origin: origin + size / 2 - newSize / 2, size: newSize };
  }
  if (edge === 0) {
    const far = origin + size; // anchor: the opposite edge never moves
    let newOrigin = origin + delta;
    let newSize = far - newOrigin;
    if (newSize < minSize) {
      newSize = minSize;
      newOrigin = far - minSize;
    }
    return { origin: newOrigin, size: newSize };
  }
  return { origin, size: Math.max(minSize, size + delta) }; // edge === 1: origin anchored
}

// Corner-only aspect-locked resize: derive ONE uniform scale from whichever axis moved
// further (in its own "growth" direction, i.e. away from the anchor), then apply it to BOTH
// dimensions, so a diagonal drag never fights itself between two independently-clamped axes.
function resizeBoxProportional(box, handle, delta, fromCenter, minSize) {
  const { x, y, w, h } = box;
  const aspect = (h !== 0 ? w / h : 0) || 1;
  const gx = (handle.fx === 0 ? -delta.dx : delta.dx) * (fromCenter ? 2 : 1);
  const gy = (handle.fy === 0 ? -delta.dy : delta.dy) * (fromCenter ? 2 : 1);
  let newW;
  let newH;
  if (Math.abs(gx) >= Math.abs(gy)) {
    newW = Math.max(minSize, w + gx);
    newH = Math.max(minSize, newW / aspect);
  } else {
    newH = Math.max(minSize, h + gy);
    newW = Math.max(minSize, newH * aspect);
  }
  if (fromCenter) {
    const cx = x + w / 2;
    const cy = y + h / 2;
    return { x: cx - newW / 2, y: cy - newH / 2, w: newW, h: newH };
  }
  return {
    x: handle.fx === 0 ? x + w - newW : x,
    y: handle.fy === 0 ? y + h - newH : y,
    w: newW,
    h: newH,
  };
}

// resizeBox(box, handle, delta, opts) -> {x, y, w, h}
// `box` = the ORIGINAL {x,y,w,h} (world units); `handle` = one of SCALE_HANDLES (or an
// equivalent {fx,fy}); `delta` = {dx,dy} the RAW world-space pointer delta since grab
// (dragWorldDelta's return shape -- unrounded; the caller rounds ONCE at commit, never
// mid-drag). `opts.proportional` only takes effect on a CORNER handle (edge handles are
// always 1-axis, per the design doc -- Shift/aspect-lock never applies to an edge drag);
// `opts.fromCenter` (Alt) anchors the box's center instead of the opposite edge/corner;
// `opts.minSize` floors both dimensions (default 1) so a resize can never invert the box.
export function resizeBox(box, handle, delta, opts = {}) {
  const minSize = Number.isFinite(opts.minSize) ? opts.minSize : 1;
  const fromCenter = !!opts.fromCenter;
  const isCorner = handle.fx !== 0.5 && handle.fy !== 0.5;
  if (opts.proportional && isCorner) {
    return resizeBoxProportional(box, handle, delta, fromCenter, minSize);
  }
  const rx = resizeAxis(box.x, box.w, delta.dx, handle.fx, fromCenter, minSize);
  const ry = resizeAxis(box.y, box.h, delta.dy, handle.fy, fromCenter, minSize);
  return { x: rx.origin, y: ry.origin, w: rx.size, h: ry.size };
}

// mapItemBox(itemBox, origAABB, newAABB) -> {x, y, w, h, sx, sy}
// Maps ONE node's original box through the selection's AABB resize (origAABB -> newAABB),
// keeping its position relative to the AABB proportional -- the "scale a block" math behind
// a multi-selection drag: every selected node (including a selected GROUP's own frame --
// never its descendants, T0232 Q2 "frame-only") is remapped by the SAME per-axis factors, so
// the block resizes as one rigid composition. A single-node drag is the degenerate case
// (itemBox === origAABB), which collapses to newAABB directly (sx/sy apply identically).
// sx/sy are returned too: a TEXT item ignores w/h (auto-measured from font size, never
// stretched -- see scaledFontSize) but still needs sy to scale its font size by the same
// factor the rest of the block is scaling by.
export function mapItemBox(itemBox, origAABB, newAABB) {
  const sx = origAABB.w !== 0 ? newAABB.w / origAABB.w : 1;
  const sy = origAABB.h !== 0 ? newAABB.h / origAABB.h : 1;
  return {
    x: newAABB.x + (itemBox.x - origAABB.x) * sx,
    y: newAABB.y + (itemBox.y - origAABB.y) * sy,
    w: itemBox.w * sx,
    h: itemBox.h * sy,
    sx,
    sy,
  };
}

// A text element never has its box directly stretched (PIL re-measures from content +
// fontSize, so a stretched w/h would just be discarded next paint/export) -- instead its
// fontSize scales by the SAME ratio the rest of a resized block is scaling by (mapItemBox's
// sy, since box height IS fontSize*lineHeight*lines -- the natural "size" axis for text).
export function scaledFontSize(origFontSize, ratio, minSize = 1) {
  const size = Number(origFontSize) || 0;
  const r = Number.isFinite(ratio) && ratio > 0 ? ratio : 1;
  return Math.max(minSize, size * r);
}

// ---- rotate gizmo math (T0232 increment 3b -- the interactive rotate handle + rotated
// hit-test/scale, deferred out of increment 3a's data+render+parity packet; see README
// "Rotation & flip"). Reuses tree.mjs's rotatedCorners (the SAME rotation convention:
// degrees CW on a Y-down canvas, about the node's own box CENTER) so the page's gizmo
// geometry and the pure hit-test math never diverge on the sign or pivot of a rotation.
// Pure, DOM-free, unit-tested in tests/rotate.test.mjs -- workspace.js only wires
// screen-space drawing/hit-testing and the drag lifecycle around these functions,
// mirroring the scale gizmo section above.

// The CW-positive angle (degrees, [0,360)) a `point` makes around `center`, measured from
// straight UP (the direction an unrotated element's top-center handle points) -- the
// INVERSE of rotatedCorners' own forward rotation: a point at rotation theta sits at
// center + (sin(theta), -cos(theta))*r for some r>0 (apply rotatedCorners' corner formula to
// a (0,-h/2) offset), so solving for theta from a raw (dx,dy) offset is atan2(dx, -dy).
// Degenerate at point===center (dx=dy=0): atan2(0,0)=0, i.e. "no rotation" -- an acceptable
// fallback since a zero-length grab vector carries no directional information.
export function angleFromCenter(center, point) {
  const dx = point.x - center.x;
  const dy = point.y - center.y;
  // Explicit zero-vector guard: Math.atan2(0, -0) is +PI (180deg) in JS, not 0 -- the
  // IEEE754 signed-zero quirk (-dy is negative zero when dy is +0) would otherwise silently
  // contradict this function's own documented degenerate-case behavior.
  if (!dx && !dy) return 0;
  const rad = Math.atan2(dx, -dy);
  const deg = (rad * 180) / Math.PI;
  return deg < 0 ? deg + 360 : deg;
}

// The unit "up" vector of an element rotated by `rotation` degrees -- where its own
// top-center now points on screen (world/screen space differ only by scale+offset, never a
// rotation, so this vector is valid in both). Used to place the rotate handle's stem a fixed
// SCREEN-space distance beyond the (rotated) top-center point. Inverse of angleFromCenter:
// angleFromCenter(c, {x: c.x + rotationUpVector(t).x, y: c.y + rotationUpVector(t).y}) === t.
export function rotationUpVector(rotation) {
  const rad = ((Number(rotation) || 0) * Math.PI) / 180;
  return { x: Math.sin(rad), y: -Math.cos(rad) };
}

// rotationFromDrag(center, startPoint, currentPoint, baseRotation, opts) -> degrees [0,360)
// The live rotation for a rotate-handle drag: `baseRotation` (the element's rotation when
// the drag was grabbed) plus the CHANGE in angle-around-center between the grab point and
// the current pointer -- not the current point's absolute angle alone, so grabbing the
// handle slightly off-center (any point near the knob, not its exact pixel) never snaps the
// element to a different starting angle. `opts.snap15` (Shift held) rounds the RESULT to the
// nearest 15-degree step, Figma-style; both the delta and the snap are recomputed fresh from
// the fixed grab point every call (no incremental accumulation), so a gesture is exact
// regardless of how many mousemove frames fired. Normalized to [0,360) like the op layer's
// own normalizeRotation (ops.mjs), so a live in-memory preview and the eventual committed
// patch agree before rounding.
export function rotationFromDrag(center, startPoint, currentPoint, baseRotation, opts = {}) {
  const delta = angleFromCenter(center, currentPoint) - angleFromCenter(center, startPoint);
  let rotation = (Number(baseRotation) || 0) + delta;
  rotation = ((rotation % 360) + 360) % 360;
  if (opts.snap15) rotation = (Math.round(rotation / 15) * 15) % 360;
  return rotation;
}

// pointInRotatedBox(world, box) -> boolean
// True when the world point falls inside `box` (an {x,y,w,h,rotation?} record -- an element
// OR a group, rotation absent/0 on the latter), rotation-aware: the world point is
// inverse-rotated into the box's own LOCAL frame about its center, then tested as a plain
// AABB -- the exact local-point formula README "Rotation & flip" promises for 3b's hit-test
// (the inverse of rotatedCorners' forward rotation). Absent/zero rotation takes a cheap
// identity fast path -- the plain AABB test every OTHER node in the scene still uses, so an
// unrotated element hit-tests byte-identically to before this increment.
export function pointInRotatedBox(world, box) {
  const x = Number(box.x) || 0;
  const y = Number(box.y) || 0;
  const w = Number(box.w) || 0;
  const h = Number(box.h) || 0;
  const rotation = Number(box.rotation) || 0;
  if (!rotation) return world.x >= x && world.x <= x + w && world.y >= y && world.y <= y + h;
  const cx = x + w / 2;
  const cy = y + h / 2;
  const rad = (rotation * Math.PI) / 180;
  const cos = Math.cos(rad);
  const sin = Math.sin(rad);
  const dx = world.x - cx;
  const dy = world.y - cy;
  const lx = dx * cos + dy * sin;
  const ly = -dx * sin + dy * cos;
  return Math.abs(lx) <= w / 2 && Math.abs(ly) <= h / 2;
}

// rotatedHandlePoints(box) -> SCALE_HANDLES shape, WORLD-space {key,fx,fy,cursor,x,y}[]
// The 8 scale-handle points on a node's OWN rotated frame (its 4 rotatedCorners plus the 4
// edge midpoints between adjacent corners) -- what a SOLO rotated element's handles sit on in
// the interactive gizmo (R4: "8 scale handles at rotatedCorners/edge-mids"), vs. the plain
// axis-aligned `box.x + box.w*fx` fan every OTHER selection (multi-select/group/unrotated
// element) still uses. `box` needs `rotation`; absent/0 collapses to the same 8 points the
// AABB fan would give (rotatedCorners' own identity fast path returns the plain box corners).
export function rotatedHandlePoints(box) {
  const [tl, tr, br, bl] = rotatedCorners(box);
  const mid = (a, b) => ({ x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 });
  const at = { nw: tl, n: mid(tl, tr), ne: tr, e: mid(tr, br), se: br, s: mid(br, bl), sw: bl, w: mid(bl, tl) };
  return SCALE_HANDLES.map((handle) => ({ ...handle, x: at[handle.key].x, y: at[handle.key].y }));
}

// resizeRotatedBox(box, handle, worldDelta, opts) -> {x, y, w, h, sx, sy}
// resizeBox's rotation-aware sibling (R3/R4): scaling a SOLO rotated element must keep its
// ROTATED anchor corner/edge fixed in WORLD space, not the plain unrotated one -- dragging a
// rotated box's "se" handle should grow it along ITS OWN tilted edges, with the opposite
// (rotated) "nw" corner pinned in place, not the AABB's nw corner. The trick: resize math is
// rotation-agnostic if done in the box's own LOCAL frame (centered on the box's rotation
// pivot, i.e. `{x:-w/2, y:-h/2, w, h}`), reusing resizeBox verbatim there -- its anchor
// semantics (opposite edge/corner fixed, or the center under `fromCenter`) are then
// automatically correct in WORLD space too, because rotating a rigid resized rectangle by the
// SAME fixed theta around the SAME fixed pivot preserves whatever stayed put locally. So:
// 1) rotate `worldDelta` into the local frame (inverse rotation -- pointInRotatedBox's same
// dx*cos+dy*sin / -dx*sin+dy*cos formula, applied to a VECTOR instead of a point); 2)
// resizeBox the local (center-relative) box with that local delta; 3) rotate the resized
// box's own new CENTER (an offset from the ORIGINAL pivot) forward back into world space
// (rotatedCorners' own forward formula); 4) re-derive the unrotated x/y/w/h element record
// from that new world center (element geometry is always stored unrotated -- rotation is
// applied at paint/hit-test time about the center). `box.rotation` absent/0 takes a fast path
// identical to calling resizeBox directly (verified in tests/rotate.test.mjs), so an
// unrotated element's scale gesture is untouched by this function's existence.
export function resizeRotatedBox(box, handle, worldDelta, opts = {}) {
  const rotation = Number(box.rotation) || 0;
  if (!rotation) {
    const resized = resizeBox(box, handle, worldDelta, opts);
    return { ...resized, sx: box.w !== 0 ? resized.w / box.w : 1, sy: box.h !== 0 ? resized.h / box.h : 1 };
  }
  const rad = (rotation * Math.PI) / 180;
  const cos = Math.cos(rad);
  const sin = Math.sin(rad);
  const localDelta = { dx: worldDelta.dx * cos + worldDelta.dy * sin, dy: -worldDelta.dx * sin + worldDelta.dy * cos };
  const localBox = { x: -box.w / 2, y: -box.h / 2, w: box.w, h: box.h };
  const resizedLocal = resizeBox(localBox, handle, localDelta, opts);
  const localCenter = { x: resizedLocal.x + resizedLocal.w / 2, y: resizedLocal.y + resizedLocal.h / 2 };
  const cx = box.x + box.w / 2;
  const cy = box.y + box.h / 2;
  // Forward rotation (rotatedCorners' own formula) of the local center OFFSET back to world.
  const worldCenter = { x: cx + localCenter.x * cos - localCenter.y * sin, y: cy + localCenter.x * sin + localCenter.y * cos };
  const w = resizedLocal.w;
  const h = resizedLocal.h;
  return {
    x: worldCenter.x - w / 2,
    y: worldCenter.y - h / 2,
    w,
    h,
    sx: box.w !== 0 ? w / box.w : 1,
    sy: box.h !== 0 ? h / box.h : 1,
  };
}
