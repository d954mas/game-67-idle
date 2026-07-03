// Canvas viewport math: pure pan/zoom/transform helpers shared by the site
// modules (workspace, actions, dnd, regions). Screen<->image point conversion,
// fit-to-frame centering, and zoom-at-cursor that keeps the point under the
// cursor stable. No DOM. Rect/polygon editing geometry is NOT here -- the region
// workbench owns that in regions.js. Moved into the canvas module from the
// retired asset_tools editor so the canvas owns its own viewport code.
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
