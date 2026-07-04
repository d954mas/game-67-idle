// Workspace view: the pan/zoom canvas, its crisp DPR-aware rendering, tool rail,
// zoom controls, top bar sync, and all pointer interaction. Geometry is reused
// from the canvas viewport module (viewport.mjs) and the region helpers in regions.js. Every
// persisted change goes through the shared actions/API; this module only renders
// and turns input into those calls.
//
// Pointer model (increment 6):
//   * Panning is EXCLUSIVELY Hand tool / Space-hold / middle-mouse.
//   * Select tool, empty canvas drag -> marquee element select (Shift adds).
//   * A single selected element shows its regions as bright numbered overlays;
//     clicking a region selects it, dragging moves it, corner/edge handles resize,
//     and (while a region is selected) dragging the element's empty area rubber-
//     bands a NEW region. Every region gesture commits ONCE via setRegions.
//   * A canvas drag NEVER changes group membership (lead 2026-07-02): positions persist
//     ONCE on mouseup as one batched op (no mid-drag journal spam); joining/leaving a
//     group is explicit only (layers drag, Ctrl+G, Ungroup, CLI group-assign).
import {
  api,
  applyMutation,
  el,
  elements,
  enterRegionEdit,
  exitRegionEdit,
  groupById,
  groups,
  hooks,
  isElementHidden,
  isSelected,
  imageFor,
  refresh,
  regionEditElement,
  selectedElements,
  selectGroupOnly,
  selectOnly,
  setStatus,
  state,
  syncPrimaryGroup,
  toggleSelect,
} from "./app.js";
import { addNoteAt, addTextAt, moveNodesTo, patchElementBox, patchNoteContent, patchTextElement, renameProject, setRegionsFor, undo, redo } from "./actions.js";
import { canvasFontString, NOTE_PADDING } from "../fonts.mjs";
// T0233: the same pure 9-slice math ops.mjs/render_group.py use (see slice9.mjs) —
// served over /ai_studio/ so the page normalizes a slice9 element identically.
import { slice9Patches } from "../slice9.mjs";
// T0260 increment 2: the SAME pure sampler ops.mjs/the PIL bake use (see animation.mjs) —
// the on-canvas preview samples it every rAF so a channel animates identically here and in
// the bake. View-state only; nothing here writes the store.
import { sampleAnimation } from "../animation.mjs";
import { areFontsReady, measureTextBox, measureTextLines, wrapNoteLines } from "./fonts.js";
import { inlineEdit } from "./inline.js";
import { openContextMenu } from "./context_menu.js";
import {
  drawPolygonDraft,
  drawRegionsOverlay,
  hitRegion,
  hitRegionHandle,
  newRegionId,
  polygonBBox,
  regionRect,
  scaleFactors,
  transformPolygon,
} from "./regions.js";
import {
  clamp,
  dragWorldDelta,
  fitViewport,
  imageToScreenPoint,
  mapItemBox,
  panOffsetFor,
  pointInRotatedBox,
  resizeBox,
  resizeRotatedBox,
  rotatedHandlePoints,
  rotationFromDrag,
  rotationUpVector,
  scaledFontSize,
  screenToImagePoint,
  SCALE_HANDLES,
  zoomViewportAt,
} from "./viewport.mjs";
import { ancestorsOf, childrenOf, descendantsOf, isNodeHidden, isNodeTransformed, nodeScope, orderedChildren, rotatedCorners, scaleGroupMoves, unionBBox } from "../tree.mjs";
// T0244 smart guides: pure snap math, no DOM (see snap.mjs's own header). Imported here only
// -- the drag-lifetime precompute + mousemove application + guide overlay all live in this
// file; ops.mjs/api.mjs/cli.mjs are untouched (snapping commits through the EXISTING ops).
import { SNAP_SCREEN_PX, collectSnapCandidates, snapDelta } from "./snap.mjs";

let canvas = null;
let ctx = null;
let groupLabelRects = []; // {groupId, x, y, w, h} in screen (CSS) space, per render
// Bounding rects cached for the duration of an active drag so the hot move path never
// forces a synchronous layout. render() writes DOM (zoom chip / breadcrumb / textarea),
// so a getBoundingClientRect READ in the following mousemove would flush a full reflow
// every frame (read-after-write thrash) — the drag's main perf cost besides repaint. The
// stage/canvas never resize mid-drag, so one capture at grab time is exact; both caches
// are cleared on mouseup and on window resize (the only mid-drag geometry change).
let dragCanvasRect = null; // canvas rect, used by pointer()
let dragStageRect = null; // stage rect, used by resizeCanvas()

// ---- crisp DPR-aware sizing --------------------------------------------------

function resizeCanvas() {
  const stage = el("stage");
  const rect = dragStageRect || stage.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  state.cssWidth = Math.max(1, Math.floor(rect.width));
  state.cssHeight = Math.max(1, Math.floor(rect.height));
  // Backing store is scaled by devicePixelRatio; CSS keeps the element at rect
  // size (width/height:100%). Assigning canvas.width/height REALLOCATES and clears
  // the backing store even when the value is unchanged, so only assign when the
  // stage size or DPR actually changed — otherwise every drag frame paid a needless
  // realloc + clear. setTransform is cheap and re-applied each render to stay correct.
  const backingW = Math.max(1, Math.round(state.cssWidth * dpr));
  const backingH = Math.max(1, Math.round(state.cssHeight * dpr));
  if (canvas.width !== backingW || canvas.height !== backingH) {
    canvas.width = backingW;
    canvas.height = backingH;
  }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}

// ---- rendering ---------------------------------------------------------------

// Coalesce burst render() calls (many mousemoves in one frame during a drag) into a
// SINGLE repaint per animation frame. render() reads live state, so the frame always
// paints the latest positions; extra calls within the same frame are dropped.
let renderScheduled = false;
export function requestRender() {
  if (renderScheduled) return;
  renderScheduled = true;
  requestAnimationFrame(() => {
    renderScheduled = false;
    render();
  });
}

// ---- viewport culling (T0255) ------------------------------------------------
//
// The paint loop skips any element or group whose CONSERVATIVE screen-space AABB lies fully
// outside the canvas rect grown by CULL_MARGIN. Everything here works in the SAME CSS-px space
// the paint coords live in (state.cssWidth/Height; DPR is folded into the base transform, so the
// visible rect is always [0,0,cssW,cssH]). Culling removes ONLY draws that could not touch a
// visible pixel, so it is byte-for-byte invisible. The margin absorbs fixed-screen-size chrome
// that overhangs a box (a 2px selection stroke, the group name pill ~18px above the frame) and
// any small text-metric drift, so nothing poking just past an edge is ever wrongly dropped.
const CULL_MARGIN = 64;

// True when the screen AABB [x0,y0,x1,y1] cannot intersect the viewport grown by CULL_MARGIN.
function screenAABBOffscreen(x0, y0, x1, y1) {
  return (
    x1 < -CULL_MARGIN ||
    y1 < -CULL_MARGIN ||
    x0 > state.cssWidth + CULL_MARGIN ||
    y0 > state.cssHeight + CULL_MARGIN
  );
}

// Conservative screen AABB of an ELEMENT's static footprint. Rotation: the four rotatedCorners
// (world) mapped to screen, min/max -> the tight rotated AABB (exact, still cheap). Unrotated
// (the common case) takes a single origin transform + w/h*scale. Flip mirrors WITHIN the box
// (no bounds change); slice9 draws within the box; the not-yet-loaded placeholder strokeRect
// and the cleanup-preview substitution both draw in the SAME box; a text element's auto-width
// w/h is kept == its live measure by patchTextElement (actions.js), so the stored box is the
// drawn box. The ONE draw that escapes this box is the animation preview (a sampled offset/
// scale about the center) — previewing elements are never culled (see elementCullable), so
// this box deliberately ignores it.
function elementScreenAABB(element, vp) {
  const rotation = Number(element.rotation) || 0;
  if (rotation) {
    let x0 = Infinity;
    let y0 = Infinity;
    let x1 = -Infinity;
    let y1 = -Infinity;
    for (const corner of rotatedCorners(element)) {
      const p = imageToScreenPoint(corner, vp);
      if (p.x < x0) x0 = p.x;
      if (p.x > x1) x1 = p.x;
      if (p.y < y0) y0 = p.y;
      if (p.y > y1) y1 = p.y;
    }
    return { x0, y0, x1, y1 };
  }
  const origin = imageToScreenPoint({ x: element.x, y: element.y }, vp);
  return { x0: origin.x, y0: origin.y, x1: origin.x + element.w * vp.scale, y1: origin.y + element.h * vp.scale };
}

// Whether an element can be skipped this frame. NEVER culls an animation-previewing element
// (its draw can travel far outside the static box via the sampled offset/scale; previewing ids
// are few and the preview rAF already repaints every frame, so keeping them is correct and
// negligible) nor the region-edit isolated element (its interactive region overlay/handles are
// pinned to it — keeping exactly that one element sidesteps any overlay-vs-cull edge).
function elementCullable(element, vp, editEl) {
  if (previewingElementIds.has(element.id)) return false;
  if (editEl && element.id === editEl.id) return false;
  const aabb = elementScreenAABB(element, vp);
  return screenAABBOffscreen(aabb.x0, aabb.y0, aabb.x1, aabb.y1);
}

// Conservative screen AABB of a GROUP's own box — the clip rect, the background fill and the
// frame rect are all this same rect (groups never rotate). Backs the pass-1 clip/background
// decision (raw box) and, grown, the pass-2 chrome cull.
function groupBoxScreenAABB(group, vp) {
  const origin = imageToScreenPoint({ x: group.x, y: group.y }, vp);
  return { x0: origin.x, y0: origin.y, x1: origin.x + group.w * vp.scale, y1: origin.y + group.h * vp.scale };
}

// Conservative screen AABB of a group's pass-2 CHROME (drawGroupFrame): the box grown RIGHT by
// a cheap upper bound on the name pill / card chip / prompt-preview width — those anchor at the
// frame's top-left and can extend past a NARROW frame's right edge (a short frame, a long name).
// They never overhang the frame's LEFT or BOTTOM, and the pill's ~18px rise above the top is
// already covered by CULL_MARGIN, so only the right edge needs widening. Width is bounded by
// string length x a generous per-char px (never ctx.measureText — the cull must cost less than
// the draws it removes; over-estimating only makes it skip strictly less).
function groupChromeScreenAABB(group, vp) {
  const box = groupBoxScreenAABB(group, vp);
  // 12px pill font, wide-glyph upper bound ~12px/char + pill padding.
  let chromeW = String(group.name || "Group").length * 12 + 12;
  // A recipe/style card adds a chip beside the pill (+gap) and an 11px prompt preview of up to
  // RECIPE_PROMPT_PREVIEW_MAX chars inside the frame.
  if (group.recipe || group.style) chromeW = Math.max(chromeW + 4 + 90, RECIPE_PROMPT_PREVIEW_MAX * 11 + 14);
  return { x0: box.x0, y0: box.y0, x1: Math.max(box.x1, box.x0 + chromeW), y1: box.y1 };
}

export function render() {
  if (!canvas || !state.project) return;
  resizeCanvas();
  const vp = state.viewport;
  // Clear in DEVICE pixels, not CSS pixels: with a fractional devicePixelRatio
  // (Windows display scaling) a CSS-space clearRect ends on a fractional device
  // row and never fully clears the last one — drag frames that painted into that
  // strip leave a 1-2px residue at the canvas edge until a page reload.
  ctx.save();
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.restore();
  // Crisp sprite pixels when zoomed in (>= 2x); smooth when zoomed out.
  ctx.imageSmoothingEnabled = vp.scale < 2;

  const editEl = regionEditElement(); // mode B element, or null (mode A)
  // Pass 1 — artwork: recurse the scene tree from root in computed z-order, so a
  // group's members paint as one contiguous band and a group's background fills
  // behind its children. No clip yet (that is increment 4) — recursion order + fill.
  paintScope(null, vp, editEl);

  // Pass 2 — chrome overlay: group frame borders + labels (+ selection) drawn on top
  // of ALL artwork; groupLabelRects feeds label hit-testing (unchanged).
  groupLabelRects = [];
  for (const group of groups()) {
    if (isNodeHidden(state.project, group)) continue;
    // T0255: skip a group whose entire pass-2 chrome (frame + name pill + card chip/preview)
    // lies off the viewport. A culled frame pushes no groupLabelRects entry — correct, since an
    // offscreen label is never under the cursor, so it was never label-hit-testable anyway.
    const chrome = groupChromeScreenAABB(group, vp);
    if (screenAABBOffscreen(chrome.x0, chrome.y0, chrome.x1, chrome.y1)) continue;
    drawGroupFrame(group, vp);
  }
  // Ghost the clipped-out part of any selected element (or selected group's members) so a
  // clipped sprite never reads as "lost". Chrome pass = never affects the artwork pixels.
  drawClipGhosts(vp);
  drawGestureOverlay();
  drawSnapGuides(vp);
  // T0232 increment 2: 8 resize handles on the current selection's AABB -- topmost chrome,
  // drawn last so a handle is never occluded by a frame/guide.
  drawSelectionHandles(vp);
  // Live polygon draft on the isolated element (page-only state, never journaled).
  if (editEl && state.regionTool === "polygon" && state.polygonDraft.length) {
    drawPolygonDraft(ctx, editEl, state.polygonDraft, state.polygonHover, vp);
  }
  updateBreadcrumb(editEl);
  updateScopeBreadcrumb(editEl);
  updateRegionTools(editEl);
  updateZoomIndicator();
  updateEmptyHint();
  syncTextEditor();
}

// A breadcrumb chip for the entered scope (Figma "Screen ▸ Button — Esc to exit"), so
// the user always knows which group's interior clicks resolve into. Shown only outside
// region-edit (the region breadcrumb owns that state). Reuses the chip visual language.
function updateScopeBreadcrumb(editEl) {
  const node = el("scope-breadcrumb");
  if (!node) return;
  if (!editEl && state.enteredGroupId) {
    const parts = [];
    let cur = state.enteredGroupId;
    let guard = 0;
    while (cur && guard < 64) {
      const group = groupById(cur);
      if (!group) break;
      parts.unshift(group.name || "Group");
      cur = group.parentId || null;
      guard += 1;
    }
    node.textContent = `${parts.join(" ▸ ")} — Esc to exit`;
    node.classList.remove("hidden");
  } else {
    node.classList.add("hidden");
  }
}

// Recursive artwork paint for one scope (null = root): each child in computed
// back-to-front order — an element draws in place; a visible group fills its
// background then recurses into its own children. A group with clip=true pushes a
// rectangular clip region (its screen box) before painting its background + children
// and pops it after, so members outside the frame are cropped; nested clips intersect
// naturally through the canvas clip stack. Chrome (frames/labels/ghost hints) is pass 2,
// outside every clip, so an empty or overflowing screen is never hidden.
function paintScope(scopeId, vp, editEl) {
  for (const child of orderedChildren(state.project, scopeId)) {
    if (isNodeHidden(state.project, child.ref)) continue;
    if (child.kind === "group") {
      const group = child.ref;
      const clip = group.clip === true;
      // T0255: the clip rect, the background fill, and the frame all share the group's box.
      const box = groupBoxScreenAABB(group, vp);
      const boxOffscreen = screenAABBOffscreen(box.x0, box.y0, box.x1, box.y1);
      // A clip=true group crops EVERY descendant to its box; when that box is fully offscreen
      // the whole subtree (background + children, nested clips included) is cropped away and
      // paints no visible pixel — skip clip setup, background, and recursion in one shot. A
      // NON-clip group's offscreen box implies nothing about its children (they keep their own
      // positions), so it still recurses and each child is culled on its own below.
      if (clip && boxOffscreen) continue;
      if (clip) {
        ctx.save();
        ctx.beginPath();
        ctx.rect(box.x0, box.y0, group.w * vp.scale, group.h * vp.scale);
        ctx.clip();
      }
      // The background IS the group box; an offscreen box fills nothing visible (reached only by
      // a non-clip group here — a clip group with an offscreen box already `continue`d above).
      if (!boxOffscreen) fillGroupBackground(group, vp);
      paintScope(child.id, vp, editEl);
      if (clip) ctx.restore();
    } else {
      if (elementCullable(child.ref, vp, editEl)) continue;
      paintElement(child.ref, vp, editEl);
    }
  }
}

// ---- cleanup preview (T0207) --------------------------------------------------
//
// View-state-only on-canvas preview for the inspector's Quantize/Denoise controls —
// same shape as the Alt-hold clip-ghost peek (state.clipGhostPeek): NEVER journaled or
// persisted, a paint-time substitution only. Kept as MODULE-level state here (paintElement
// below is the only reader) rather than on app.js's shared `state` object — the inspector
// drives it entirely through the setters below, which own the repaint themselves (this
// file calls render() directly internally, the same as every other page-state change in
// here; hooks.renderCanvas() is only how code OUTSIDE this module reaches it).
// { elementId, bitmap, tool, params, report } | null. `bitmap` is an already-loaded <img>
// (see loadCleanupBitmap) so paintElement's existing `img.complete` gate just works.
let cleanupPreview = null;
// While true (the inspector's "Hold to compare" button held down), paint the element's
// real source instead of the preview bitmap for this frame — the preview itself is
// untouched, this only flips which image paintElement picks.
let cleanupPreviewCompare = false;

export function getCleanupPreview() {
  return cleanupPreview;
}

export function setCleanupPreview(preview) {
  cleanupPreview = preview;
  cleanupPreviewCompare = false;
  render();
}

export function clearCleanupPreview() {
  if (!cleanupPreview) return;
  cleanupPreview = null;
  cleanupPreviewCompare = false;
  render();
}

