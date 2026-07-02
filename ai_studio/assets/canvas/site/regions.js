// Region workbench geometry (increment 6). Pure, DOM-free helpers shared by the
// workspace: convert an element's source-pixel region rects to world/screen boxes,
// hit-test region bodies and resize handles, and draw the bright numbered overlays.
// The pointer state machine lives in workspace.js; every persisted change still
// goes through the shared setRegions op. No business logic beyond geometry/drawing.
import { imageToScreenPoint } from "./viewport.mjs";

// The editable rect is the padded slice rect; fall back to the tight content_bbox
// for older/foreign regions that only carry that.
export function regionRect(region) {
  return region.rect || region.content_bbox || null;
}

// Element display box may be resized away from its source pixels, so map source
// coordinates through these factors (usually 1).
export function scaleFactors(element) {
  return {
    sx: element.w / (element.source_w || element.w),
    sy: element.h / (element.source_h || element.h),
  };
}

// World-space AABB of a region on its element.
export function regionWorldRect(element, region) {
  const rect = regionRect(region);
  if (!rect) return null;
  const { sx, sy } = scaleFactors(element);
  return { x: element.x + rect[0] * sx, y: element.y + rect[1] * sy, w: rect[2] * sx, h: rect[3] * sy };
}

// Screen-space (CSS px) box of a region under the given viewport.
export function regionScreenRect(element, region, vp) {
  const world = regionWorldRect(element, region);
  if (!world) return null;
  const p = imageToScreenPoint({ x: world.x, y: world.y }, vp);
  return { x: p.x, y: p.y, w: world.w * vp.scale, h: world.h * vp.scale };
}

// ---- polygonal regions -------------------------------------------------------
// A region is polygonal when it carries a `polygon` ring of >=3 [x, y] source-pixel
// pairs (no `shape` field — the discriminator matches ops.setRegions and the slicer).

export function regionHasPolygon(region) {
  return Boolean(region && Array.isArray(region.polygon) && region.polygon.length >= 3);
}

// World-space points {x, y} of a region's polygon on its element (or null).
export function regionWorldPolygon(element, region) {
  if (!regionHasPolygon(region)) return null;
  const { sx, sy } = scaleFactors(element);
  return region.polygon.map((p) => ({ x: element.x + Number(p[0]) * sx, y: element.y + Number(p[1]) * sy }));
}

// Screen-space points {x, y} of a region's polygon under the viewport (or null).
export function regionScreenPolygon(element, region, vp) {
  const world = regionWorldPolygon(element, region);
  if (!world) return null;
  return world.map((p) => imageToScreenPoint(p, vp));
}

// Even-odd point-in-polygon over an array of {x, y}. Affine-invariant, so it is run in
// world space; used to hit-test polygonal region bodies instead of a plain rect.
function pointInPolygon(point, ring) {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const xi = ring[i].x;
    const yi = ring[i].y;
    const xj = ring[j].x;
    const yj = ring[j].y;
    const denom = yj - yi || 1e-9;
    if (((yi > point.y) !== (yj > point.y)) && point.x < ((xj - xi) * (point.y - yi)) / denom + xi) {
      inside = !inside;
    }
  }
  return inside;
}

// Proportionally rescale a polygon's points from oldRect to newRect (ports the legacy
// transformPolygon). Both rects are [x, y, w, h] in source pixels; returns rounded int
// points (or null when there is nothing to transform).
export function transformPolygon(points, oldRect, newRect) {
  if (!Array.isArray(points) || points.length < 3) return null;
  const [ox, oy, ow, oh] = oldRect;
  const [nx, ny, nw, nh] = newRect;
  const scaleX = nw / Math.max(1, ow);
  const scaleY = nh / Math.max(1, oh);
  return points.map((p) => [
    Math.round(nx + (Number(p[0]) - ox) * scaleX),
    Math.round(ny + (Number(p[1]) - oy) * scaleY),
  ]);
}

// Axis-aligned bbox [x, y, w, h] of an integer polygon (page-side mirror of the op's
// rectFromPolygon; the op re-derives the stored rect, this is for the live draft/finish).
export function polygonBBox(points) {
  const xs = points.map((p) => Number(p[0]));
  const ys = points.map((p) => Number(p[1]));
  const x = Math.floor(Math.min(...xs));
  const y = Math.floor(Math.min(...ys));
  return [x, y, Math.max(1, Math.ceil(Math.max(...xs)) - x), Math.max(1, Math.ceil(Math.max(...ys)) - y)];
}

