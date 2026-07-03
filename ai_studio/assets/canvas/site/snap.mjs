// T0244 Figma-style smart guides + snap. Pure, dependency-free except tree.mjs (mirrors
// viewport.mjs). No DOM -- headless-testable with plain project objects, served statically
// over /ai_studio/ like the other site .mjs modules.
//
// PARITY STANCE (read first -- nobody invent an op): snapping is a UI drag-GESTURE aid, not
// an operation. The commit path is unchanged -- onMouseUp still rounds the final fractional
// world positions once and calls the EXISTING ops (elements-set / moveNodes / patchGroup).
// Ops receive FINAL POSITIONS ONLY and have no idea a snap happened. T0244 therefore adds NO
// CLI verb, NO HTTP route, NO ops.mjs surface, NO journal change. An agent calling
// moveNodes({moves:[{nodeId,x,y}]}) lands exactly where it asked; this module only helps a
// HUMAN pick those x/y before they commit. The feature lives entirely in site/ (this module
// plus the workspace.js drag wiring landing in increment 2).
import { childrenOf, descendantsOf, isNodeHidden, nodeScope, unionBBox } from "../tree.mjs";

// Figma-typical screen tolerance (5-8px). The caller divides this by viewport.scale to get
// the WORLD tolerance (zoom-aware): tight when zoomed in, generous when zoomed out. See the
// design doc's zoom-tolerance bound: scale is clamped [0.05,12] -> world tolerance ranges
// 6/12=0.5px (max zoom-in) .. 6/0.05=120px (max zoom-out).
export const SNAP_SCREEN_PX = 6;

// A candidate/matched alignment line.
//   vertical  : { pos = world X, min/max = world Y extent of the source box }
//   horizontal: { pos = world Y, min/max = world X extent of the source box }
// min/max are carried ONLY so a guide can be drawn spanning the source box.

function elementsArr(project) {
  return Array.isArray(project && project.elements) ? project.elements : [];
}

function groupsArr(project) {
  return Array.isArray(project && project.groups) ? project.groups : [];
}

// Resolve a dragged id to its stored node (element or group ref); null when unknown (a
// caller passing a stale/removed id just yields no scope contribution for that id).
function findNode(project, id) {
  return elementsArr(project).find((item) => item.id === id) || groupsArr(project).find((item) => item.id === id) || null;
}

function isGroupId(project, id) {
  return groupsArr(project).some((group) => group.id === id);
}

// The 3 vertical + 3 horizontal candidate lines a single box contributes (left/hcenter/right,
// top/vcenter/bottom). Reuses tree.mjs's unionBBox on a single-item array so the numeric
// coercion (Number(x)||0 etc.) is the SAME code path align/distribute already trust.
function linesFor(node) {
  const box = unionBBox([node]);
  const cx = box.minX + box.w / 2;
  const cy = box.minY + box.h / 2;
  return {
    vertical: [
      { pos: box.minX, min: box.minY, max: box.maxY },
      { pos: cx, min: box.minY, max: box.maxY },
      { pos: box.maxX, min: box.minY, max: box.maxY },
    ],
    horizontal: [
      { pos: box.minY, min: box.minX, max: box.maxX },
      { pos: cy, min: box.minX, max: box.maxX },
      { pos: box.maxY, min: box.minX, max: box.maxX },
    ],
  };
}