// `active` mirrors the compare button's pressed state (mousedown/touchstart..mouseup/
// mouseleave/touchend); a no-op when there is nothing to compare against (the button is
// hidden in that case anyway, but a stray event should never resurrect a cleared preview).
export function setCleanupPreviewCompare(active) {
  if (!cleanupPreview || cleanupPreviewCompare === active) return;
  cleanupPreviewCompare = active;
  render();
}

// Decode a cleanup-preview PNG (base64, from the cleanup-preview API response) into an
// <img> ready for ctx.drawImage — the same Image() decode path app.js's imageFor uses,
// just keyed by content instead of a project-file URL (a preview is never written to the
// store, so it has no src of its own).
export function loadCleanupBitmap(base64) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = () => reject(new Error("could not decode the cleanup preview image"));
    img.src = `data:image/png;base64,${base64}`;
  });
}

// ---- animation preview (T0260 increment 2) ------------------------------------
//
// On-canvas playback of an element's procedural animation (element.animation), sampled by
// the SHARED pure animation.mjs so the page matches the eventual PIL bake byte-for-byte. Pure
// VIEW-STATE like the cleanup preview above: NOTHING is journaled/persisted — the sample is a
// paint-time transform (offset/rot/scale about the box center + an opacity multiply) applied
// ONLY to the drawn image/glyphs. Element geometry is NEVER mutated, so hit-testing, selection
// outlines, the gizmo, guides and export all keep using the STATIC box (the sprite animates
// away from its own selection outline — intended, and simpler).
//
// Playback is PER-ELEMENT (previewingElementIds), but the CLOCK is shared (previewClockT0),
// reset only when the FIRST element starts — so two elements previewing at once sample the
// same t = now - t0 and identical specs stay in phase. The rAF loop runs ONLY while something
// previews and STOPS DEAD when the set empties (the perf culture here forbids an idle rAF). It
// also self-cleans: an element deleted/undone (or whose animation was cleared) drops out on
// the next tick and the loop ends once nothing is left — no stale id keeps it alive.
const previewingElementIds = new Set();
let previewClockT0 = 0;
let previewRafId = 0;

// Drop previewing ids that no longer resolve to a live element carrying an animation (the
// element was deleted/undone, its animation was cleared, or the project was switched) — the
// self-clean that lets a mutation on ANY other path silently retire a preview.
function prunePreviewIds() {
  for (const id of previewingElementIds) {
    const element = elements().find((item) => item.id === id);
    if (!element || !element.animation) previewingElementIds.delete(id);
  }
}

function previewLoop() {
  previewRafId = 0;
  prunePreviewIds();
  if (!previewingElementIds.size) return; // nothing left -> the loop dies (no idle rAF)
  render();
  previewRafId = requestAnimationFrame(previewLoop);
}

export function isAnimationPreviewing(elementId) {
  return previewingElementIds.has(elementId);
}

// Start previewing an element (the inspector Play button). The shared clock resets only when
// the FIRST element starts, so a second element joining an in-flight preview stays in phase.
export function startAnimationPreview(elementId) {
  if (!elementId || previewingElementIds.has(elementId)) return;
  if (!previewingElementIds.size) previewClockT0 = performance.now();
  previewingElementIds.add(elementId);
  if (!previewRafId) previewRafId = requestAnimationFrame(previewLoop);
  render(); // paint the first animated frame now, don't wait a whole rAF
}

// Stop previewing an element (the inspector Stop button). The canvas settles back to the
// static box immediately; cancelling the rAF outright when it was the last preview.
export function stopAnimationPreview(elementId) {
  if (!previewingElementIds.delete(elementId)) return;
  if (!previewingElementIds.size && previewRafId) {
    cancelAnimationFrame(previewRafId);
    previewRafId = 0;
  }
  render();
}

export function toggleAnimationPreview(elementId) {
  if (previewingElementIds.has(elementId)) stopAnimationPreview(elementId);
  else startAnimationPreview(elementId);
}

// The composed animation transform for an element THIS frame, or null when it is not
// previewing / carries no animation. Sampled at the SHARED clock so every previewing element
// reads the same t (and thus stays in phase).
function previewSampleFor(element) {
  if (!previewingElementIds.has(element.id) || !element.animation) return null;
  return sampleAnimation(element.animation, performance.now() - previewClockT0);
}

// Wrap a draw callback in the render-time animation transform: the world-unit offset becomes a
// screen translation (world->screen is a pan/zoom similarity, no rotation), then rot+scale
// pivot about the element's STATIC box center — the SAME center the element's own rotation
// pivots about, so anim rot/scale compose on top of the element's rotation/flip for free (this
// wrapper sits OUTSIDE that block). A plain passthrough when the sample is identity, so a
// resting channel never pays a save/restore.
function drawWithAnimation(sample, element, vp, drawFn) {
  const active = sample && (sample.offX !== 0 || sample.offY !== 0 || sample.rot !== 0 || sample.scale !== 1);
  if (!active) {
    drawFn();
    return;
  }
  const center = imageToScreenPoint({ x: element.x + element.w / 2, y: element.y + element.h / 2 }, vp);
  ctx.save();
  ctx.translate(sample.offX * vp.scale, sample.offY * vp.scale);
  ctx.translate(center.x, center.y);
  if (sample.rot) ctx.rotate((sample.rot * Math.PI) / 180);
  if (sample.scale !== 1) ctx.scale(sample.scale, sample.scale);
  ctx.translate(-center.x, -center.y);
  drawFn();
  ctx.restore();
}

function paintElement(element, vp, editEl) {
  if (element.type === "text") {
    paintTextElement(element, vp, editEl);
    return;
  }
  if (element.type === "note") {
    paintNoteElement(element, vp, editEl);
    return;
  }
  const isEdit = editEl && element.id === editEl.id;
  // T0260 increment 2: the live animation-preview sample for THIS element this frame (null
  // unless it is previewing with a real animation). Its opacity multiplies into globalAlpha
  // below; its offset/rot/scale wrap the image draw (drawWithAnimation) further down.
  const animSample = previewSampleFor(element);
  // Mode B dims every other element to focus the isolated one. T0260: element.opacity
  // (static, [0,1], absent = 1) multiplies into that dim, set BEFORE the rotate/flip/
  // slice9 drawBody so the whole element draw is alpha-scaled — parity with
  // render_group.py's paint_element alpha multiply. The sampled opacity channel (increment 2)
  // multiplies in on top, clamped [0,1] by the sampler. Reset to 1 below.
  const opacity = element.opacity === undefined || element.opacity === null ? 1 : Number(element.opacity);
  ctx.globalAlpha = (editEl && !isEdit ? 0.3 : 1) * opacity * (animSample ? animSample.opacity : 1);
  // T0207: a live Quantize/Denoise preview for THIS element, while "Hold to compare"
  // isn't pressed, paints in place of the source — pure view-state substitution, nothing
  // about `element` or the store changes.
  const previewBitmap =
    cleanupPreview && cleanupPreview.elementId === element.id && !cleanupPreviewCompare ? cleanupPreview.bitmap : null;
  const img = previewBitmap || imageFor(element);
  const origin = imageToScreenPoint({ x: element.x, y: element.y }, vp);
  const w = element.w * vp.scale;
  const h = element.h * vp.scale;
  const rotation = Number(element.rotation) || 0;
  const flipH = element.flipH === true;
  const flipV = element.flipV === true;
  if (img.complete && img.naturalWidth) {
    // T0233: a slice9-configured element replaces the single "resize to box" draw
    // with a loop of <=9 drawImage patches (dst in ELEMENT-LOCAL units, mapped to
    // screen via origin + vp.scale below) — the drop-in replacement (design section
    // 4.0) that lets slice9 compose with the T0232 rotate/flip wrapper for free: it
    // sits INSIDE the same ctx transform the plain draw already relies on.
    const patches = element.slice9
      ? slice9Patches(element.slice9, img.naturalWidth, img.naturalHeight, element.w, element.h)
      : null;
    const drawBody = () => {
      if (patches) {
        for (const p of patches) {
          ctx.drawImage(
            img,
            p.sx, p.sy, p.sw, p.sh,
            origin.x + p.dx * vp.scale, origin.y + p.dy * vp.scale, p.dw * vp.scale, p.dh * vp.scale,
          );
        }
      } else {
        ctx.drawImage(img, origin.x, origin.y, w, h); // unchanged fallback
      }
    };
    // T0260 increment 2: the animation preview composes as an OUTER transform around this
    // (untouched) rotation/flip block — see drawWithAnimation. Not previewing / at rest = a
    // plain passthrough, byte-identical to before this increment.
    drawWithAnimation(animSample, element, vp, () => {
      if (rotation || flipH || flipV) {
        // T0232 increment 3a — rotation/flip parity contract (must agree byte-for-byte with
        // render_group.py's paint_element, see README "Rotation & flip"): rotate(+theta) is
        // CW on this Y-down canvas, about the element's box CENTER; flip mirrors in the same
        // rotated local frame (composition order resize -> flip -> rotate, flip innermost).
        // Translating to the SCREEN-space center, applying rotate+scale(flip), then
        // translating back lets drawImage keep using the ORIGINAL unrotated origin/w/h — pan
        // and zoom never rotate, so "screen space" and "world space" differ only by a
        // similarity transform, and this composition is algebraically identical to drawing
        // the image in a box centered at the origin under the same rotate+flip.
        const center = imageToScreenPoint({ x: element.x + element.w / 2, y: element.y + element.h / 2 }, vp);
        ctx.save();
        ctx.translate(center.x, center.y);
        if (rotation) ctx.rotate((rotation * Math.PI) / 180);
        if (flipH || flipV) ctx.scale(flipH ? -1 : 1, flipV ? -1 : 1);
        ctx.translate(-center.x, -center.y);
        drawBody();
        ctx.restore();
      } else {
        drawBody();
      }
    });
  } else {
    ctx.strokeStyle = "#596774";
    ctx.strokeRect(origin.x, origin.y, w, h);
  }
  ctx.globalAlpha = 1;
  // T0207 UX (lead live-verify 2026-07-04): while a cleanup preview paints in place of the
  // source, the canvas itself must SAY so — an amber chip above the element's top-left,
  // flipping to "original" while Hold-to-see-original is pressed. Pure view-state like the
  // preview substitution above; same unrotated-chrome shortcut as the ref chip below. Sits
  // within CULL_MARGIN, so culling math needs no change.
  if (cleanupPreview && cleanupPreview.elementId === element.id) {
    const label = cleanupPreviewCompare ? "original" : "preview — Apply to keep";
    ctx.save();
    ctx.font = "11px system-ui, 'Segoe UI', sans-serif";
    const chipPadX = 4;
    const chipH = 14;
    const chipTextW = Math.ceil(ctx.measureText(label).width);
    ctx.fillStyle = "#d7a14a";
    ctx.fillRect(origin.x, origin.y - chipH - 2, chipTextW + chipPadX * 2, chipH);
    ctx.fillStyle = "#231a08"; // dark text for contrast against the amber fill
    ctx.textBaseline = "middle";
    ctx.fillText(label, origin.x + chipPadX, origin.y - chipH / 2 - 2 + 0.5);
    ctx.restore();
  }
  // Ref marking (T0239 increment 3, design R1): the style card's ONE ref image gets a blue
  // outline + a small "ref" chip at its top-left, drawn regardless of selection — always
  // visible so it is clear at a glance which member actually travels with generation. Plain
  // unrotated box (matches the rest of this file's un-rotated chrome shortcuts).
  if (element.type === "image" && element.groupId) {
    const parentGroup = groupById(element.groupId);
    if (parentGroup && parentGroup.style && parentGroup.style.ref === element.id) {
      ctx.save();
      ctx.strokeStyle = STYLE_ACCENT;
      ctx.lineWidth = 2;
      ctx.strokeRect(origin.x, origin.y, w, h);
      const chipLabel = "ref";
      ctx.font = "11px system-ui, 'Segoe UI', sans-serif";
      const chipTextW = Math.ceil(ctx.measureText(chipLabel).width);
      const chipPadX = 4;
      const chipH = 14;
      ctx.fillStyle = STYLE_ACCENT;
      ctx.fillRect(origin.x, origin.y, chipTextW + chipPadX * 2, chipH);
      ctx.fillStyle = "#0a2430"; // dark text for contrast against the blue fill
      ctx.textBaseline = "middle";
      ctx.fillText(chipLabel, origin.x + chipPadX, origin.y + chipH / 2 + 0.5);
      ctx.restore();
    }
  }
  if (isSelected(element)) {
    ctx.strokeStyle = isEdit ? "#3fc7ba" : "#77a7ff";
    ctx.lineWidth = 2;
    // T0232 increment 3b: a rotated element's selection outline is its ROTATED quad (the
    // true footprint), matching the rotation-aware hit-test/handles below -- an unrotated
    // element keeps the plain rect (identical pixels to increment 3a).
    if (rotation) strokeRotatedQuad(element, vp);
    else ctx.strokeRect(origin.x, origin.y, w, h);
    // T0233 (lead 2026-07-04: «слайс9 нужно показывать линиями как и где растягивается сами
    // зоны»): a selected slice9 element shows its band seams — dashed green lines where the
    // fixed corners end and the stretch zones begin ON THE CURRENT BOX. Derived from the SAME
    // slice9Patches call the draw itself uses (scale multiplier + proportional clamp
    // included), so the lines sit exactly on the real seams at any box size. Drawn inside the
    // same rotate/flip transform trick as the body, so a transformed panel shows truthful
    // seams; like the selection outline, they ignore the animation-preview offset (static
    // chrome by design).
    if (element.slice9) {
      const s9img = imageFor(element);
      if (s9img.complete && s9img.naturalWidth) {
        const s9patches = slice9Patches(element.slice9, s9img.naturalWidth, s9img.naturalHeight, element.w, element.h);
        const seamXs = new Set();
        const seamYs = new Set();
        for (const p of s9patches) {
          for (const v of [p.dx, p.dx + p.dw]) if (v > 0.01 && v < element.w - 0.01) seamXs.add(Math.round(v * 1000));
          for (const v of [p.dy, p.dy + p.dh]) if (v > 0.01 && v < element.h - 0.01) seamYs.add(Math.round(v * 1000));
        }
        const drawSeams = () => {
          ctx.save();
          ctx.strokeStyle = "#65bd81";
          ctx.lineWidth = 1;
          ctx.setLineDash([4, 4]);
          ctx.beginPath();
          for (const raw of seamXs) {
            const sx = origin.x + (raw / 1000) * vp.scale;
            ctx.moveTo(sx, origin.y);
            ctx.lineTo(sx, origin.y + h);
          }
          for (const raw of seamYs) {
            const sy = origin.y + (raw / 1000) * vp.scale;
            ctx.moveTo(origin.x, sy);
            ctx.lineTo(origin.x + w, sy);
          }
          ctx.stroke();
          ctx.restore();
        };
        if (rotation || flipH || flipV) {
          const center = imageToScreenPoint({ x: element.x + element.w / 2, y: element.y + element.h / 2 }, vp);
          ctx.save();
          ctx.translate(center.x, center.y);
          if (rotation) ctx.rotate((rotation * Math.PI) / 180);
          if (flipH || flipV) ctx.scale(flipH ? -1 : 1, flipV ? -1 : 1);
          ctx.translate(-center.x, -center.y);
          drawSeams();
          ctx.restore();
        } else {
          drawSeams();
        }
      }
    }
    // Passive numbered hint in mode A; strong strokes + handles in mode B.
    drawRegionsOverlay(ctx, element, vp, {
      selectedRegionIds: state.selectedRegionIds,
      interactive: Boolean(isEdit),
    });
  }
}

// Stroke a node's ROTATED footprint (rotatedCorners, world -> screen) instead of the plain
// AABB rect -- the selection outline for a single rotated element/text. Assumes
// ctx.strokeStyle/lineWidth are already set by the caller (mirrors strokeRect's own
// "caller sets style" convention throughout this file).
function strokeRotatedQuad(node, vp) {
  const corners = rotatedCorners(node).map((corner) => imageToScreenPoint(corner, vp));
  ctx.beginPath();
  ctx.moveTo(corners[0].x, corners[0].y);
  for (let i = 1; i < corners.length; i += 1) ctx.lineTo(corners[i].x, corners[i].y);
  ctx.closePath();
  ctx.stroke();
}

// Paint a TEXT element: the page's same-font approximation of the PIL export (PIL is
// the source of rendered truth). Re-measures every paint (auto-width); reconciles the
// stored w/h (selection/marquee bookkeeping) in memory WITHOUT writing to disk — the
// disk w/h only updates on the patchElement that commits a content/style change.
// Parity rules: textBaseline "top"; per-line y = top + i*(fontSize*lineHeight); the
// HARD offset shadow is drawn FIRST (blur 0 in v1); the stroke is drawn UNDER the fill
// with lineWidth = 2 x style.stroke.width + lineJoin round (PIL grows the stroke
// outward, so 2x centered matches). The element being inline-edited is skipped (the
// textarea overlay shows the live text).
function paintTextElement(element, vp, editEl) {
  const isEdit = editEl && element.id === editEl.id;
  // T0260 increment 2: text carries the same animation as an image element — the sampled
  // opacity multiplies globalAlpha, the offset/rot/scale wrap the glyph draw (about the box
  // center). Null when not previewing => byte-identical to before.
  const animSample = previewSampleFor(element);
  ctx.globalAlpha = (editEl && !isEdit ? 0.3 : 1) * (animSample ? animSample.opacity : 1);
  const origin = imageToScreenPoint({ x: element.x, y: element.y }, vp);
  const style = element.style || {};
  if (areFontsReady() && state.editingTextId !== element.id) {
    drawWithAnimation(animSample, element, vp, () => drawTextGlyphs(element, style, origin, vp));
  }
  ctx.globalAlpha = 1;
  if (isSelected(element)) {
    ctx.strokeStyle = "#77a7ff";
    ctx.lineWidth = 2;
    // T0232 increment 3b: same rotated-quad treatment as an image element's outline above --
    // a text element's BOX can carry rotation even though its glyphs don't render rotated
    // yet (README "Rotation & flip"), so the outline still shows the box's true footprint.
    const rotation = Number(element.rotation) || 0;
    if (rotation) strokeRotatedQuad(element, vp);
    else ctx.strokeRect(origin.x, origin.y, element.w * vp.scale, element.h * vp.scale);
  }
}