// Topmost region containing the point (last in array = topmost): point-in-polygon for
// polygonal regions, rect AABB otherwise.
export function hitRegion(world, element) {
  const regions = element.regions || [];
  for (let i = regions.length - 1; i >= 0; i -= 1) {
    const region = regions[i];
    if (regionHasPolygon(region)) {
      const ring = regionWorldPolygon(element, region);
      if (ring && pointInPolygon(world, ring)) return region;
    } else {
      const r = regionWorldRect(element, region);
      if (r && world.x >= r.x && world.x <= r.x + r.w && world.y >= r.y && world.y <= r.y + r.h) return region;
    }
  }
  return null;
}

// Trace a screen-space polygon ({x, y} points) as a closed path on ctx.
function tracePolygonPath(ctx, points) {
  ctx.beginPath();
  ctx.moveTo(points[0].x, points[0].y);
  for (let i = 1; i < points.length; i += 1) ctx.lineTo(points[i].x, points[i].y);
  ctx.closePath();
}

// Eight resize handles (corners + edges) as fractional positions with a cursor.
export const REGION_HANDLES = [
  { key: "nw", fx: 0, fy: 0, cursor: "nwse-resize" },
  { key: "n", fx: 0.5, fy: 0, cursor: "ns-resize" },
  { key: "ne", fx: 1, fy: 0, cursor: "nesw-resize" },
  { key: "e", fx: 1, fy: 0.5, cursor: "ew-resize" },
  { key: "se", fx: 1, fy: 1, cursor: "nwse-resize" },
  { key: "s", fx: 0.5, fy: 1, cursor: "ns-resize" },
  { key: "sw", fx: 0, fy: 1, cursor: "nesw-resize" },
  { key: "w", fx: 0, fy: 0.5, cursor: "ew-resize" },
];

const HANDLE_HALF = 4; // half a handle square, CSS px
const HANDLE_TOL = 6; // hit tolerance around a handle centre, CSS px

function handlePoints(box) {
  return REGION_HANDLES.map((handle) => ({
    ...handle,
    x: box.x + box.w * handle.fx,
    y: box.y + box.h * handle.fy,
  }));
}

// Hit a resize handle of any currently-selected region (screen space). Returns
// { region, handle } or null. Checked before region bodies so edges stay grabbable.
export function hitRegionHandle(screen, element, selectedIds, vp) {
  const regions = (element.regions || []).filter((region) => selectedIds.has(region.id));
  for (let i = regions.length - 1; i >= 0; i -= 1) {
    const box = regionScreenRect(element, regions[i], vp);
    if (!box) continue;
    for (const point of handlePoints(box)) {
      if (Math.abs(screen.x - point.x) <= HANDLE_TOL && Math.abs(screen.y - point.y) <= HANDLE_TOL) {
        return { region: regions[i], handle: point };
      }
    }
  }
  return null;
}

function drawBadge(ctx, x, y, text, selected, interactive = true) {
  const width = Math.max(14, Math.ceil(ctx.measureText(text).width) + 8);
  const height = 15;
  ctx.globalAlpha = interactive ? 1 : 0.75;
  ctx.fillStyle = selected ? "#d7a14a" : "#2b8f86";
  ctx.fillRect(x, y, width, height);
  ctx.fillStyle = "#f8fbff";
  ctx.fillText(text, x + 4, y + height / 2 + 0.5);
  ctx.globalAlpha = 1;
}