// collectSnapCandidates(project, draggedIds) -> { vertical: Line[], horizontal: Line[] }
// Called ONCE at drag start. draggedIds = the TOP-LEVEL dragged nodes (element ids + group
// ids). Excludes the dragged nodes AND every descendant of a dragged group (they move
// together, so they are never snap targets). Candidates =
//   - visible siblings in each dragged node's scope (childrenOf(scope) minus excluded), PLUS
//   - the parent group frame of each non-root scope (its own 6 lines: the "center in screen"
//     / "flush to frame edge" targets -- open Q1's recommended default: ship all 6).
// Hidden nodes (isNodeHidden, including under a hidden ancestor) are skipped -- this applies
// to sibling candidates AND to a scope's own frame (a hidden group contributes no frame). A
// scope shared by multiple dragged ids is visited (and its frame added) only once.
export function collectSnapCandidates(project, draggedIds) {
  const ids = Array.isArray(draggedIds) ? draggedIds : [];

  const excluded = new Set(ids);
  for (const id of ids) {
    if (!isGroupId(project, id)) continue;
    const { elements, groups } = descendantsOf(project, id);
    for (const element of elements) excluded.add(element.id);
    for (const group of groups) excluded.add(group.id);
  }

  const scopes = new Set();
  for (const id of ids) {
    const node = findNode(project, id);
    if (node) scopes.add(nodeScope(project, node));
  }

  const vertical = [];
  const horizontal = [];
  const pushLines = (node) => {
    const lines = linesFor(node);
    vertical.push(...lines.vertical);
    horizontal.push(...lines.horizontal);
  };

  for (const scope of scopes) {
    const { elements, groups } = childrenOf(project, scope);
    for (const node of [...elements, ...groups]) {
      if (excluded.has(node.id) || isNodeHidden(project, node)) continue;
      pushLines(node);
    }
    if (scope != null) {
      const frame = groupsArr(project).find((group) => group.id === scope);
      if (frame && !isNodeHidden(project, frame)) pushLines(frame);
    }
  }

  return { vertical, horizontal };
}

// Scan `lines` for the nearest one within `tol` of ANY of the three probes (p0/p1/p2 -- the
// dragged bbox's min/center/max on this axis). Three locals, no probe-array allocation (the
// hot path runs once per mousemove over precomputed candidate arrays). Returns null when
// nothing on this axis is within tolerance.
function bestMatch(lines, p0, p1, p2, tol) {
  let best = null;
  for (const line of lines) {
    let delta = line.pos - p0;
    let dist = Math.abs(delta);
    if (dist <= tol && (best === null || dist < best.dist)) best = { dist, delta, line };
    delta = line.pos - p1;
    dist = Math.abs(delta);
    if (dist <= tol && (best === null || dist < best.dist)) best = { dist, delta, line };
    delta = line.pos - p2;
    dist = Math.abs(delta);
    if (dist <= tol && (best === null || dist < best.dist)) best = { dist, delta, line };
  }
  return best;
}

// snapDelta(dragBBox, candidates, toleranceWorld) -> { dx, dy, guides }
// dragBBox = { x, y, w, h } -- the RAW (unsnapped) union bbox of the dragged selection THIS
// frame. Probes X = [minX, cx, maxX]; probes Y = [minY, cy, maxY]. Per axis independently:
// over all (probe x candidate-line-on-that-axis) pairs, keep the one with the smallest
// |line.pos - probe| that is <= toleranceWorld (nearest wins; boundary delta == tol snaps).
// dx/dy = that signed correction (may be 0 when already exactly aligned -- a match still
// emits a guide). No match on an axis => that axis contributes 0 and no guide. Both axes are
// resolved BEFORE guide extents are built (see below), so a guide's perpendicular span always
// reflects the FINAL snapped bbox, never an intermediate one.
export function snapDelta(dragBBox, candidates, toleranceWorld) {
  const minX = dragBBox.x;
  const maxX = dragBBox.x + dragBBox.w;
  const cx = minX + dragBBox.w / 2;
  const minY = dragBBox.y;
  const maxY = dragBBox.y + dragBBox.h;
  const cy = minY + dragBBox.h / 2;

  const bestX = bestMatch(candidates && candidates.vertical ? candidates.vertical : [], minX, cx, maxX, toleranceWorld);
  const bestY = bestMatch(candidates && candidates.horizontal ? candidates.horizontal : [], minY, cy, maxY, toleranceWorld);

  const dx = bestX ? bestX.delta : 0;
  const dy = bestY ? bestY.delta : 0;

  // Guide extents: the source line's own extent UNIONED with the SNAPPED dragged bbox's
  // extent on the perpendicular axis, so the guide visibly connects target and dragged
  // object. Built only after both dx and dy are resolved above (§9 risk note: get this
  // ordering right or the guide's other end lags one axis behind).
  const guides = [];
  if (bestX) {
    guides.push({
      axis: "x",
      pos: bestX.line.pos,
      min: Math.min(bestX.line.min, minY + dy),
      max: Math.max(bestX.line.max, maxY + dy),
    });
  }
  if (bestY) {
    guides.push({
      axis: "y",
      pos: bestY.line.pos,
      min: Math.min(bestY.line.min, minX + dx),
      max: Math.max(bestY.line.max, maxX + dx),
    });
  }

  return { dx, dy, guides };
}