// Draw a text element's glyphs onto the canvas and reconcile its in-memory box.
// `origin` is the box top-left in screen px; widths are measured at WORLD size and
// scaled (canvas advance scales linearly), so lines align without a second measure.
function drawTextGlyphs(element, style, origin, vp) {
  const scale = vp.scale;
  const fontSize = Number(style.fontSize) || 24;
  const lineHeight = Number(style.lineHeight) || 1.2;
  const { lines, widths, boxW } = measureTextLines(element.content, style);
  // Reconcile the stored box (world px) so selection/marquee/hit-test match the glyphs.
  element.w = Math.max(1, Math.ceil(boxW));
  element.h = Math.max(1, Math.ceil(lines.length * fontSize * lineHeight));

  const align = style.align || "left";
  const lineStep = fontSize * lineHeight * scale;
  const lineX = (i) => {
    if (align === "center") return origin.x + ((boxW - widths[i]) * scale) / 2;
    if (align === "right") return origin.x + (boxW - widths[i]) * scale;
    return origin.x;
  };

  ctx.save();
  ctx.font = canvasFontString(style, fontSize * scale);
  ctx.textBaseline = "top";
  ctx.textAlign = "left"; // per-line align offsets are computed manually
  const stroke = style.stroke && Number(style.stroke.width) > 0 ? style.stroke : null;
  const shadow = style.shadow || null;

  // Shadow pass FIRST (hard offset, fill only) so the main text always sits on top.
  if (shadow) {
    ctx.fillStyle = shadow.color || "#000000";
    const sdx = (Number(shadow.dx) || 0) * scale;
    const sdy = (Number(shadow.dy) || 0) * scale;
    for (let i = 0; i < lines.length; i += 1) ctx.fillText(lines[i], lineX(i) + sdx, origin.y + i * lineStep + sdy);
  }
  // Stroke UNDER fill: lineWidth = 2 x width so the centered canvas stroke matches PIL's
  // outward-grown stroke.
  if (stroke) {
    ctx.strokeStyle = stroke.color || "#000000";
    ctx.lineWidth = 2 * Number(stroke.width) * scale;
    ctx.lineJoin = "round";
    for (let i = 0; i < lines.length; i += 1) ctx.strokeText(lines[i], lineX(i), origin.y + i * lineStep);
  }
  ctx.fillStyle = style.color || "#111111";
  for (let i = 0; i < lines.length; i += 1) ctx.fillText(lines[i], lineX(i), origin.y + i * lineStep);
  ctx.restore();
}

// ---- note cards (T0268) ------------------------------------------------------
//
// A note is a Miro/FigJam sticky: a colored, FULLY-fixed box (user-set w AND h) with plain
// text word-wrapped to the padded inner width and CLIPPED at the box, plus an overflow
// indicator (a bottom fade) when the wrapped text is taller than the box. Notes are canvas
// annotations — they never reach a PNG (renderGroup/exportProject skip them). The greedy
// wrap is cached per element, keyed on content + inner width + font, so it only recomputes
// when one of those actually changes (never per frame during a pan/zoom).
const noteWrapCache = new Map(); // elementId -> { key, lines }

function noteInnerWidth(element) {
  return Math.max(1, (Number(element.w) || 0) - NOTE_PADDING * 2);
}

function noteWrappedLines(element, style) {
  const innerWidth = noteInnerWidth(element);
  const key = `${element.content || ""}\u0000${innerWidth}\u0000${canvasFontString(style, Number(style.fontSize) || 18)}`;
  const cached = noteWrapCache.get(element.id);
  if (cached && cached.key === key) return cached.lines;
  const lines = wrapNoteLines(element.content, style, innerWidth);
  noteWrapCache.set(element.id, { key, lines });
  return lines;
}

function paintNoteElement(element, vp, editEl) {
  const isEdit = editEl && element.id === editEl.id;
  ctx.globalAlpha = editEl && !isEdit ? 0.3 : 1;
  const origin = imageToScreenPoint({ x: element.x, y: element.y }, vp);
  const style = element.style || {};
  const w = Math.max(1, (Number(element.w) || 0) * vp.scale);
  const h = Math.max(1, (Number(element.h) || 0) * vp.scale);
  const radius = Math.min(10 * vp.scale, w / 2, h / 2);

  // Background fill (a slightly rounded rect) — the sticky card. `background` absent = no fill.
  const bg = element.background && element.background.type === "color" ? element.background.color : null;
  ctx.save();
  roundRectPath(origin.x, origin.y, w, h, radius);
  if (bg) {
    ctx.fillStyle = bg;
    ctx.fill();
  }
  // Thin card border so an unfilled/pale note still reads as a box.
  ctx.strokeStyle = "rgba(0,0,0,0.18)";
  ctx.lineWidth = 1;
  ctx.stroke();
  ctx.restore();

  // Wrapped text, clipped to the padded inner box. Skipped while inline-editing (the
  // textarea overlay shows the live text) and until the real fonts are ready (no FOUT).
  let overflow = false;
  if (areFontsReady() && state.editingTextId !== element.id) {
    const fontSize = Number(style.fontSize) || 18;
    const lineHeight = Number(style.lineHeight) || 1.35;
    const lines = noteWrappedLines(element, style);
    const pad = NOTE_PADDING * vp.scale;
    const innerX = origin.x + pad;
    const innerY = origin.y + pad;
    const innerW = Math.max(1, w - pad * 2);
    const innerH = Math.max(1, h - pad * 2);
    const lineStep = fontSize * lineHeight * vp.scale;
    overflow = lines.length * lineStep > innerH + 0.5;

    ctx.save();
    ctx.beginPath();
    ctx.rect(innerX, innerY, innerW, innerH);
    ctx.clip();
    ctx.font = canvasFontString(style, fontSize * vp.scale);
    ctx.textBaseline = "top";
    ctx.textAlign = "left";
    ctx.fillStyle = style.color || "#1a1a1a";
    const align = style.align || "left";
    for (let i = 0; i < lines.length; i += 1) {
      const y = innerY + i * lineStep;
      if (y > innerY + innerH) break; // fully below the clip box — stop early
      let x = innerX;
      if (align !== "left") {
        const lineW = ctx.measureText(lines[i]).width;
        x = align === "center" ? innerX + (innerW - lineW) / 2 : innerX + innerW - lineW;
      }
      ctx.fillText(lines[i], x, y);
    }
    ctx.restore();

    // Overflow indicator: a bottom fade over the clipped-off text — cheap + obvious that
    // more text exists below (double-click to read/edit it all).
    if (overflow) {
      const fadeH = Math.min(18 * vp.scale, h * 0.4);
      const grad = ctx.createLinearGradient(0, origin.y + h - fadeH, 0, origin.y + h);
      const fade = bg || "#ffffff";
      grad.addColorStop(0, hexToRgba(fade, 0));
      grad.addColorStop(1, hexToRgba(fade, 0.95));
      ctx.save();
      roundRectPath(origin.x, origin.y, w, h, radius);
      ctx.clip();
      ctx.fillStyle = grad;
      ctx.fillRect(origin.x, origin.y + h - fadeH, w, fadeH);
      ctx.restore();
    }
  }
  ctx.globalAlpha = 1;

  if (isSelected(element)) {
    ctx.strokeStyle = "#77a7ff";
    ctx.lineWidth = 2;
    ctx.strokeRect(origin.x, origin.y, w, h);
  }
}

// Trace a rounded-rectangle path (no fill/stroke — caller decides). Used by the note card.
function roundRectPath(x, y, w, h, r) {
  const radius = Math.max(0, Math.min(r, w / 2, h / 2));
  ctx.beginPath();
  ctx.moveTo(x + radius, y);
  ctx.arcTo(x + w, y, x + w, y + h, radius);
  ctx.arcTo(x + w, y + h, x, y + h, radius);
  ctx.arcTo(x, y + h, x, y, radius);
  ctx.arcTo(x, y, x + w, y, radius);
  ctx.closePath();
}

// #rrggbb -> "rgba(r,g,b,a)" for the note overflow fade gradient. A non-hex value falls
// back to white so the fade still reads.
function hexToRgba(hex, alpha) {
  const m = /^#([0-9a-fA-F]{6})$/.exec(String(hex || "").trim());
  if (!m) return `rgba(255,255,255,${alpha})`;
  const n = parseInt(m[1], 16);
  return `rgba(${(n >> 16) & 255},${(n >> 8) & 255},${n & 255},${alpha})`;
}

// ---- inline text editor (textarea overlay) -----------------------------------
//
// Double-click a text element (or place one with the T tool) to edit it in place: a
// textarea positioned over the box, styled to match the font. Commit on blur or
// Ctrl/Cmd+Enter (Enter alone inserts a newline — text is multi-line); Esc cancels.
// One patchElement per commit (content + the re-measured box in the SAME entry).
let textEditor = null; // { el: <textarea>, elementId }
let textCommitting = false;

function openTextEditor(element) {
  closeTextEditor();
  state.editingTextId = element.id;
  const isNote = element.type === "note";
  const ta = document.createElement("textarea");
  ta.className = isNote ? "text-edit-overlay note-edit-overlay" : "text-edit-overlay";
  ta.value = element.content || "";
  ta.spellcheck = false;
  // A note wraps softly inside its FIXED box (T0268); a text element is auto-width (no wrap).
  ta.wrap = isNote ? "soft" : "off";
  ta.style.whiteSpace = isNote ? "pre-wrap" : "pre";
  el("stage").appendChild(ta);
  textEditor = { el: ta, elementId: element.id, isNote };
  positionTextEditor();
  ta.focus();
  ta.select();
  ta.addEventListener("keydown", onTextEditorKey);
  ta.addEventListener("blur", () => commitTextEditor());
  // A note's box is fixed, so it never re-flows the overlay on input; only auto-width text does.
  if (!isNote) ta.addEventListener("input", positionTextEditor);
}

function onTextEditorKey(event) {
  event.stopPropagation(); // keep the global shortcut handler out of the editor
  if (event.key === "Escape") {
    event.preventDefault();
    closeTextEditor();
    render();
  } else if (event.key === "Enter" && (event.ctrlKey || event.metaKey)) {
    event.preventDefault();
    commitTextEditor();
  }
}

function positionTextEditor() {
  if (!textEditor) return;
  const element = elements().find((item) => item.id === textEditor.elementId);
  if (!element) {
    closeTextEditor();
    return;
  }
  const vp = state.viewport;
  const origin = imageToScreenPoint({ x: element.x, y: element.y }, vp);
  const style = element.style || {};
  const ta = textEditor.el;
  if (textEditor.isNote) {
    // A note's editor exactly matches its FIXED box (padded), wrapping softly inside it —
    // no auto-width, no per-keystroke reflow; commit only changes the content string.
    const fontSize = Number(style.fontSize) || 18;
    const pad = NOTE_PADDING * vp.scale;
    ta.style.left = `${origin.x + pad}px`;
    ta.style.top = `${origin.y + pad}px`;
    ta.style.font = canvasFontString(style, fontSize * vp.scale);
    ta.style.lineHeight = `${fontSize * (Number(style.lineHeight) || 1.35) * vp.scale}px`;
    ta.style.color = style.color || "#1a1a1a";
    ta.style.textAlign = style.align || "left";
    ta.style.width = `${Math.max(24, element.w * vp.scale - pad * 2)}px`;
    ta.style.height = `${Math.max(fontSize * vp.scale, element.h * vp.scale - pad * 2)}px`;
    return;
  }
  const fontSize = Number(style.fontSize) || 24;
  // Live auto-width: re-measure the CURRENT textarea value with the same measureTextBox
  // used by the commit path (actions.js patchTextElement), so the box grows/shrinks on
  // every keystroke and never jumps on Enter/commit (identical math, not the browser's
  // native scrollHeight autosize, which can drift from the font-metrics measurement).
  const box = measureTextBox(ta.value, style);
  ta.style.left = `${origin.x}px`;
  ta.style.top = `${origin.y}px`;
  ta.style.font = canvasFontString(style, fontSize * vp.scale);
  ta.style.lineHeight = `${fontSize * (Number(style.lineHeight) || 1.2) * vp.scale}px`;
  ta.style.color = style.color || "#111111";
  ta.style.textAlign = style.align || "left";
  ta.style.width = `${Math.max(48, box.w * vp.scale + 12)}px`;
  ta.style.height = `${Math.max(box.h * vp.scale, fontSize * vp.scale)}px`;
}

async function commitTextEditor() {
  if (!textEditor || textCommitting) return;
  textCommitting = true;
  const { el: ta, elementId, isNote } = textEditor;
  const content = ta.value;
  const element = elements().find((item) => item.id === elementId);
  textEditor = null;
  state.editingTextId = null;
  ta.remove();
  if (element && content !== element.content) {
    // A note's box is fixed: commit ONLY the content (no re-measured box); text commits the
    // content + its re-measured auto-width box in the same journaled patchElement entry.
    if (isNote) await patchNoteContent(elementId, content);
    else await patchTextElement(elementId, { content });
  } else {
    render();
  }
  textCommitting = false;
}

// Discard the editor without committing (Esc / teardown). Safe to call when none open.
function closeTextEditor() {
  if (!textEditor) return;
  const ta = textEditor.el;
  textEditor = null;
  state.editingTextId = null;
  ta.remove();
}

// Reposition or tear down the editor to match state each render (pan/zoom, or the
// element vanished under undo/redo/project-switch).
function syncTextEditor() {
  if (!textEditor) return;
  const element = elements().find((item) => item.id === textEditor.elementId);
  if (!element || state.editingTextId !== textEditor.elementId) {
    closeTextEditor();
    return;
  }
  positionTextEditor();
}