// Draw an element's regions as numbered overlays. `interactive` (the isolated mode
// B element) uses strong strokes, a translucent fill, per-region selection
// highlight, and resize handles on selected regions. Otherwise (mode A) they render
// as a thin, passive, fill-less numbered HINT — visible but never inviting a drag.
export function drawRegionsOverlay(ctx, element, vp, { selectedRegionIds, interactive } = {}) {
  const regions = element.regions || [];
  if (!regions.length) return;
  ctx.save();
  ctx.font = "11px system-ui, 'Segoe UI', sans-serif";
  ctx.textBaseline = "middle";
  for (let i = 0; i < regions.length; i += 1) {
    const box = regionScreenRect(element, regions[i], vp);
    if (!box) continue;
    const selected = Boolean(interactive && selectedRegionIds && selectedRegionIds.has(regions[i].id));
    // Polygonal regions render as their stroked ring (same fill/selection colors), not
    // just the bbox; a plain region renders as its rect.
    const poly = regionScreenPolygon(element, regions[i], vp);
    if (interactive) {
      ctx.fillStyle = selected ? "rgba(215, 161, 74, 0.24)" : "rgba(63, 199, 186, 0.14)";
      ctx.lineWidth = selected ? 2 : 1.5;
      ctx.strokeStyle = selected ? "#ffca6a" : "#3fc7ba";
    } else {
      // Passive hint: no fill, thin dimmer stroke.
      ctx.lineWidth = 1;
      ctx.strokeStyle = "rgba(63, 199, 186, 0.6)";
    }
    if (poly) {
      tracePolygonPath(ctx, poly);
      if (interactive) ctx.fill();
      ctx.stroke();
    } else {
      if (interactive) ctx.fillRect(box.x, box.y, box.w, box.h);
      ctx.strokeRect(box.x + 0.5, box.y + 0.5, Math.max(0, box.w - 1), Math.max(0, box.h - 1));
    }
    // Badge shows the region's NAME (detect assigns "<element> 1..N"); the bare
    // index is only the fallback for unnamed regions. Long names truncate.
    const label = (regions[i].name || "").trim() || String(i + 1);
    drawBadge(ctx, box.x, box.y, label.length > 20 ? `${label.slice(0, 19)}…` : label, selected, interactive);
    if (selected) {
      ctx.lineWidth = 1;
      for (const point of handlePoints(box)) {
        ctx.fillStyle = "#ffca6a";
        ctx.fillRect(point.x - HANDLE_HALF, point.y - HANDLE_HALF, HANDLE_HALF * 2, HANDLE_HALF * 2);
        ctx.strokeStyle = "#1a1f2b";
        ctx.strokeRect(point.x - HANDLE_HALF, point.y - HANDLE_HALF, HANDLE_HALF * 2, HANDLE_HALF * 2);
      }
      // Vertex dots for a selected polygon (not individually draggable — bbox handles
      // rescale the whole ring, matching the legacy editor).
      if (poly) {
        for (const point of poly) {
          ctx.beginPath();
          ctx.arc(point.x, point.y, 3.5, 0, Math.PI * 2);
          ctx.fillStyle = "#d7a14a";
          ctx.fill();
          ctx.strokeStyle = "#1a1f2b";
          ctx.stroke();
        }
      }
    }
  }
  ctx.restore();
}

// Draw the in-progress polygon draft on the isolated element: dashed edges (auto-
// closing at >=3 points), a live rubber segment to the hover point, and a dot at each
// placed vertex. `draft`/`hover` are SOURCE-pixel coordinates on `element`.
export function drawPolygonDraft(ctx, element, draft, hover, vp) {
  if (!draft || !draft.length) return;
  const { sx, sy } = scaleFactors(element);
  const toScreen = (p) => imageToScreenPoint({ x: element.x + Number(p[0]) * sx, y: element.y + Number(p[1]) * sy }, vp);
  const placed = draft.map(toScreen);
  const chain = hover ? [...placed, toScreen([hover.x, hover.y])] : placed;
  ctx.save();
  ctx.setLineDash([7, 5]);
  ctx.lineWidth = 2;
  ctx.strokeStyle = "#d7a14a";
  ctx.fillStyle = "rgba(215, 161, 74, 0.14)";
  if (chain.length >= 2) {
    ctx.beginPath();
    ctx.moveTo(chain[0].x, chain[0].y);
    for (let i = 1; i < chain.length; i += 1) ctx.lineTo(chain[i].x, chain[i].y);
    if (draft.length >= 3) ctx.closePath();
    ctx.stroke();
    if (draft.length >= 3) ctx.fill();
  }
  ctx.setLineDash([]);
  for (const point of placed) {
    ctx.beginPath();
    ctx.arc(point.x, point.y, 4, 0, Math.PI * 2);
    ctx.fillStyle = "#f8fbff";
    ctx.fill();
    ctx.strokeStyle = "#1a1f2b";
    ctx.stroke();
  }
  ctx.restore();
}

// A fresh region id for a rubber-banded region (browser crypto, with a fallback).
export function newRegionId() {
  const rand = typeof crypto !== "undefined" && crypto.randomUUID
    ? crypto.randomUUID().slice(0, 8)
    : Math.random().toString(36).slice(2, 10);
  return `region_${rand}`;
}