// Solid group background fill behind the group's children (§ group background). Only a
// valid {type:"color"} background paints; the hairline frame + label ALWAYS draw in
// pass 2, so an empty screen stays visible even with no fill.
function fillGroupBackground(group, vp) {
  const bg = group.background;
  if (!bg || bg.type !== "color" || !/^#[0-9a-fA-F]{6}$/.test(String(bg.color || ""))) return;
  const origin = imageToScreenPoint({ x: group.x, y: group.y }, vp);
  ctx.fillStyle = bg.color;
  ctx.fillRect(origin.x, origin.y, group.w * vp.scale, group.h * vp.scale);
}

// The intersection (in world coords) of every clip=true ANCESTOR group's box for a node,
// or null when the node has no clipping ancestor. This is the region a clipped node is
// actually visible within — used for hit-testing, marquee, and the ghost hint. Nested
// clips intersect into one box.
function clipIntersection(node) {
  let has = false;
  let x0 = -Infinity;
  let y0 = -Infinity;
  let x1 = Infinity;
  let y1 = Infinity;
  for (const ancestor of ancestorsOf(state.project, node)) {
    if (ancestor.clip !== true) continue;
    has = true;
    x0 = Math.max(x0, ancestor.x);
    y0 = Math.max(y0, ancestor.y);
    x1 = Math.min(x1, ancestor.x + ancestor.w);
    y1 = Math.min(y1, ancestor.y + ancestor.h);
  }
  return has ? { x: x0, y: y0, w: x1 - x0, h: y1 - y0 } : null;
}

// A node's visible (clipped) box: its own box intersected with every clip=true ancestor,
// or null when a clip crops it away entirely. Equals the raw box when there is no clipping
// ancestor. Backs the marquee's "select what is actually visible" test.
function visibleBox(node) {
  const clip = clipIntersection(node);
  if (!clip) return { x: node.x, y: node.y, w: node.w, h: node.h };
  const x0 = Math.max(node.x, clip.x);
  const y0 = Math.max(node.y, clip.y);
  const x1 = Math.min(node.x + node.w, clip.x + clip.w);
  const y1 = Math.min(node.y + node.h, clip.y + clip.h);
  return x1 > x0 && y1 > y0 ? { x: x0, y: y0, w: x1 - x0, h: y1 - y0 } : null;
}

// Ghost the portion of a selected image element that a clipping ancestor crops away: the
// image redrawn at low alpha OUTSIDE the clip box (an even-odd clip subtracts the visible
// region, so only the cropped-away part shows). Redrawing the image (vs. a dashed box) is
// chosen because it shows WHAT is hidden and exactly where, which is the whole point of the
// anti-"lost my sprite" hint; it stays cheap (selection-only) and, being pass-2 chrome over
// an even-odd clip, never repaints the visible artwork.
function drawClipGhost(element, vp) {
  const clip = clipIntersection(element);
  if (!clip) return; // no clipping ancestor -> nothing is cropped
  const inside =
    element.x >= clip.x &&
    element.y >= clip.y &&
    element.x + element.w <= clip.x + clip.w &&
    element.y + element.h <= clip.y + clip.h;
  if (inside) return; // fully within the clip -> nothing cropped away
  const img = imageFor(element);
  if (!(img.complete && img.naturalWidth)) return;
  const origin = imageToScreenPoint({ x: element.x, y: element.y }, vp);
  const clipOrigin = imageToScreenPoint({ x: clip.x, y: clip.y }, vp);
  ctx.save();
  ctx.beginPath();
  ctx.rect(0, 0, state.cssWidth, state.cssHeight);
  ctx.rect(clipOrigin.x, clipOrigin.y, clip.w * vp.scale, clip.h * vp.scale);
  ctx.clip("evenodd"); // region = viewport MINUS the visible clip box (the cropped-away area)
  ctx.globalAlpha = 0.25;
  ctx.drawImage(img, origin.x, origin.y, element.w * vp.scale, element.h * vp.scale);
  ctx.globalAlpha = 1;
  ctx.restore();
}

// Ghost every selected element and every image element inside a selected group that a
// clipping ancestor crops. Deduped so an element selected directly and via its group ghosts
// once. Hidden nodes are skipped (they never paint). HIDDEN by default (T0224 item 6): the
// ghost only paints while Alt is held (state.clipGhostPeek) — a view-state peek, never
// journaled/persisted — so a clipped sprite doesn't permanently show its cropped-away half.
function drawClipGhosts(vp) {
  if (!state.clipGhostPeek) return;
  const seen = new Set();
  const ghost = (element) => {
    if (!element || seen.has(element.id) || isNodeHidden(state.project, element)) return;
    seen.add(element.id);
    drawClipGhost(element, vp);
  };
  for (const element of selectedElements()) ghost(element);
  const groupIds = new Set(state.selectedGroupIds);
  if (state.selectedGroupId) groupIds.add(state.selectedGroupId);
  for (const gid of groupIds) {
    for (const element of descendantsOf(state.project, gid).elements) ghost(element);
  }
}

function updateBreadcrumb(editEl) {
  const node = el("region-breadcrumb");
  if (!node) return;
  if (editEl) {
    const empty = !(editEl.regions || []).length;
    // Empty-state hint inside the mode so a fresh image tells you what to do.
    node.textContent = empty
      ? `Regions: ${editEl.name || editEl.id} — pick a tool and draw on the image (Esc to exit)`
      : `Regions: ${editEl.name || editEl.id} — Esc to exit`;
    node.classList.remove("hidden");
  } else {
    node.classList.add("hidden");
  }
}

// The region-edit tool row (Select / Draw Rect / Draw Polygon) is shown only inside
// region-edit isolation; the active tool is highlighted. Placement/markup is a chip-
// like floating toolbar under the breadcrumb (canvas.html / canvas.css).
function updateRegionTools(editEl) {
  const node = el("region-tools");
  if (!node) return;
  node.classList.toggle("hidden", !editEl);
  if (!editEl) return;
  for (const button of node.querySelectorAll(".rtool")) {
    button.classList.toggle("active", button.dataset.regionTool === state.regionTool);
  }
}

// T0239 increment 2 (lead, live: "сейчас выглядит как группа" — a recipe card must read as
// a distinct "special container" at a glance, not a plain group). #d7a14a mirrors canvas.css's
// --amber custom property — workspace.js draws on a bare 2D context (no CSS var access), so
// the hex is duplicated here; keep it in sync if canvas.css ever changes --amber. This is the
// SAME hex the frame already uses for a plain group's "selected" stroke (line below), so a
// recipe card's stroke stays amber REGARDLESS of selection (dashed too, see drawGroupFrame)
// — selection still reads via the thicker line width, exactly like a plain group.
const RECIPE_ACCENT = "#d7a14a";

// T0239 increment 3 (design R1): style card chrome — the SAME "special container" contract
// as the recipe card above, a distinct accent so the two card types read apart at a glance.
// Hardcoded for the same bare-2D-context reason as RECIPE_ACCENT (no CSS var access here);
// canvas.css also carries this as --style-accent for the DOM-side "ref" badge. Violet per
// the 2026-07-04 review (item 4): the original cyan collided with the region-edit accent.
const STYLE_ACCENT = "#9d7fd8";

// Recipe-card prompt preview (T0239 increment 2): mirrors layers_panel.js's textPreview
// (T0231) — newlines collapsed to spaces, trimmed, truncated with an ellipsis. Empty/
// whitespace-only prompt yields "" so the caller skips drawing it (no bare quotes on a
// still-blank card). Reused verbatim for the style card's prompt preview (T0239 increment
// 3) — the flatten/truncate logic is not recipe-specific despite the name.
const RECIPE_PROMPT_PREVIEW_MAX = 40;
function recipePromptPreview(prompt) {
  const flat = String(prompt || "").replace(/\s+/g, " ").trim();
  if (!flat) return "";
  return flat.length > RECIPE_PROMPT_PREVIEW_MAX ? `${flat.slice(0, RECIPE_PROMPT_PREVIEW_MAX)}…` : flat;
}

function drawGroupFrame(group, vp) {
  const origin = imageToScreenPoint({ x: group.x, y: group.y }, vp);
  const w = group.w * vp.scale;
  const h = group.h * vp.scale;
  const selected = state.selectedGroupId === group.id || state.selectedGroupIds.has(group.id);
  const isRecipeCard = !!group.recipe;
  const isStyleCard = !!group.style;
  ctx.lineWidth = selected ? 2 : 1;
  ctx.strokeStyle = isRecipeCard ? RECIPE_ACCENT : isStyleCard ? STYLE_ACCENT : selected ? "#d7a14a" : "#77a7ff";
  if (isRecipeCard || isStyleCard) {
    // Dashed frame = "special container" (a card is a workshop widget, not a plain group) —
    // read at a glance regardless of selection state. Reset after so the marquee/guide
    // overlays that draw later this same frame never inherit the dash pattern.
    ctx.save();
    ctx.setLineDash([6, 4]);
    ctx.strokeRect(origin.x, origin.y, w, h);
    ctx.restore();
  } else {
    ctx.strokeRect(origin.x, origin.y, w, h);
  }

  // Hover affordance: a subtle outline on the group a plain click would select at the
  // current scope, so the user sees what a click will grab before pressing.
  if (!selected && state.hoverGroupId === group.id) {
    ctx.save();
    ctx.lineWidth = 1.5;
    ctx.strokeStyle = "rgba(119, 167, 255, 0.6)";
    ctx.strokeRect(origin.x, origin.y, w, h);
    ctx.restore();
  }

  const label = group.name || "Group";
  ctx.font = "12px system-ui, 'Segoe UI', sans-serif";
  const padX = 6;
  const labelH = 16;
  const textW = Math.ceil(ctx.measureText(label).width);
  const rect = { groupId: group.id, x: origin.x, y: origin.y - labelH - 2, w: textW + padX * 2, h: labelH };
  ctx.fillStyle = selected ? "#d7a14a" : "#2764bd";
  ctx.fillRect(rect.x, rect.y, rect.w, rect.h);
  ctx.fillStyle = "#f8fbff";
  ctx.textBaseline = "middle";
  ctx.fillText(label, rect.x + padX, rect.y + labelH / 2 + 0.5);
  groupLabelRects.push(rect);

  // Recipe/Style-card chrome (T0239 increments 2/3): a tag chip beside the title (same
  // accent as the frame stroke) + a truncated prompt preview inside the frame. Both are
  // pure chrome — never pushed to groupLabelRects, so neither is click-selectable on its
  // own (the existing name pill above stays the only interactive label hit-area). The two
  // card types are mutually exclusive (a group never carries both blobs), so this is a
  // plain if/else — never two chips on the same frame.
  if (isRecipeCard) {
    const chipLabel = "Recipe";
    const chipTextW = Math.ceil(ctx.measureText(chipLabel).width);
    const chipX = rect.x + rect.w + 4;
    ctx.fillStyle = RECIPE_ACCENT;
    ctx.fillRect(chipX, rect.y, chipTextW + padX * 2, labelH);
    ctx.fillStyle = "#241b0a"; // dark text for contrast against the amber fill
    ctx.textBaseline = "middle";
    ctx.fillText(chipLabel, chipX + padX, rect.y + labelH / 2 + 0.5);

    const preview = recipePromptPreview(group.recipe.prompt);
    if (preview) {
      ctx.font = "11px system-ui, 'Segoe UI', sans-serif";
      ctx.fillStyle = "rgba(248, 251, 255, 0.6)";
      ctx.textBaseline = "top";
      ctx.fillText(preview, origin.x + padX, origin.y + 4);
    }
  } else if (isStyleCard) {
    const chipLabel = "Style";
    const chipTextW = Math.ceil(ctx.measureText(chipLabel).width);
    const chipX = rect.x + rect.w + 4;
    ctx.fillStyle = STYLE_ACCENT;
    ctx.fillRect(chipX, rect.y, chipTextW + padX * 2, labelH);
    ctx.fillStyle = "#0a2430"; // dark text for contrast against the blue fill
    ctx.textBaseline = "middle";
    ctx.fillText(chipLabel, chipX + padX, rect.y + labelH / 2 + 0.5);

    const preview = recipePromptPreview(group.style.prompt);
    if (preview) {
      ctx.font = "11px system-ui, 'Segoe UI', sans-serif";
      ctx.fillStyle = "rgba(248, 251, 255, 0.6)";
      ctx.textBaseline = "top";
      ctx.fillText(preview, origin.x + padX, origin.y + 4);
    }
  }
}

function normScreenRect(a, b) {
  return { x: Math.min(a.x, b.x), y: Math.min(a.y, b.y), w: Math.abs(b.x - a.x), h: Math.abs(b.y - a.y) };
}

// Live rubber-band overlays for the marquee and new-region gestures. The anchor corner is
// derived from the world start point every frame (imageToScreenPoint), so the band stays
// glued to the content point where the press began even if the user zooms mid-gesture; the
// moving corner is the live pointer (lastScreen).
function drawGestureOverlay() {
  if (!drag) return;
  if (drag.mode !== "marquee" && drag.mode !== "region-create") return;
  const start = imageToScreenPoint(drag.startWorld, state.viewport);
  const rect = normScreenRect(start, drag.lastScreen);
  const marquee = drag.mode === "marquee";
  ctx.save();
  ctx.setLineDash([4, 3]);
  ctx.fillStyle = marquee ? "rgba(119, 167, 255, 0.12)" : "rgba(63, 199, 186, 0.18)";
  ctx.strokeStyle = marquee ? "#77a7ff" : "#3fc7ba";
  ctx.lineWidth = marquee ? 1 : 1.5;
  ctx.fillRect(rect.x, rect.y, rect.w, rect.h);
  ctx.strokeRect(rect.x + 0.5, rect.y + 0.5, rect.w, rect.h);
  ctx.restore();
}

// T0244 Figma-pink alignment guides for the ACTIVE drag only: reads drag.activeGuides, set
// fresh every mousemove by the snap match (onMouseMove's three move cases) and cleared to []
// on bypass/no-match/mouseup. Early return keeps idle frames (no drag, or a drag with nothing
// in tolerance) free of any draw call -- this pays nothing outside an active, matched drag.
function drawSnapGuides(vp) {
  if (!drag || !drag.activeGuides || !drag.activeGuides.length) return;
  ctx.save();
  ctx.strokeStyle = "#ff2d78";
  ctx.lineWidth = 1;
  for (const guide of drag.activeGuides) {
    ctx.beginPath();
    if (guide.axis === "x") {
      const top = imageToScreenPoint({ x: guide.pos, y: guide.min }, vp);
      const bottom = imageToScreenPoint({ x: guide.pos, y: guide.max }, vp);
      ctx.moveTo(top.x, top.y);
      ctx.lineTo(bottom.x, bottom.y);
    } else {
      const left = imageToScreenPoint({ x: guide.min, y: guide.pos }, vp);
      const right = imageToScreenPoint({ x: guide.max, y: guide.pos }, vp);
      ctx.moveTo(left.x, left.y);
      ctx.lineTo(right.x, right.y);
    }
    ctx.stroke();
  }
  ctx.restore();
}

// ---- scale + rotate gizmo (T0232 increment 2 skeleton -> increment 3b interactive rotation
// -- see README "Rotation & flip"). Pure box math lives in viewport.mjs
// (resizeBox/mapItemBox/scaledFontSize/SCALE_HANDLES for the plain AABB case;
// resizeRotatedBox/rotatedHandlePoints/rotationFromDrag/pointInRotatedBox for a SOLO rotated
// element, added in 3b) so it is DOM-free and unit-testable; this section only wires
// screen-space drawing/hit-testing and the drag lifecycle, mirroring regions.js's
// REGION_HANDLES/hitRegionHandle pattern one level up (the SELECTION's AABB, or -- for a
// single rotated element -- its OWN rotated frame, instead of one region's box). A scale OR
// rotate gesture is NOT a T0244 snap-eligible drag (site/snap.mjs) -- these drag objects
// never carry snapCandidates/snapBBox/activeGuides, so drawSnapGuides's early-return keeps
// it silent for both.
const SCALE_HANDLE_HALF = 5; // half a handle square, CSS px
const SCALE_HANDLE_TOL = 8; // hit tolerance around a handle centre, CSS px
const SCALE_MIN_SIZE = 4; // floor for a resized element/group w or h
const ROTATE_HANDLE_OFFSET = 24; // stem length beyond the (rotated) top-center handle, CSS px
const ROTATE_HANDLE_RADIUS = 6; // knob glyph radius, CSS px
const ROTATE_HANDLE_TOL = 8; // hit tolerance around the knob centre, CSS px (matches SCALE_HANDLE_TOL)

// The nodes a scale gesture directly targets: every selected element (image or text) plus
// every selected group's OWN frame entry -- a group's descendants are NOT added here (they
// never get their own handles/hit-test); instead, applyGroupScalePreview scales them
// separately off the group's own item entry (T0271: subtree-scale is now the DEFAULT --
// see beginScaleDrag/applyGroupScalePreview below; Ctrl+drag keeps the ORIGINAL T0232 Q2
// frame-only behavior, children pinned). `box` is the node's CURRENT frame; called fresh
// each frame for drawing/hit-testing, and ONCE at grab time to seed a drag (drag.items then
// holds that frozen snapshot for the rest of the gesture).
function collectResizeItems() {
  const items = [];
  for (const element of selectedElements()) {
    const isText = element.type === "text";
    // A NOTE resizes its FIXED box (w/h change, text re-wraps) like an image — NOT like text
    // (which scales its font). Flagged so beginScaleDrag excludes it from image-style
    // aspect-lock (a note box resizes freely on both axes).
    const isNote = element.type === "note";
    items.push({
      kind: "element",
      id: element.id,
      ref: element,
      box: { x: element.x, y: element.y, w: element.w, h: element.h },
      isText,
      isNote,
      origFontSize: isText ? Number((element.style || {}).fontSize) || 24 : undefined,
    });
  }
  for (const groupId of state.selectedGroupIds) {
    const group = groupById(groupId);
    if (!group) continue;
    items.push({ kind: "group", id: group.id, ref: group, box: { x: group.x, y: group.y, w: group.w, h: group.h } });
  }
  return items;
}

// The lone selected ELEMENT (image or text), or null when the selection is empty, a group,
// or a multi-selection -- the rotate handle (and a scale drag's rotation-aware math) only
// ever act on exactly one element (T0232 R4/R10: rotate-as-a-block is explicitly deferred;
// groups have no rotation field at all).
function singleSelectedElement() {
  if (state.selectedIds.size !== 1 || state.selectedGroupIds.size !== 0) return null;
  const [element] = selectedElements();
  return element || null;
}

function scaleHandlePoints(box) {
  return SCALE_HANDLES.map((handle) => ({ ...handle, x: box.x + box.w * handle.fx, y: box.y + box.h * handle.fy }));
}

// The current resize target(s)' WORLD-space handle points -- a SOLO rotated element draws
// and hit-tests its 8 handles on ITS OWN rotated frame (rotatedHandlePoints, T0232 3b R4:
// "8 scale handles at rotatedCorners/edge-mids"); every other case (multi-selection, a
// group, or an unrotated element) keeps the plain AABB fan unchanged from increment 2 --
// rotate-as-a-block stays deferred, and an unrotated element's handles are numerically
// identical either way (rotatedHandlePoints' identity fast path). Shared by the draw pass
// and the hit-test so both agree on the exact same points.
function resizeHandleWorldPoints(items) {
  if (items.length === 1 && items[0].kind === "element") {
    const rotation = Number(items[0].ref.rotation) || 0;
    if (rotation) return rotatedHandlePoints({ ...items[0].box, rotation });
  }
  return scaleHandlePoints(unionBBox(items.map((item) => item.box)));
}

// Screen-space hit-test for the selection's resize handles -- checked BEFORE element
// hit-test/move in onMouseDown (grab a handle before falling through to move), the same
// precedence dragRegionResize takes over dragRegionMove; also backs the idle hover cursor.
// Mode B (region-edit isolation) and the inline text editor own their own handles/overlay,
// so neither shows the generic gizmo.
function hitScaleHandle(screen) {
  if (regionEditElement() || state.editingTextId) return null;
  const items = collectResizeItems();
  if (!items.length) return null;
  for (const point of resizeHandleWorldPoints(items)) {
    const screenPt = imageToScreenPoint(point, state.viewport);
    if (Math.abs(screen.x - screenPt.x) <= SCALE_HANDLE_TOL && Math.abs(screen.y - screenPt.y) <= SCALE_HANDLE_TOL) {
      return { ...point, x: screenPt.x, y: screenPt.y };
    }
  }
  return null;
}

// The rotate handle's SCREEN position: the element's own rotated top-center handle point
// (rotatedHandlePoints' "n" entry -- correct for both a rotated AND an unrotated element,
// since rotation absent/0 collapses to the plain top-center), pushed a further FIXED
// ROTATE_HANDLE_OFFSET screen px along the SAME rotated "up" direction (rotationUpVector) --
// a Figma-style knob on a short stem, always the same visual length regardless of zoom.
function rotateHandleScreenPoint(element, vp) {
  const box = { x: element.x, y: element.y, w: element.w, h: element.h, rotation: Number(element.rotation) || 0 };
  const topCenterWorld = rotatedHandlePoints(box).find((point) => point.key === "n");
  const topCenterScreen = imageToScreenPoint(topCenterWorld, vp);
  const up = rotationUpVector(box.rotation);
  return { x: topCenterScreen.x + up.x * ROTATE_HANDLE_OFFSET, y: topCenterScreen.y + up.y * ROTATE_HANDLE_OFFSET };
}

// Hit-test for the rotate handle (the single knob floating above a SOLO selected element's
// top-center) -- checked BEFORE hitScaleHandle in onMouseDown (it floats OUTSIDE the box, so
// it is always the most specific target under the cursor when hit, same "most specific wins"
// precedence hitScaleHandle already follows over move). Returns the element itself (the drag
// needs its full box + current rotation), or null.
function hitRotateHandle(screen) {
  if (regionEditElement() || state.editingTextId) return null;
  const element = singleSelectedElement();
  if (!element) return null;
  const point = rotateHandleScreenPoint(element, state.viewport);
  if (Math.abs(screen.x - point.x) <= ROTATE_HANDLE_TOL && Math.abs(screen.y - point.y) <= ROTATE_HANDLE_TOL) return element;
  return null;
}

// 8 resize handles on the current selection's AABB, drawn in the chrome pass (on top of
// every group frame/selection stroke). Reads LIVE selection state every call (not cached),
// so the handles track a scale drag's own live preview the same way the selection stroke
// on each element does.
function drawSelectionHandles(vp) {
  if (regionEditElement() || state.editingTextId) return;
  if (drag && drag.mode === "marquee") return;
  const items = collectResizeItems();
  if (!items.length) return;
  ctx.save();
  ctx.lineWidth = 1;
  for (const point of resizeHandleWorldPoints(items)) {
    const screenPt = imageToScreenPoint(point, vp);
    ctx.fillStyle = "#77a7ff";
    ctx.fillRect(screenPt.x - SCALE_HANDLE_HALF, screenPt.y - SCALE_HANDLE_HALF, SCALE_HANDLE_HALF * 2, SCALE_HANDLE_HALF * 2);
    ctx.strokeStyle = "#1a1f2b";
    ctx.strokeRect(screenPt.x - SCALE_HANDLE_HALF, screenPt.y - SCALE_HANDLE_HALF, SCALE_HANDLE_HALF * 2, SCALE_HANDLE_HALF * 2);
  }
  ctx.restore();
  // T0232 increment 3b: the rotate handle -- a knob on a short stem above a SOLO selected
  // element's (rotated) top-center -- draws in the same chrome pass, right after the scale
  // handles, for a single element/text selection only (singleSelectedElement's guard).
  drawRotateHandle(vp);
}

function drawRotateHandle(vp) {
  const element = singleSelectedElement();
  if (!element) return;
  const box = { x: element.x, y: element.y, w: element.w, h: element.h, rotation: Number(element.rotation) || 0 };
  const topCenterScreen = imageToScreenPoint(rotatedHandlePoints(box).find((point) => point.key === "n"), vp);
  const knob = rotateHandleScreenPoint(element, vp);
  ctx.save();
  ctx.strokeStyle = "#77a7ff";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(topCenterScreen.x, topCenterScreen.y);
  ctx.lineTo(knob.x, knob.y);
  ctx.stroke();
  ctx.beginPath();
  ctx.arc(knob.x, knob.y, ROTATE_HANDLE_RADIUS, 0, Math.PI * 2);
  ctx.fillStyle = "#77a7ff";
  ctx.fill();
  ctx.strokeStyle = "#1a1f2b";
  ctx.stroke();
  ctx.restore();
}

// Begin a "scale" drag: snapshot every resize target's box ONCE at grab (drag.items), plus
// the selection's original AABB (drag.origAABB) the live preview and commit both key off of.
// `proportionalDefault` is true only when EVERY target is a plain image element (single or a
// pure multi-image block) -- "sprites keep proportions by default, Shift = free distort"
// (the design doc's default); a group or a text element in the mix defaults to free resize
// (Shift then LOCKS it), since neither has an intrinsic "aspect" the way a bitmap does.
// A frozen `{groups, elements}`-shaped snapshot of a group's OWN frame plus its FULL
// descendant closure (`descendantsOf` -- nested subgroup frames AND every element in the
// subtree), captured ONCE at scale-drag grab (beginScaleDrag). This is the exact shape
// `tree.scaleGroupMoves` expects as its `project` argument, so the live preview
// (applyGroupScalePreview below) maps FROM the SAME pure math `scaleGroup` commits
// server-side -- page and op can never disagree. A shallow-per-node clone (own fields + a
// cloned `style` object) is enough: nothing here reads nested objects deeper than
// `style.fontSize`, and cloning keeps this snapshot immune to this SAME drag's own later
// preview writes onto the real live elements/groups. Frozen at grab, never re-read after --
// every mousemove maps from this ORIGINAL, not from a previous frame's own preview, so
// toggling Ctrl mid-drag always recomputes cleanly (T0271).
function snapshotGroupSubtree(groupId) {
  const group = groupById(groupId);
  const { groups: descGroups, elements: descElements } = descendantsOf(state.project, groupId);
  return {
    groups: [group, ...descGroups].map((g) => ({ ...g })),
    elements: descElements.map((e) => ({ ...e, style: e.style ? { ...e.style } : e.style })),
  };
}

function beginScaleDrag(handle, screen, world) {
  const items = collectResizeItems();
  if (!items.length) return;
  const origAABB = unionBBox(items.map((item) => item.box));
  // Only a pure block of IMAGE elements keeps proportions by default; text (font scale) and
  // notes (free-resize fixed box, T0268) both default to free resize.
  const proportionalDefault = items.every((item) => item.kind === "element" && !item.isText && !item.isNote);
  // T0232 increment 3b: a SOLO rotated element scales in its OWN local (rotated) frame --
  // see resizeRotatedBox's header in viewport.mjs -- so its rotated anchor corner/edge stays
  // put in WORLD space. Every other case (multi-selection, a group, or an unrotated element)
  // keeps the plain AABB block-scale path unchanged (`rotation` 0 -> the mousemove "scale"
  // case below takes the ORIGINAL branch verbatim).
  const rotation = items.length === 1 && items[0].kind === "element" ? Number(items[0].ref.rotation) || 0 : 0;
  // T0271: snapshot every selected GROUP's subtree ONCE at grab -- see snapshotGroupSubtree.
  for (const item of items) {
    if (item.kind === "group") item.subtreeSnapshot = snapshotGroupSubtree(item.id);
  }
  drag = { mode: "scale", startX: screen.x, startY: screen.y, grabWorld: world, handle, items, origAABB, proportionalDefault, rotation };
}

// Live drag-preview for ONE selected GROUP item's scale (T0271 -- the lead's override of
// T0232 Q2's shipped frame-only default). `mapped` is this item's own new frame (the SAME
// per-item `mapItemBox` mapping every other selected item already goes through in the
// mousemove "scale" case below); `frameOnly` is the live Ctrl/Cmd modifier read fresh every
// event, exactly like Shift/Alt.
//
// `frameOnly` true (Ctrl/Cmd held) = the ORIGINAL T0232 behavior: only the group's own w/h
// changes (x/y stay pinned at grab, the existing v1 skeleton limitation), and every
// descendant is restored to its GRABBED original from the frozen snapshot -- unconditionally,
// every frame, so a mode flip mid-drag (content-scale on a previous frame, then Ctrl pressed)
// always lands on a clean "children exactly as grabbed" state rather than whatever the last
// content-mode frame left behind.
//
// `frameOnly` false (the NEW default) = the group's own frame moves/resizes to `mapped` too
// (no longer pinned), and its full descendant closure is remapped by feeding the FROZEN
// grab-time snapshot through `tree.scaleGroupMoves` -- the exact same pure function
// `scaleGroup` (ops.mjs) uses server-side, so the live preview and the eventual commit can
// never numerically disagree. Recomputed from the snapshot fresh every call (never from the
// previous frame's own mutated refs), so this is order-independent across mousemove events
// exactly like the plain block-scale path above it (`resizeBox(drag.origAABB, ...)`).
function applyGroupScalePreview(item, mapped, frameOnly) {
  const snapshot = item.subtreeSnapshot;
  if (!snapshot) return; // defensive: every group item gets one in beginScaleDrag
  if (frameOnly) {
    for (const g of snapshot.groups) {
      if (g.id === item.id) continue;
      const live = groupById(g.id);
      if (live) Object.assign(live, { x: g.x, y: g.y, w: g.w, h: g.h });
    }
    for (const e of snapshot.elements) {
      const live = elements().find((element) => element.id === e.id);
      if (!live) continue;
      live.x = e.x;
      live.y = e.y;
      live.w = e.w;
      live.h = e.h;
      if (e.type === "text") live.style = { ...live.style, fontSize: Number((e.style || {}).fontSize) || 24 };
    }
    item.ref.w = Math.max(SCALE_MIN_SIZE, mapped.w);
    item.ref.h = Math.max(SCALE_MIN_SIZE, mapped.h);
    return;
  }
  const newFrame = { x: mapped.x, y: mapped.y, w: Math.max(SCALE_MIN_SIZE, mapped.w), h: Math.max(SCALE_MIN_SIZE, mapped.h) };
  const patches = scaleGroupMoves(snapshot, item.id, newFrame);
  for (const patch of patches) {
    if (patch.kind === "group") {
      const live = patch.id === item.id ? item.ref : groupById(patch.id);
      if (live) Object.assign(live, { x: patch.x, y: patch.y, w: patch.w, h: patch.h });
      continue;
    }
    const live = elements().find((element) => element.id === patch.id);
    if (!live) continue;
    live.x = patch.x;
    live.y = patch.y;
    if (patch.fontSize !== undefined) live.style = { ...live.style, fontSize: patch.fontSize };
    else {
      live.w = patch.w;
      live.h = patch.h;
    }
  }
}

// Begin a "rotate" drag: the pivot is the element's OWN box center at grab time (rotation
// never moves the box, so this stays valid for the whole gesture); `startWorld` anchors the
// angle delta (rotationFromDrag), `baseRotation` is the element's rotation when grabbed.
function beginRotateDrag(element, screen, world) {
  const center = { x: element.x + element.w / 2, y: element.y + element.h / 2 };
  drag = { mode: "rotate", startX: screen.x, startY: screen.y, elementId: element.id, center, startWorld: world, baseRotation: Number(element.rotation) || 0 };
}

// Commit a finished scale drag. Element geometry (images + text) always batches through ONE
// elements-set call -- one journal entry / one undo, whether 1 or N elements resized (mirrors
// commitElementDrag, which does the same for a move). A selected GROUP's frame has no
// batched-resize op (T0232 §2b: patchGroup is per-group, frame-only, no `patchGroups`-style
// batch reused here) -- a gesture that resizes 2+ groups, or a MIX of elements and groups,
// therefore lands as more than one journal entry (one Ctrl+Z per group patch/scale); a pure
// element-only block or a single solo group still commits as exactly one undo, matching the
// doc's two documented cases. A future combined resize op (moveNodes's resize sibling) would
// close that gap; out of scope here. T0271: each group item commits through ONE of two
// routes depending on `finished.groupFrameOnly` (the last-known Ctrl/Cmd state stashed by
// the mousemove "scale" case, since `onMouseUp` itself carries no modifier state) -- the
// EXISTING frame-only `PATCH .../groups/<gid> {w,h}` when Ctrl was held, or the NEW
// `POST .../groups/<gid>/scale {x,y,w,h}` (the default): the server computes every
// descendant patch from the final frame via `scaleGroup`/`tree.scaleGroupMoves`, so the page
// never sends descendant patches itself (page and op can't disagree).
function commitScaleDrag(finished) {
  const projectId = state.project.id;
  const frameOnly = !!finished.groupFrameOnly;
  const elementPatches = [];
  const groupPatches = []; // frame-only PATCH {w,h} (Ctrl held)
  const groupScales = []; // content-mode POST /scale {x,y,w,h} (the default)
  for (const item of finished.items) {
    const orig = item.box;
    if (item.kind === "group") {
      const x = Math.round(item.ref.x);
      const y = Math.round(item.ref.y);
      const w = Math.round(item.ref.w);
      const h = Math.round(item.ref.h);
      if (frameOnly) {
        // No x/y here on purpose (see the mousemove "scale" case's group branch): only w/h
        // ever moved, so patchGroup's move-cascade (triggered by ANY x/y change) never fires.
        if (w !== Math.round(orig.w) || h !== Math.round(orig.h)) {
          groupPatches.push({ groupId: item.id, w, h });
        }
      } else if (x !== Math.round(orig.x) || y !== Math.round(orig.y) || w !== Math.round(orig.w) || h !== Math.round(orig.h)) {
        groupScales.push({ groupId: item.id, x, y, w, h });
      }
      continue;
    }
    const x = Math.round(item.ref.x);
    const y = Math.round(item.ref.y);
    if (item.isText) {
      const fontSize = Math.round(Number((item.ref.style || {}).fontSize) || item.origFontSize);
      if (x !== Math.round(orig.x) || y !== Math.round(orig.y) || fontSize !== Math.round(item.origFontSize)) {
        elementPatches.push({ elementId: item.id, x, y, style: { fontSize } });
      }
      continue;
    }
    const w = Math.round(item.ref.w);
    const h = Math.round(item.ref.h);
    if (x !== Math.round(orig.x) || y !== Math.round(orig.y) || w !== Math.round(orig.w) || h !== Math.round(orig.h)) {
      elementPatches.push({ elementId: item.id, x, y, w, h });
    }
  }
  if (!elementPatches.length && !groupPatches.length && !groupScales.length) {
    refresh();
    return;
  }
  (async () => {
    try {
      let result = null;
      if (elementPatches.length) {
        result = await api("POST", `/projects/${projectId}/elements-set`, { patches: elementPatches });
      }
      for (const patch of groupPatches) {
        const { groupId, ...body } = patch;
        result = await api("PATCH", `/projects/${projectId}/groups/${groupId}`, body);
      }
      for (const scale of groupScales) {
        const { groupId, ...body } = scale;
        result = await api("POST", `/projects/${projectId}/groups/${groupId}/scale`, body);
      }
      applyMutation(result);
    } catch (error) {
      setStatus(error.message, true);
    }
  })();
}

// Commit a finished rotate drag: ONE patchElement({rotation}) -- the SAME PATCH path the
// inspector's Rotation input and the CLI's `element-set --rotation` use (README "Rotation &
// flip": "whatever the page can set, the CLI/API can set identically"), so a rotate-handle
// gesture is a single journal entry / one Ctrl+Z, exactly like a scale or move. Rounded to
// the nearest whole degree at commit (T0236 fractional-during/round-at-commit law, same as
// every other drag in this file); a no-op gesture (released back at the grabbed angle)
// writes no entry.
function commitRotateDrag(finished) {
  const element = elements().find((item) => item.id === finished.elementId);
  if (!element) {
    refresh();
    return;
  }
  const rotation = Math.round(Number(element.rotation) || 0) % 360;
  if (rotation === Math.round(finished.baseRotation)) {
    refresh();
    return;
  }
  patchElementBox(finished.elementId, { rotation });
}

function updateZoomIndicator() {
  const node = el("zoom-indicator");
  if (node) node.textContent = `${Math.round(state.viewport.scale * 100)}%`;
}

function updateEmptyHint() {
  const node = el("empty-hint");
  if (node) node.classList.toggle("hidden", elements().length > 0 || groups().length > 0);
}

// ---- viewport ----------------------------------------------------------------

function contentBounds() {
  const boxes = [
    ...elements().filter((element) => !isElementHidden(element)),
    ...groups().filter((group) => !isNodeHidden(state.project, group)),
  ];
  if (!boxes.length) return { x: 0, y: 0, width: 1024, height: 768 };
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const box of boxes) {
    minX = Math.min(minX, box.x);
    minY = Math.min(minY, box.y);
    maxX = Math.max(maxX, box.x + box.w);
    maxY = Math.max(maxY, box.y + box.h);
  }
  return { x: minX, y: minY, width: Math.max(1, maxX - minX), height: Math.max(1, maxY - minY) };
}

export function fit() {
  resizeCanvas();
  const bounds = contentBounds();
  const base = fitViewport({
    imageWidth: bounds.width,
    imageHeight: bounds.height,
    frameWidth: state.cssWidth,
    frameHeight: state.cssHeight,
    padding: 48,
  });
  state.viewport = {
    scale: base.scale,
    offsetX: base.offsetX - bounds.x * base.scale,
    offsetY: base.offsetY - bounds.y * base.scale,
  };
  render();
}

// Zoom to an exact scale, keeping the stage center fixed (used by 1 = 100%, 2 = 200%).
export function zoomTo(scale) {
  const center = { x: state.cssWidth / 2, y: state.cssHeight / 2 };
  const world = screenToImagePoint(center, state.viewport);
  const next = clamp(scale, 0.05, 12);
  state.viewport = { scale: next, offsetX: center.x - world.x * next, offsetY: center.y - world.y * next };
  render();
}

// ---- top bar -----------------------------------------------------------------

export function syncTopBar() {
  const title = el("ws-title");
  if (title && state.project) title.textContent = state.project.title;
  const undoBtn = el("undo");
  const redoBtn = el("redo");
  if (undoBtn) undoBtn.disabled = !state.history.canUndo;
  if (redoBtn) redoBtn.disabled = !state.history.canRedo;
  for (const button of document.querySelectorAll("#tool-rail .tool")) {
    button.classList.toggle("active", button.dataset.tool === state.tool);
  }
}

// ---- pointer interaction -----------------------------------------------------

let drag = null;

function pointer(event) {
  const rect = dragCanvasRect || canvas.getBoundingClientRect();
  return { x: event.clientX - rect.left, y: event.clientY - rect.top };
}

// Capture the stage/canvas rects for the life of a drag (see dragCanvasRect note). Called
// once a mousedown has actually started a drag; cleared on mouseup / resize.
function beginDragGeometryCache() {
  const stage = el("stage");
  dragStageRect = stage ? stage.getBoundingClientRect() : null;
  dragCanvasRect = canvas ? canvas.getBoundingClientRect() : null;
}

function clearDragGeometryCache() {
  dragStageRect = null;
  dragCanvasRect = null;
}

function setCursor(name) {
  if (canvas) canvas.style.cursor = name;
}

// Does a group carry a valid solid background fill? (mirrors fillGroupBackground's guard.)
// A filled frame is click-selectable by its body; an unfilled one stays passthrough.
function hasGroupFill(group) {
  const bg = group.background;
  return !!bg && bg.type === "color" && /^#[0-9a-fA-F]{6}$/.test(String(bg.color || ""));
}

// T0253 F4: onHover (idle mousemove) calls hitElement TWICE with the SAME (world,
// project) point -- once for the hover-group affordance (updateHoverGroup), once for
// the cursor (updateCursorAt) -- each paying a full front-to-back tree walk. This
// single-slot memo turns the second call into an O(1) lookup. Keyed on project
// identity (not just x/y), so a fresh project object (any op/undo/redo/reload) always
// misses -- it can never serve a stale result. A pure function of (state.project,
// world.x, world.y), so an exact-key cache is always correct (not just "close enough"),
// and safe to share across every call site (mousedown/dblclick/context-menu too), not
// just the idle-hover pair.
let hitCacheProject = null;
let hitCacheX = NaN;
let hitCacheY = NaN;
let hitCacheResult = null;

// Top-most VISIBLE hittable NODE under a world point, in COMPUTED paint order (front =
// painted last = on top). Walks the scene tree front-to-back so z-order + nesting agree
// with what is drawn. Returns an image/text element, OR a GROUP when the point lands on the
// empty BODY of a group that carries a background fill (T0224 item 2): a Figma frame with a
// fill occludes what is behind it, so its fill sits at the group's own z-slot — checked
// AFTER its children (which paint on top of the fill) and before the siblings behind it. An
// UNFILLED frame stays passthrough (deliberate: a marquee still starts on empty frame
// interior). Clipped-out areas stay unhittable (the clip guard skips the whole subtree AND
// the fill when the point is outside a clipping ancestor's box).
function hitElement(world) {
  if (hitCacheProject === state.project && hitCacheX === world.x && hitCacheY === world.y) {
    return hitCacheResult;
  }
  // T0232 increment 3b: rotation-aware -- a rotated element/group hit-tests by its TRUE
  // (rotated) footprint, not its stale unrotated box (pointInRotatedBox's identity fast
  // path keeps every unrotated node's hit-test byte-identical to before this increment;
  // groups never carry `rotation`, so they always take that fast path).
  const inBox = (e) => pointInRotatedBox(world, e);
  const walk = (scopeId) => {
    const children = orderedChildren(state.project, scopeId);
    for (let i = children.length - 1; i >= 0; i -= 1) {
      const child = children[i];
      if (isNodeHidden(state.project, child.ref)) continue;
      if (child.kind === "group") {
        // A clip=true group makes its interior the only hit-testable region: a point
        // outside its box can't hit anything in its (clipped-away) subtree. This also
        // enforces "outside ANY clipping ancestor" — an excluded outer group's whole
        // subtree, including nested groups, is skipped.
        if (child.ref.clip === true && !inBox(child.ref)) continue;
        const found = walk(child.id);
        if (found) return found;
        // No child hit: a FILLED group's body is selectable (occludes behind it); an
        // unfilled frame is transparent to the click (falls through to siblings/marquee).
        if (hasGroupFill(child.ref) && inBox(child.ref)) return child.ref;
      } else if (inBox(child.ref)) {
        return child.ref;
      }
    }
    return null;
  };
  const result = walk(null);
  hitCacheProject = state.project;
  hitCacheX = world.x;
  hitCacheY = world.y;
  hitCacheResult = result;
  return result;
}

// Resolve a Figma single-click on NODE `e` (an element OR a filled group body) given the
// entered scope: select the node itself when it lives directly in that scope, else the
// TOP-MOST ancestor group that is a child of that scope. Returns { kind, id, scope } —
// `scope` becomes the new enteredGroupId (clicking outside the entered group resolves at
// root, i.e. steps out). `kind` is "group" when the resolved node is a group (a filled
// group body hit, or an ancestor container), else "element" (T0224 item 2 generalized it
// from element-only to any node).
function resolveClickSelection(e, enteredGroupId) {
  const project = state.project;
  const isGroup = groupById(e.id) != null;
  const chain = ancestorsOf(project, e).map((group) => group.id); // nearest ... top-level
  const scope = enteredGroupId && chain.includes(enteredGroupId) ? enteredGroupId : null;
  if (nodeScope(project, e) === scope) return { kind: isGroup ? "group" : "element", id: e.id, scope };
  for (const gid of chain) {
    const group = groupById(gid);
    if (group && (group.parentId || null) === scope) return { kind: "group", id: gid, scope };
  }
  return { kind: isGroup ? "group" : "element", id: e.id, scope };
}

// A drag item for a selected group: its full descendant closure captured at grab time so
// the live preview moves the whole subtree (frames + elements); the commit patchGroup
// then cascades identically on the server.
function groupDragItem(groupId) {
  const group = groupById(groupId);
  if (!group) return null;
  const desc = descendantsOf(state.project, groupId);
  return {
    group,
    origX: group.x,
    origY: group.y,
    members: desc.elements.map((element) => ({ element, origX: element.x, origY: element.y })),
    subgroups: desc.groups.map((sub) => ({ group: sub, origX: sub.x, origY: sub.y })),
  };
}

// Start a drag matching the current selection: a lone group moves its whole subtree; a
// pure element set moves via the element batch; a mix (or 2+ groups) moves as a combined
// "selection" drag (loose elements batched + each group cascaded).
// `world` is the grabbed point in IMAGE space (screenToImagePoint at grab time). It is the
// drag anchor: every move recomputes the delta from the CURRENT pointer's image point minus
// this fixed world anchor (dragWorldDelta), so a mid-drag wheel-zoom stays glued to the
// cursor without any rebase. startX/startY are kept only for the mouseup cursor fallback.
function beginSelectionDrag(screen, world) {
  const groupIds = [...state.selectedGroupIds];
  const elItems = selectedElements().map((element) => ({ element, origX: element.x, origY: element.y }));

  // T0244 smart-guide precompute -- ONCE per drag (T0236 perf law: no per-frame candidate
  // collection, no getBoundingClientRect, no refresh() in the hot mousemove path). Candidates
  // and the selection's ORIGINAL union bbox never change during a pure translate, so onMouseMove
  // only ever re-runs the cheap snapDelta arithmetic against these precomputed arrays.
  const draggedIds = [...state.selectedIds, ...state.selectedGroupIds];
  const origFrames = [
    ...elItems.map((it) => ({ x: it.element.x, y: it.element.y, w: it.element.w, h: it.element.h })),
    ...groupIds.map((gid) => groupById(gid)).filter(Boolean).map((g) => ({ x: g.x, y: g.y, w: g.w, h: g.h })),
  ];
  const snapCandidates = collectSnapCandidates(state.project, draggedIds);
  const snapBBox = unionBBox(origFrames);

  if (groupIds.length === 0) {
    drag = { mode: "element", startX: screen.x, startY: screen.y, grabWorld: world, items: elItems, snapCandidates, snapBBox, activeGuides: [] };
    return;
  }
  const grpItems = groupIds.map((gid) => groupDragItem(gid)).filter(Boolean);
  if (groupIds.length === 1 && elItems.length === 0) {
    const only = grpItems[0];
    drag = { mode: "group", startX: screen.x, startY: screen.y, grabWorld: world, group: only.group, origGroup: { x: only.origX, y: only.origY }, members: only.members, subgroups: only.subgroups, snapCandidates, snapBBox, activeGuides: [] };
    return;
  }
  drag = { mode: "selection", startX: screen.x, startY: screen.y, grabWorld: world, elItems, grpItems, snapCandidates, snapBBox, activeGuides: [] };
}

function hitGroupLabel(screen) {
  for (let i = groupLabelRects.length - 1; i >= 0; i -= 1) {
    const r = groupLabelRects[i];
    if (screen.x >= r.x && screen.x <= r.x + r.w && screen.y >= r.y && screen.y <= r.y + r.h) return r.groupId;
  }
  return null;
}

// (groupAtCenter removed 2026-07-02 with geometric drop-reparenting — group
// membership changes are explicit only: layers drag, Ctrl+G, Ungroup, CLI.)

// ---- region gesture starts ---------------------------------------------------

function beginRegionResize(element, grabbed, screen) {
  const { sx, sy } = scaleFactors(element);
  drag = {
    mode: "region-resize",
    element,
    region: grabbed.region,
    handle: grabbed.handle,
    orig: [...regionRect(grabbed.region)],
    // A polygonal region rescales its ring with the bbox; capture the original points.
    origPolygon: Array.isArray(grabbed.region.polygon) ? grabbed.region.polygon.map((p) => [...p]) : null,
    startX: screen.x,
    startY: screen.y,
    // World-space grab anchor (see beginSelectionDrag): the source-pixel delta is derived
    // from the CURRENT pointer's image point minus this, so a mid-drag zoom stays anchored.
    grabWorld: screenToImagePoint(screen, state.viewport),
    sx,
    sy,
    cursor: grabbed.handle.cursor,
    changed: false,
  };
  setCursor(grabbed.handle.cursor);
}

function beginRegionSelectMove(element, region, screen, shift) {
  if (shift) {
    if (state.selectedRegionIds.has(region.id)) state.selectedRegionIds.delete(region.id);
    else state.selectedRegionIds.add(region.id);
  } else if (!state.selectedRegionIds.has(region.id)) {
    state.selectedRegionIds = new Set([region.id]);
  }
  state.expandedElements.add(element.id);
  if (state.selectedRegionIds.has(region.id)) {
    const { sx, sy } = scaleFactors(element);
    const items = (element.regions || [])
      .filter((r) => state.selectedRegionIds.has(r.id) && regionRect(r))
      .map((r) => ({
        region: r,
        orig: [...regionRect(r)],
        // Capture the polygon ring so a move translates it with the bbox (one commit).
        origPolygon: Array.isArray(r.polygon) ? r.polygon.map((p) => [...p]) : null,
      }));
    drag = { mode: "region-move", element, items, startX: screen.x, startY: screen.y, grabWorld: screenToImagePoint(screen, state.viewport), sx, sy, changed: false };
    setCursor("move");
  } else {
    drag = null;
  }
  refresh();
}

function beginRegionCreate(element, screen, world) {
  drag = { mode: "region-create", element, startScreen: screen, lastScreen: screen, startWorld: world };
  setCursor("crosshair");
}

function pointInElement(world, element) {
  return world.x >= element.x && world.x <= element.x + element.w && world.y >= element.y && world.y <= element.y + element.h;
}

function onMouseDown(event) {
  if (event.button !== 0 && event.button !== 1) return;
  // Cache the stage/canvas rects for the whole gesture up front (cleared in onMouseUp): the
  // pointer() below and every move frame then read the cache instead of forcing a reflow.
  beginDragGeometryCache();
  const screen = pointer(event);
  const world = screenToImagePoint(screen, state.viewport);
  const wantPan = event.button === 1 || state.tool === "pan" || state.spacePan;

  if (wantPan) {
    // Pan grabs a WORLD point and keeps it under the cursor (panOffsetFor). Storing the
    // grabbed world point (not the start offset) makes a mid-pan wheel-zoom self-correct:
    // the next move re-solves the offset for the current scale. onWheel rebases grabWorld
    // to the wheel's anchor so the zoom itself is seamless.
    drag = { mode: "pan", startX: screen.x, startY: screen.y, grabWorld: world };
    setCursor("grabbing");
    return;
  }

  // T tool: drop a text element at the click point (Figma-style), then edit it inline.
  if (state.tool === "text") {
    placeTextAt(world);
    return;
  }

  // N tool: drop a note card at the click point (T0268), then edit it inline.
  if (state.tool === "note") {
    placeNoteAt(world);
    return;
  }

  // MODE B (region-edit isolation): only the isolated element's regions respond, and
  // the active region tool decides what a press on the image does.
  const editEl = regionEditElement();
  if (editEl) {
    const tool = state.regionTool;
    if (tool === "polygon") {
      if (pointInElement(world, editEl)) {
        // Double-click (detail>1) on a >=3-point draft closes it; otherwise place a vertex.
        if (event.detail > 1 && state.polygonDraft.length >= 3) finishPolygonDraft();
        else addPolygonVertex(editEl, screen);
        return;
      }
      // Outside the image: never abandon an in-progress draft on a stray click.
      if (state.polygonDraft.length) return;
      exitRegionEdit();
      refresh();
    } else {
      // Select tool reserves the region body for move/resize; Rect tool always draws
      // (a rubber-band NEW rect region), even on top of existing regions.
      if (tool === "select" && state.selectedRegionIds.size) {
        const grabbed = hitRegionHandle(screen, editEl, state.selectedRegionIds, state.viewport);
        if (grabbed) {
          beginRegionResize(editEl, grabbed, screen);
          return;
        }
      }
      if (pointInElement(world, editEl)) {
        if (tool === "select") {
          const region = hitRegion(world, editEl);
          if (region) {
            beginRegionSelectMove(editEl, region, screen, event.shiftKey);
            return;
          }
        }
        // Empty area (select) or anywhere (rect) -> rubber-band a NEW rect region.
        beginRegionCreate(editEl, screen, world);
        return;
      }
      // Clicked outside the isolated image -> exit isolation, then handle as mode A.
      exitRegionEdit();
      refresh();
    }
  }

  // T0232 increment 3b: grab the rotate handle BEFORE the scale handles/element hit-test --
  // it floats OUTSIDE the box, so it is always the most specific target under the cursor
  // when hit (same precedence rule the scale-handle check right below already follows).
  const grabbedRotate = hitRotateHandle(screen);
  if (grabbedRotate) {
    beginRotateDrag(grabbedRotate, screen, world);
    setCursor("grabbing");
    return;
  }

  // T0232 increment 2: grab a resize handle on the current selection's AABB BEFORE falling
  // through to element hit-test/move or a group-label click -- same precedence
  // dragRegionResize takes over dragRegionMove (a handle is always the most specific target
  // under the cursor). No-op when nothing is selected (hitScaleHandle returns null).
  const grabbedHandle = hitScaleHandle(screen);
  if (grabbedHandle) {
    beginScaleDrag(grabbedHandle, screen, world);
    setCursor(grabbedHandle.cursor);
    return;
  }

  // A group LABEL always selects that group (chrome above the artwork) — dragging it
  // moves the whole subtree. Scope is unchanged (a label selects, it does not enter).
  const labelGroupId = hitGroupLabel(screen);
  if (labelGroupId) {
    // An already-selected group's label keeps the whole (multi) selection so a
    // press-drag moves it all together; a fresh label click selects that group.
    if (!state.selectedGroupIds.has(labelGroupId)) selectGroupOnly(labelGroupId);
    beginSelectionDrag(screen, world);
    setCursor("move");
    refresh();
    return;
  }

  // MODE A (object mode): Figma nested selection. Ctrl/Cmd+click deep-selects the leaf
  // (and enters its scope); a plain click selects the leaf when it lives directly in the
  // entered scope, else the top-most container group within that scope; Shift adds.
  const hit = hitElement(world);
  if (hit) {
    if (event.ctrlKey || event.metaKey) {
      // Deep-select the leaf under the cursor and enter its scope. A filled group body has
      // no deeper leaf — it selects the group itself (as a unit) at its parent scope.
      state.enteredGroupId = nodeScope(state.project, hit);
      if (groupById(hit.id)) selectGroupOnly(hit.id);
      else selectOnly(hit.id);
    } else {
      const res = resolveClickSelection(hit, state.enteredGroupId);
      state.enteredGroupId = res.scope;
      if (res.kind === "group") {
        if (event.shiftKey) {
          if (state.selectedGroupIds.has(res.id)) state.selectedGroupIds.delete(res.id);
          else state.selectedGroupIds.add(res.id);
          state.selectedRegionIds = new Set();
          state.regionEditId = null;
          syncPrimaryGroup();
        } else if (!state.selectedGroupIds.has(res.id)) {
          // A fresh group replaces the selection; an already-selected group keeps the
          // whole (possibly mixed) selection so a press-drag moves it all together —
          // same rule as the element branch below.
          selectGroupOnly(res.id);
        }
      } else if (event.shiftKey) {
        toggleSelect(hit.id);
      } else if (!state.selectedIds.has(hit.id)) {
        // A fresh element replaces the selection; an already-selected element keeps the
        // whole (possibly mixed) selection so a press-drag moves it all together.
        selectOnly(hit.id);
      }
    }
    beginSelectionDrag(screen, world);
    setCursor("move");
    refresh();
    return;
  }

  // Empty canvas -> marquee select at the CURRENT scope (panning is Hand/Space/middle-
  // mouse only). The selection clears now but the entered scope is kept so the marquee
  // selects within it; a plain click (no marquee drag) exits to root on mouseup.
  drag = {
    mode: "marquee",
    startScreen: screen, // kept only for the mouseup tap-vs-drag test (physical px)
    startWorld: world, // the marquee's anchor CORNER in image space (zoom-stable)
    lastScreen: screen,
    shift: event.shiftKey,
    base: event.shiftKey ? new Set(state.selectedIds) : new Set(),
    baseGroups: event.shiftKey ? new Set(state.selectedGroupIds) : new Set(),
  };
  if (!event.shiftKey) {
    state.selectedIds = new Set();
    state.selectedGroupId = null;
    state.selectedGroupIds = new Set();
    state.selectedRegionIds = new Set();
    state.regionEditId = null;
  }
  setCursor("crosshair");
  refresh();
}

function applyMarquee() {
  // Anchor corner is the FIXED world point grabbed at mousedown (drag.startWorld); only the
  // moving corner is re-projected from the live pointer. Reconverting a stored SCREEN start
  // point each frame (the old path) drifted the anchor the instant a mid-marquee zoom moved
  // the offset, so the rectangle no longer started where the press landed.
  const a = drag.startWorld;
  const b = screenToImagePoint(drag.lastScreen, state.viewport);
  const rx = Math.min(a.x, b.x);
  const ry = Math.min(a.y, b.y);
  const rw = Math.abs(b.x - a.x);
  const rh = Math.abs(b.y - a.y);
  const intersects = (box) => !(box.x + box.w < rx || box.x > rx + rw || box.y + box.h < ry || box.y > ry + rh);
  // Marquee selects nodes at the CURRENT scope: loose elements + groups-as-units (a
  // group's frame box). Root marquee = top-level groups + loose elements; inside an
  // entered group = its own children.
  const scope = childrenOf(state.project, state.enteredGroupId);
  const inside = new Set(drag.base);
  for (const element of scope.elements) {
    if (isElementHidden(element)) continue;
    // Test the element's VISIBLE (clipped) box: intersect it with every clipping ancestor
    // so a member cropped away by a clip frame isn't grabbed where it no longer shows. A
    // fully clipped-out element (clip null) is skipped.
    const box = visibleBox(element);
    if (box && intersects(box)) inside.add(element.id);
  }
  const insideGroups = new Set(drag.baseGroups || []);
  for (const group of scope.groups) {
    if (isNodeHidden(state.project, group)) continue;
    // A group is touched when the marquee hits its FRAME or any of its VISIBLE member
    // artwork — membership is derived from ancestry, never frame containment, so a member
    // parked OUTSIDE the frame is still grabbed via its group at root (invariant: parked
    // == in-frame). A clip:true member is tested by its visible (clipped) box, so a
    // cropped-away part never grabs.
    let touched = intersects(group);
    if (!touched) {
      for (const member of descendantsOf(state.project, group.id).elements) {
        if (isElementHidden(member)) continue;
        const box = visibleBox(member);
        if (box && intersects(box)) {
          touched = true;
          break;
        }
      }
    }
    if (touched) insideGroups.add(group.id);
  }
  state.selectedRegionIds = new Set();
  state.selectedIds = inside;
  state.selectedGroupIds = insideGroups;
  syncPrimaryGroup();
}

function dragRegionMove(screen) {
  const el2 = drag.element;
  const sw = el2.source_w || el2.w;
  const sh = el2.source_h || el2.h;
  // World-anchored delta (see beginSelectionDrag): the source-pixel delta is the CURRENT
  // pointer's image-space offset from the grab anchor divided by the element's scale, so a
  // mid-drag zoom stays anchored. Regions are source-pixel rects, so the delta is snapped to
  // whole source pixels (deliberate precision, not the screen-space stepping T0236 fixes).
  const { dx: dxWorld, dy: dyWorld } = dragWorldDelta(drag.grabWorld, screen, state.viewport);
  const dxSrc = Math.round(dxWorld / drag.sx);
  const dySrc = Math.round(dyWorld / drag.sy);
  for (const item of drag.items) {
    const [ox, oy, w, h] = item.orig;
    const nx = clamp(ox + dxSrc, 0, Math.max(0, sw - w));
    const ny = clamp(oy + dySrc, 0, Math.max(0, sh - h));
    item.region.rect = [nx, ny, w, h];
    // Translate the polygon with its bbox (same w/h → transformPolygon is a pure shift).
    if (item.origPolygon) item.region.polygon = transformPolygon(item.origPolygon, item.orig, [nx, ny, w, h]);
  }
  if (dxSrc !== 0 || dySrc !== 0) drag.changed = true;
}

function dragRegionResize(screen) {
  const el2 = drag.element;
  const sw = el2.source_w || el2.w;
  const sh = el2.source_h || el2.h;
  const MIN = 2;
  // World-anchored source-pixel delta (see dragRegionMove): stays glued to the cursor across
  // a mid-drag zoom instead of re-interpreting a stale screen anchor at the new scale.
  const { dx: dxWorld, dy: dyWorld } = dragWorldDelta(drag.grabWorld, screen, state.viewport);
  const dx = Math.round(dxWorld / drag.sx);
  const dy = Math.round(dyWorld / drag.sy);
  const [ox, oy, ow, oh] = drag.orig;
  let x = ox;
  let y = oy;
  let w = ow;
  let h = oh;
  if (drag.handle.fx === 0) {
    x = clamp(ox + dx, 0, ox + ow - MIN);
    w = ox + ow - x;
  } else if (drag.handle.fx === 1) {
    w = clamp(ow + dx, MIN, sw - ox);
  }
  if (drag.handle.fy === 0) {
    y = clamp(oy + dy, 0, oy + oh - MIN);
    h = oy + oh - y;
  } else if (drag.handle.fy === 1) {
    h = clamp(oh + dy, MIN, sh - oy);
  }
  drag.region.rect = [x, y, w, h];
  // Rescale the polygon proportionally into the new bbox (matches the legacy editor).
  if (drag.origPolygon) drag.region.polygon = transformPolygon(drag.origPolygon, drag.orig, [x, y, w, h]);
  if (x !== ox || y !== oy || w !== ow || h !== oh) drag.changed = true;
}

function onMouseMove(event) {
  if (!drag) return;
  const screen = pointer(event);
  const vp = state.viewport;
  drag.lastScreen = screen; // release-point cursor + rubber-band moving corner, every mode
  switch (drag.mode) {
    case "pan":
      // Re-solve the offset from the grabbed world point each move: keeps that point under
      // the cursor at the CURRENT scale, so a wheel-zoom mid-pan can't desync the content.
      state.viewport = panOffsetFor(drag.grabWorld, screen, vp);
      requestRender();
      break;
    case "group": {
      // Delta = current pointer image point - grabbed world anchor (zoom/pan stable). Kept
      // fractional for smooth sub-pixel motion; commitGroupDrag rounds ONCE on mouseup.
      const { dx, dy } = dragWorldDelta(drag.grabWorld, screen, vp);
      // T0244 §6: Ctrl/Cmd held mid-drag bypasses snap (Figma's own modifier) -- not read
      // anywhere else during a move (see design doc §2 modifier audit), so this is safe.
      const bypass = event.ctrlKey || event.metaKey;
      let sdx = dx;
      let sdy = dy;
      if (!bypass && drag.snapCandidates) {
        const tol = SNAP_SCREEN_PX / vp.scale; // screen px -> world tolerance (zoom-aware)
        const raw = { x: drag.snapBBox.x + dx, y: drag.snapBBox.y + dy, w: drag.snapBBox.w, h: drag.snapBBox.h };
        const snap = snapDelta(raw, drag.snapCandidates, tol);
        sdx = dx + snap.dx;
        sdy = dy + snap.dy;
        drag.activeGuides = snap.guides;
      } else {
        drag.activeGuides = [];
      }
      drag.group.x = drag.origGroup.x + sdx;
      drag.group.y = drag.origGroup.y + sdy;
      for (const item of drag.members) {
        item.element.x = item.origX + sdx;
        item.element.y = item.origY + sdy;
      }
      // The group drag moves its FULL subtree: nested frames translate too.
      for (const item of drag.subgroups || []) {
        item.group.x = item.origX + sdx;
        item.group.y = item.origY + sdy;
      }
      requestRender();
      break;
    }
    case "selection": {
      const { dx, dy } = dragWorldDelta(drag.grabWorld, screen, vp);
      // T0244 §6: Ctrl/Cmd held mid-drag bypasses snap (Figma's own modifier).
      const bypass = event.ctrlKey || event.metaKey;
      let sdx = dx;
      let sdy = dy;
      if (!bypass && drag.snapCandidates) {
        const tol = SNAP_SCREEN_PX / vp.scale; // screen px -> world tolerance (zoom-aware)
        const raw = { x: drag.snapBBox.x + dx, y: drag.snapBBox.y + dy, w: drag.snapBBox.w, h: drag.snapBBox.h };
        const snap = snapDelta(raw, drag.snapCandidates, tol);
        sdx = dx + snap.dx;
        sdy = dy + snap.dy;
        drag.activeGuides = snap.guides;
      } else {
        drag.activeGuides = [];
      }
      for (const item of drag.elItems) {
        item.element.x = item.origX + sdx;
        item.element.y = item.origY + sdy;
      }
      for (const g of drag.grpItems) {
        g.group.x = g.origX + sdx;
        g.group.y = g.origY + sdy;
        for (const m of g.members) {
          m.element.x = m.origX + sdx;
          m.element.y = m.origY + sdy;
        }
        for (const s of g.subgroups) {
          s.group.x = s.origX + sdx;
          s.group.y = s.origY + sdy;
        }
      }
      requestRender();
      break;
    }
    case "element": {
      const { dx, dy } = dragWorldDelta(drag.grabWorld, screen, vp);
      // T0244 §6: Ctrl/Cmd held mid-drag bypasses snap (Figma's own modifier).
      const bypass = event.ctrlKey || event.metaKey;
      let sdx = dx;
      let sdy = dy;
      if (!bypass && drag.snapCandidates) {
        const tol = SNAP_SCREEN_PX / vp.scale; // screen px -> world tolerance (zoom-aware)
        const raw = { x: drag.snapBBox.x + dx, y: drag.snapBBox.y + dy, w: drag.snapBBox.w, h: drag.snapBBox.h };
        const snap = snapDelta(raw, drag.snapCandidates, tol);
        sdx = dx + snap.dx;
        sdy = dy + snap.dy;
        drag.activeGuides = snap.guides;
      } else {
        drag.activeGuides = [];
      }
      for (const item of drag.items) {
        item.element.x = item.origX + sdx;
        item.element.y = item.origY + sdy;
      }
      requestRender();
      break;
    }
    case "marquee":
      applyMarquee();
      // Live selection shows on the CANVAS (selected boxes stroke) via requestRender only —
      // the layers/inspector panels are NOT rebuilt per frame (that full refresh() per
      // mousemove was the marquee's stepping/perf cost); the panels sync once on mouseup.
      requestRender();
      break;
    case "region-move":
      dragRegionMove(screen);
      requestRender();
      break;
    case "region-resize":
      dragRegionResize(screen);
      requestRender();
      break;
    case "region-create":
      requestRender();
      break;
    case "scale": {
      // World-anchored delta (see beginSelectionDrag) -- kept fractional for smooth live
      // preview; commitScaleDrag rounds ONCE on mouseup (T0236 fractional-during/round-at-
      // commit). NOT a T0244 snap-eligible drag -- no snapCandidates/activeGuides here.
      const { dx, dy } = dragWorldDelta(drag.grabWorld, screen, vp);
      const proportional = event.shiftKey ? !drag.proportionalDefault : drag.proportionalDefault;
      const fromCenter = event.altKey;
      if (drag.rotation) {
        // T0232 increment 3b: a SOLO rotated element resizes in its own local (rotated)
        // frame (resizeRotatedBox) -- rotation itself never changes during a scale gesture,
        // so drag.rotation stays fixed for the whole drag.
        const item = drag.items[0];
        const mapped = resizeRotatedBox(
          { ...item.box, rotation: drag.rotation },
          drag.handle,
          { dx, dy },
          { proportional, fromCenter, minSize: SCALE_MIN_SIZE },
        );
        item.ref.x = mapped.x;
        item.ref.y = mapped.y;
        if (item.isText) {
          item.ref.style = { ...item.ref.style, fontSize: scaledFontSize(item.origFontSize, mapped.sy) };
        } else {
          item.ref.w = Math.max(1, mapped.w);
          item.ref.h = Math.max(1, mapped.h);
        }
        requestRender();
        break;
      }
      const newAABB = resizeBox(drag.origAABB, drag.handle, { dx, dy }, { proportional, fromCenter, minSize: SCALE_MIN_SIZE });
      // T0271: Ctrl/Cmd held during a SCALE drag now means "frame-only" (the ORIGINAL
      // T0232 Q2 default -- the group's own box resizes, children pinned exactly as
      // grabbed), demoted to a modifier now that whole-subtree content-scale is the
      // default. Read live per-event, exactly like Shift/Alt, so toggling mid-drag flips
      // modes on the very next frame (applyGroupScalePreview always recomputes from the
      // frozen grab-time snapshot, never from a previous frame's own preview). Stashed on
      // `drag` too so commitScaleDrag (which only sees the FINAL released state, not the
      // mouseup event) knows which server route each group item should commit through.
      const groupFrameOnly = event.ctrlKey || event.metaKey;
      drag.groupFrameOnly = groupFrameOnly;
      for (const item of drag.items) {
        const mapped = mapItemBox(item.box, drag.origAABB, newAABB);
        if (item.kind === "group") {
          applyGroupScalePreview(item, mapped, groupFrameOnly);
          continue;
        }
        item.ref.x = mapped.x;
        item.ref.y = mapped.y;
        if (item.isText) {
          // Never stretch the text box (PIL re-measures from fontSize+content) -- scale the
          // font size by the same ratio the rest of the block is scaling by instead.
          item.ref.style = { ...item.ref.style, fontSize: scaledFontSize(item.origFontSize, mapped.sy) };
        } else {
          item.ref.w = Math.max(1, mapped.w);
          item.ref.h = Math.max(1, mapped.h);
        }
      }
      requestRender();
      break;
    }
    case "rotate": {
      // T0232 increment 3b: recompute the ABSOLUTE angle fresh every frame from the FIXED
      // grab point/pivot (rotationFromDrag) -- never an incremental accumulation, so the
      // result is exact regardless of frame count; a mid-drag zoom is naturally absorbed the
      // same way dragWorldDelta absorbs it elsewhere (screenToImagePoint through the LIVE
      // viewport). Shift snaps to 15-degree steps (Figma); NEVER smart-guide snapped (T0244
      // is translate-only -- this drag object carries no snapCandidates/activeGuides, so
      // drawSnapGuides's early-return keeps it silent here).
      const currentWorld = screenToImagePoint(screen, vp);
      const rotation = rotationFromDrag(drag.center, drag.startWorld, currentWorld, drag.baseRotation, { snap15: event.shiftKey });
      const element = elements().find((item) => item.id === drag.elementId);
      if (element) element.rotation = rotation;
      requestRender();
      break;
    }
    default:
      break;
  }
}

function commitElementDrag(finished) {
  const projectId = state.project.id;
  const moved = finished.items.filter(
    (it) => Math.round(it.element.x) !== Math.round(it.origX) || Math.round(it.element.y) !== Math.round(it.origY),
  );
  // NO geometric reparenting (lead 2026-07-02, live verify): a canvas drag NEVER
  // changes group membership — parked-off-frame elements stay members, and moving a
  // button over another frame must not capture it ("я бы хотел выносить и добавлять в
  // группу всегда явно"). Joining/leaving a group is explicit only: layers drag onto a
  // group header / to root, Ctrl+G, Ungroup, CLI group-assign.
  if (!moved.length) {
    refresh();
    return;
  }
  // Persist ALL positions as ONE batched elements-set, so a single Ctrl+Z restores
  // every element's pre-drag position in one step (one HTTP call, one journal entry).
  (async () => {
    try {
      const patches = moved.map((it) => ({
        elementId: it.element.id,
        x: Math.round(it.element.x),
        y: Math.round(it.element.y),
      }));
      applyMutation(await api("POST", `/projects/${projectId}/elements-set`, { patches }));
    } catch (error) {
      setStatus(error.message, true);
    }
  })();
}

function commitGroupDrag(finished) {
  const moved =
    Math.round(finished.group.x) !== Math.round(finished.origGroup.x) ||
    Math.round(finished.group.y) !== Math.round(finished.origGroup.y);
  if (!moved) {
    refresh();
    return;
  }
  (async () => {
    try {
      const result = await api("PATCH", `/projects/${state.project.id}/groups/${finished.group.id}`, {
        x: Math.round(finished.group.x),
        y: Math.round(finished.group.y),
      });
      applyMutation(result, "Moved group.");
    } catch (error) {
      setStatus(error.message, true);
    }
  })();
}

// Commit a mixed selection move (loose elements + one or more group frames) as ONE
// journaled moveNodes op: loose elements AND group frames go in a single {nodeId,x,y}
// batch, each group cascading its subtree on the server — one HTTP call, one journal
// entry, one undo (no more N+1). Reparent-on-drop is intentionally NOT applied to a mixed
// selection (membership stays explicit: layers drag, Ctrl+G, Ungroup, CLI).
function commitSelectionDrag(finished) {
  const changed = (a, bx, by) => Math.round(a.x) !== Math.round(bx) || Math.round(a.y) !== Math.round(by);
  const movedEls = finished.elItems.filter((it) => changed(it.element, it.origX, it.origY));
  const movedGroups = finished.grpItems.filter((g) => changed(g.group, g.origX, g.origY));
  if (!movedEls.length && !movedGroups.length) {
    refresh();
    return;
  }
  const moves = [
    ...movedEls.map((it) => ({ nodeId: it.element.id, x: Math.round(it.element.x), y: Math.round(it.element.y) })),
    ...movedGroups.map((g) => ({ nodeId: g.group.id, x: Math.round(g.group.x), y: Math.round(g.group.y) })),
  ];
  moveNodesTo(moves);
}

function commitRegionCreate(finished) {
  const element = finished.element;
  // Click-vs-drag is decided in SCREEN pixels: at low zoom a few screen pixels of
  // tap jitter map to tens of SOURCE pixels, so the source-space minimum below
  // never caught an accidental tap (lead 2026-07-03).
  const dxScreen = Math.abs(finished.lastScreen.x - finished.startScreen.x);
  const dyScreen = Math.abs(finished.lastScreen.y - finished.startScreen.y);
  if (dxScreen < 4 && dyScreen < 4) {
    refresh(); // a tap, not a drag: no region
    return;
  }
  // Anchor corner = the world point grabbed at mousedown (zoom-stable); moving corner is the
  // live pointer re-projected through the current viewport — both correct after a mid-drag zoom.
  const a = finished.startWorld;
  const b = screenToImagePoint(finished.lastScreen, state.viewport);
  const { sx, sy } = scaleFactors(element);
  const sw = element.source_w || element.w;
  const sh = element.source_h || element.h;
  const toSrcX = (wx) => clamp(Math.round((wx - element.x) / sx), 0, sw);
  const toSrcY = (wy) => clamp(Math.round((wy - element.y) / sy), 0, sh);
  const x0 = toSrcX(Math.min(a.x, b.x));
  const y0 = toSrcY(Math.min(a.y, b.y));
  const w = toSrcX(Math.max(a.x, b.x)) - x0;
  const h = toSrcY(Math.max(a.y, b.y)) - y0;
  if (w < 3 || h < 3) {
    refresh(); // too small: treat as a click, no region created
    return;
  }
  const id = newRegionId();
  state.selectedRegionIds = new Set([id]);
  state.expandedElements.add(element.id);
  setRegionsFor(
    element.id,
    [...(element.regions || []), { id, name: nextRegionName(element), rect: [x0, y0, w, h] }],
    "Added region.",
  );
}

// Hand-drawn regions get the same "<element name> N" names as detected ones
// (ops.mjs nameDetectedRegions); N continues past both the region count and the
// highest existing suffix, so deleting a region never reuses its number.
function nextRegionName(element) {
  const base = String(element.name || "").trim() || "Region";
  let max = 0;
  for (const region of element.regions || []) {
    const name = typeof region.name === "string" ? region.name : "";
    const match = name.startsWith(`${base} `) ? name.slice(base.length + 1).match(/^(\d+)$/) : null;
    if (match) max = Math.max(max, Number(match[1]));
  }
  return `${base} ${Math.max(max, (element.regions || []).length) + 1}`;
}

// ---- polygon draft (page-only; never journaled until it closes) --------------

// A stage point -> the edited element's SOURCE-pixel coordinate, clamped to bounds.
function screenToSource(element, screen) {
  const world = screenToImagePoint(screen, state.viewport);
  const { sx, sy } = scaleFactors(element);
  const sw = element.source_w || element.w;
  const sh = element.source_h || element.h;
  return {
    x: clamp(Math.round((world.x - element.x) / sx), 0, sw),
    y: clamp(Math.round((world.y - element.y) / sy), 0, sh),
  };
}

// Append a vertex to the in-progress polygon draft (pure UI state — no journal entry).
function addPolygonVertex(element, screen) {
  const p = screenToSource(element, screen);
  state.polygonDraft = [...state.polygonDraft, [p.x, p.y]];
  state.polygonHover = p;
  render();
}

// Pop the last placed vertex (Ctrl+Z / Backspace while a draft is open). Returns true
// if a vertex was removed, so the key handler knows it consumed the event. Never
// touches the journal.
export function popPolygonVertex() {
  if (!state.polygonDraft.length) return false;
  state.polygonDraft = state.polygonDraft.slice(0, -1);
  state.polygonHover = null;
  render();
  return true;
}

// Cancel the whole draft (first Esc). Returns true when there was one to cancel.
export function cancelPolygonDraft() {
  if (!state.polygonDraft.length) return false;
  state.polygonDraft = [];
  state.polygonHover = null;
  render();
  return true;
}

// Close the draft (double-click / Enter) into ONE journaled setRegions that appends the
// new polygon region ({id, rect: bbox, polygon}); the op re-derives rect from the ring.
// No-ops below 3 points.
export function finishPolygonDraft() {
  const element = regionEditElement();
  if (!element || state.polygonDraft.length < 3) return;
  const polygon = state.polygonDraft.map((p) => [p[0], p[1]]);
  const rect = polygonBBox(polygon);
  const id = newRegionId();
  state.polygonDraft = [];
  state.polygonHover = null;
  state.selectedRegionIds = new Set([id]);
  state.expandedElements.add(element.id);
  setRegionsFor(
    element.id,
    [...(element.regions || []), { id, name: nextRegionName(element), rect, polygon }],
    "Added polygon region.",
  );
}

// Switch the region-edit tool (Select / Draw Rect / Draw Polygon). Leaving polygon
// abandons any in-progress draft. Page-only state, so no journal entry.
export function setRegionTool(tool) {
  const next = tool === "rect" || tool === "polygon" ? tool : "select";
  // Leaving polygon abandons the in-progress draft (it lives only in polygon mode).
  if (state.regionTool === "polygon" && next !== "polygon") {
    state.polygonDraft = [];
    state.polygonHover = null;
  }
  state.regionTool = next;
  render();
}

function onMouseUp() {
  // Always drop the drag-geometry cache (mousedown captured it even for no-drag clicks like
  // text placement / polygon vertices), so idle hover reads a fresh rect once the gesture ends.
  clearDragGeometryCache();
  if (!drag) return;
  const finished = drag;
  drag = null;
  switch (finished.mode) {
    case "element":
      commitElementDrag(finished);
      break;
    case "group":
      commitGroupDrag(finished);
      break;
    case "selection":
      commitSelectionDrag(finished);
      break;
    case "marquee": {
      // A plain click (no real marquee) on empty canvas exits the entered scope to root
      // (Figma: click outside deselects and steps out). A real marquee keeps the scope.
      const movedPx = Math.hypot(finished.lastScreen.x - finished.startScreen.x, finished.lastScreen.y - finished.startScreen.y);
      if (movedPx < 3 && !finished.shift) state.enteredGroupId = null;
      refresh(); // selection already applied live
      break;
    }
    case "region-move":
    case "region-resize":
      if (finished.changed) setRegionsFor(finished.element.id, finished.element.regions);
      else refresh();
      break;
    case "region-create":
      commitRegionCreate(finished);
      break;
    case "scale":
      commitScaleDrag(finished);
      break;
    case "rotate":
      commitRotateDrag(finished);
      break;
    default:
      render();
      break;
  }
  updateCursorAt(finished.lastScreen || { x: finished.startX || 0, y: finished.startY || 0 });
}

// Idle hover cursor. Mode A: default arrow, move over elements. Mode B: resize over
// handles, move over regions, crosshair over the isolated image's empty area (draw),
// default elsewhere. Grab/grabbing appear ONLY while panning.
function updateCursorAt(screen) {
  if (drag || !canvas) return;
  if (state.tool === "pan" || state.spacePan) {
    setCursor("grab");
    return;
  }
  const world = screenToImagePoint(screen, state.viewport);
  const editEl = regionEditElement();
  if (editEl) {
    // Draw tools: crosshair over the image, default elsewhere.
    if (state.regionTool === "polygon" || state.regionTool === "rect") {
      setCursor(pointInElement(world, editEl) ? "crosshair" : "default");
      return;
    }
    // Select tool: resize over a handle, move over a region body, crosshair over empty.
    if (state.selectedRegionIds.size) {
      const grabbed = hitRegionHandle(screen, editEl, state.selectedRegionIds, state.viewport);
      if (grabbed) {
        setCursor(grabbed.handle.cursor);
        return;
      }
    }
    if (pointInElement(world, editEl)) {
      setCursor(hitRegion(world, editEl) ? "move" : "crosshair");
      return;
    }
    setCursor("default");
    return;
  }
  // T0232 increment 3b: the rotate handle takes cursor precedence over the scale handles
  // (same order as the mousedown grab check above).
  if (hitRotateHandle(screen)) {
    setCursor("grab");
    return;
  }
  // T0232 increment 2: a resize handle takes cursor precedence over move (same order as
  // the mousedown grab check above).
  const handle = hitScaleHandle(screen);
  if (handle) {
    setCursor(handle.cursor);
    return;
  }
  if (hitGroupLabel(screen) || hitElement(world)) {
    setCursor("move");
    return;
  }
  setCursor("default");
}

function onHover(event) {
  if (drag) return;
  const screen = pointer(event);
  const editEl = regionEditElement();
  // Live rubber segment while placing polygon vertices.
  if (editEl && state.regionTool === "polygon" && state.polygonDraft.length) {
    state.polygonHover = screenToSource(editEl, screen);
    render();
  }
  updateHoverGroup(screen, editEl);
  updateCursorAt(screen);
}

// Track the group a plain click would select at the current scope (hover affordance).
// Only repaints when the hovered group actually changes, so idle mouse-moves are cheap.
function updateHoverGroup(screen, editEl) {
  let next = null;
  if (!editEl && state.tool === "select" && !state.spacePan) {
    const world = screenToImagePoint(screen, state.viewport);
    const hit = hitElement(world);
    if (hit) {
      const res = resolveClickSelection(hit, state.enteredGroupId);
      if (res.kind === "group") next = res.id;
    }
  }
  if (next !== state.hoverGroupId) {
    state.hoverGroupId = next;
    requestRender();
  }
}

// Double-click an image WITH regions -> enter region-edit isolation (mode B).
// An image with NO regions stays in object mode (lead 2026-07-03: dblclick kept
// dropping him into region-edit where a stray tap created an accidental region);
// creating the FIRST region is explicit only — context menu "Edit regions" or
// inspector "+ Add region". In polygon mode a double-click closes the draft
// (handled in onMouseDown via event.detail), so it must not re-enter isolation here.
function onDblClick(event) {
  // T0232 increment 3b: double-click the rotate handle resets rotation to 0 in ONE commit
  // (R4/R5 -- the same reset the inspector's "Reset rotation" button drives elsewhere).
  const screen = pointer(event);
  const rotateHit = hitRotateHandle(screen);
  if (rotateHit) {
    if ((Number(rotateHit.rotation) || 0) !== 0) patchElementBox(rotateHit.id, { rotation: 0 });
    return;
  }
  if (state.regionEditId && state.regionTool === "polygon") return;
  const world = screenToImagePoint(screen, state.viewport);
  const hit = hitElement(world);
  if (!hit) return;
  const res = resolveClickSelection(hit, state.enteredGroupId);
  if (res.kind === "group") {
    // Drill ONE level: enter that group, then select the child under the cursor at the
    // new (deeper) scope — a nested subgroup, or the leaf element if it lives here.
    state.enteredGroupId = res.id;
    const inner = resolveClickSelection(hit, res.id);
    if (inner.kind === "group") selectGroupOnly(inner.id);
    else selectOnly(hit.id);
    refresh();
    return;
  }
  // A TEXT or NOTE leaf has no regions, so double-click = inline edit (T0219 text; T0268
  // note — the same textarea overlay, wrap=soft on the note's fixed box). An image leaf
  // enters region-edit instead.
  if (hit.type === "text" || hit.type === "note") {
    selectOnly(hit.id);
    openTextEditor(hit);
    refresh();
    return;
  }
  // The click already resolves to the leaf element in the entered scope. Only an
  // image that already HAS regions drills into region-edit; otherwise just select.
  // R7 (T0232 increment 3a): a rotated/flipped element's regions read UNtransformed
  // source pixels, so region-edit refuses to open on one (mirrors the inspector's
  // grayed-out Detect/Slice/Alpha controls + the ops-layer refusal).
  if ((hit.regions || []).length) {
    if (isNodeTransformed(hit)) {
      setStatus("Reset rotation/flip to edit regions — the source is untransformed.", true);
      selectOnly(hit.id);
    } else {
      enterRegionEdit(hit.id);
    }
  } else {
    selectOnly(hit.id);
  }
  refresh();
}

function onWheel(event) {
  event.preventDefault();
  const screen = pointer(event);
  const factor = event.deltaY < 0 ? 1.1 : 1 / 1.1;
  state.viewport = zoomViewportAt(state.viewport, factor, screen);
  state.viewport.scale = clamp(state.viewport.scale, 0.05, 12);
  // Object/marquee/region drags anchor in WORLD space, so a mid-drag zoom needs no rebase —
  // their next move re-projects through the new viewport automatically. Pan is the exception:
  // its offset is DERIVED from the grabbed world point, so rebase that anchor to the wheel's
  // pointer (post-zoom) to keep panning seamless instead of snapping on the next move.
  if (drag && drag.mode === "pan") drag.grabWorld = screenToImagePoint(screen, state.viewport);
  // T0253 F2: the viewport math above stays synchronous (zoom anchor correctness — the
  // NEXT wheel/move event must see the updated state.viewport), but the repaint itself
  // routes through the same rAF coalescing every drag path uses. A high-res wheel/trackpad
  // emits 30-100 events/s; without this, each one was a full synchronous repaint (tree walk
  // + every drawImage + resizeCanvas), the one hot path the drag-rAF pass had missed.
  requestRender();
}

function onContextMenu(event) {
  event.preventDefault();
  const screen = pointer(event);
  const world = screenToImagePoint(screen, state.viewport);
  const labelGroupId = hitGroupLabel(screen);
  if (labelGroupId) {
    state.selectedGroupId = labelGroupId;
    state.selectedIds = new Set();
    state.selectedRegionIds = new Set();
    refresh();
    openContextMenu(event.clientX, event.clientY, { kind: "group", groupId: labelGroupId });
    return;
  }
  // Mode B: right-click a region on the isolated element -> region menu.
  const editEl = regionEditElement();
  if (editEl && pointInElement(world, editEl)) {
    const region = hitRegion(world, editEl);
    if (region) {
      if (!state.selectedRegionIds.has(region.id)) state.selectedRegionIds = new Set([region.id]);
      refresh();
      openContextMenu(event.clientX, event.clientY, { kind: "region", elementId: editEl.id, regionId: region.id });
      return;
    }
  }
  const hit = hitElement(world);
  if (hit) {
    // Resolve like a left-click so the menu targets the same node the click would select
    // (the container group for a grouped element, else the element).
    const res = resolveClickSelection(hit, state.enteredGroupId);
    state.enteredGroupId = res.scope;
    if (res.kind === "group") {
      selectGroupOnly(res.id);
      refresh();
      openContextMenu(event.clientX, event.clientY, { kind: "group", groupId: res.id });
      return;
    }
    if (!state.selectedIds.has(hit.id)) selectOnly(hit.id);
    refresh();
    openContextMenu(event.clientX, event.clientY, { kind: "element", elementId: hit.id });
    return;
  }
  openContextMenu(event.clientX, event.clientY, { kind: "empty", world });
}

// ---- init --------------------------------------------------------------------

export function initWorkspace() {
  canvas = el("canvas");
  ctx = canvas.getContext("2d");
  hooks.renderCanvas = render;
  hooks.syncTopBar = syncTopBar;

  canvas.addEventListener("mousedown", onMouseDown);
  canvas.addEventListener("mousemove", onHover);
  canvas.addEventListener("dblclick", onDblClick);
  window.addEventListener("mousemove", onMouseMove);
  window.addEventListener("mouseup", onMouseUp);
  canvas.addEventListener("wheel", onWheel, { passive: false });
  canvas.addEventListener("contextmenu", onContextMenu);

  for (const button of document.querySelectorAll("#tool-rail .tool")) {
    button.addEventListener("click", () => setTool(button.dataset.tool));
  }
  // Region-edit tool row (shown only in isolation mode): Select / Draw Rect / Draw Polygon.
  for (const button of document.querySelectorAll("#region-tools .rtool")) {
    button.addEventListener("click", () => setRegionTool(button.dataset.regionTool));
  }
  el("undo").addEventListener("click", undo);
  el("redo").addEventListener("click", redo);
  el("zoom-fit").addEventListener("click", fit);
  el("zoom-100").addEventListener("click", () => zoomTo(1));
  el("add-image").addEventListener("click", () => el("file-input").click());
  el("file-input").addEventListener("change", onFilePick);

  const title = el("ws-title");
  title.addEventListener("dblclick", () => {
    inlineEdit(title, state.project.title, (next) => renameProject(next));
  });

  // Layers-panel collapse/expand is owned by layers_panel.js (persisted + rail).

  window.addEventListener("resize", () => {
    // A resize is the only mid-drag change to stage/canvas geometry; drop the cached rects
    // so the next render/pointer re-reads them instead of mapping through a stale rect.
    clearDragGeometryCache();
    if (state.project) render();
  });
}

async function onFilePick(event) {
  const files = event.target.files;
  event.target.value = "";
  if (!files || !files.length) return;
  const { addImageFiles } = await import("./actions.js");
  await addImageFiles(files);
  fit();
}

export function setTool(tool) {
  state.tool = tool === "pan" ? "pan" : tool === "text" ? "text" : tool === "note" ? "note" : "select";
  const stage = el("stage");
  if (stage) stage.classList.toggle("pan-tool", state.tool === "pan");
  if (canvas) {
    canvas.style.cursor =
      state.tool === "pan" ? "grab" : state.tool === "text" || state.tool === "note" ? "text" : "default";
  }
  syncTopBar();
}

// T tool: place a fresh text element at the click point, switch back to Select (Figma),
// and open its inline editor so the user types immediately over the default "Text".
async function placeTextAt(world) {
  const element = await addTextAt(world);
  setTool("select");
  if (element) {
    const live = elements().find((item) => item.id === element.id);
    if (live) openTextEditor(live);
  }
}

// N tool: place a fresh note card at the click point, switch back to Select, and open its
// inline editor so the user types immediately (T0268 — mirrors placeTextAt). Exported so
// the empty-canvas context menu's "New note" can create + edit in one gesture too.
export async function placeNoteAt(world) {
  const element = await addNoteAt(world);
  setTool("select");
  if (element) {
    const live = elements().find((item) => item.id === element.id);
    if (live) openTextEditor(live);
  }
  return element;
}
