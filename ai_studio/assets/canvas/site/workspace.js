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
import { addTextAt, moveNodesTo, patchTextElement, renameProject, setRegionsFor, undo, redo } from "./actions.js";
import { canvasFontString } from "../fonts.mjs";
import { areFontsReady, measureTextBox, measureTextLines } from "./fonts.js";
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
  resizeBox,
  scaledFontSize,
  screenToImagePoint,
  SCALE_HANDLES,
  zoomViewportAt,
} from "./viewport.mjs";
import { ancestorsOf, childrenOf, descendantsOf, isNodeHidden, nodeScope, orderedChildren, unionBBox } from "../tree.mjs";
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
      if (clip) {
        ctx.save();
        const origin = imageToScreenPoint({ x: group.x, y: group.y }, vp);
        ctx.beginPath();
        ctx.rect(origin.x, origin.y, group.w * vp.scale, group.h * vp.scale);
        ctx.clip();
      }
      fillGroupBackground(group, vp);
      paintScope(child.id, vp, editEl);
      if (clip) ctx.restore();
    } else {
      paintElement(child.ref, vp, editEl);
    }
  }
}

function paintElement(element, vp, editEl) {
  if (element.type === "text") {
    paintTextElement(element, vp, editEl);
    return;
  }
  const isEdit = editEl && element.id === editEl.id;
  // Mode B dims every other element to focus the isolated one.
  ctx.globalAlpha = editEl && !isEdit ? 0.3 : 1;
  const img = imageFor(element);
  const origin = imageToScreenPoint({ x: element.x, y: element.y }, vp);
  const w = element.w * vp.scale;
  const h = element.h * vp.scale;
  if (img.complete && img.naturalWidth) {
    ctx.drawImage(img, origin.x, origin.y, w, h);
  } else {
    ctx.strokeStyle = "#596774";
    ctx.strokeRect(origin.x, origin.y, w, h);
  }
  ctx.globalAlpha = 1;
  if (isSelected(element)) {
    ctx.strokeStyle = isEdit ? "#3fc7ba" : "#77a7ff";
    ctx.lineWidth = 2;
    ctx.strokeRect(origin.x, origin.y, w, h);
    // Passive numbered hint in mode A; strong strokes + handles in mode B.
    drawRegionsOverlay(ctx, element, vp, {
      selectedRegionIds: state.selectedRegionIds,
      interactive: Boolean(isEdit),
    });
  }
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
  ctx.globalAlpha = editEl && !isEdit ? 0.3 : 1;
  const origin = imageToScreenPoint({ x: element.x, y: element.y }, vp);
  const style = element.style || {};
  if (areFontsReady() && state.editingTextId !== element.id) {
    drawTextGlyphs(element, style, origin, vp);
  }
  ctx.globalAlpha = 1;
  if (isSelected(element)) {
    ctx.strokeStyle = "#77a7ff";
    ctx.lineWidth = 2;
    ctx.strokeRect(origin.x, origin.y, element.w * vp.scale, element.h * vp.scale);
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
  const ta = document.createElement("textarea");
  ta.className = "text-edit-overlay";
  ta.value = element.content || "";
  ta.spellcheck = false;
  ta.wrap = "off";
  el("stage").appendChild(ta);
  textEditor = { el: ta, elementId: element.id };
  positionTextEditor();
  ta.focus();
  ta.select();
  ta.addEventListener("keydown", onTextEditorKey);
  ta.addEventListener("blur", () => commitTextEditor());
  ta.addEventListener("input", positionTextEditor);
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
  const fontSize = Number(style.fontSize) || 24;
  const ta = textEditor.el;
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
  const { el: ta, elementId } = textEditor;
  const content = ta.value;
  const element = elements().find((item) => item.id === elementId);
  textEditor = null;
  state.editingTextId = null;
  ta.remove();
  if (element && content !== element.content) {
    await patchTextElement(elementId, { content });
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

function drawGroupFrame(group, vp) {
  const origin = imageToScreenPoint({ x: group.x, y: group.y }, vp);
  const w = group.w * vp.scale;
  const h = group.h * vp.scale;
  const selected = state.selectedGroupId === group.id || state.selectedGroupIds.has(group.id);
  ctx.lineWidth = selected ? 2 : 1;
  ctx.strokeStyle = selected ? "#d7a14a" : "#77a7ff";
  ctx.strokeRect(origin.x, origin.y, w, h);

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

// ---- scale gizmo (T0232 increment 2 -- skeleton: move + 8 resize handles, NO rotation,
// NO flip, NO rotate handle; those land in increment 3). Pure box math lives in
// viewport.mjs (resizeBox/mapItemBox/scaledFontSize, SCALE_HANDLES) so it is DOM-free and
// unit-testable; this section only wires screen-space drawing/hit-testing and the drag
// lifecycle, mirroring regions.js's REGION_HANDLES/hitRegionHandle pattern one level up
// (the SELECTION's AABB instead of one region's box). A resize gesture is NOT a T0244
// snap-eligible drag (site/snap.mjs) -- these drag objects never carry
// snapCandidates/snapBBox/activeGuides, so drawSnapGuides's early-return keeps it silent.
const SCALE_HANDLE_HALF = 5; // half a handle square, CSS px
const SCALE_HANDLE_TOL = 8; // hit tolerance around a handle centre, CSS px
const SCALE_MIN_SIZE = 4; // floor for a resized element/group w or h

// The nodes a scale gesture would resize: every selected element (image or text) plus
// every selected group's OWN frame -- never a group's descendants (T0232 Q2 default:
// "group-in-selection = frame-only resize", no subtree scale in v1). `box` is the node's
// CURRENT frame; called fresh each frame for drawing/hit-testing, and ONCE at grab time to
// seed a drag (drag.items then holds that frozen snapshot for the rest of the gesture).
function collectResizeItems() {
  const items = [];
  for (const element of selectedElements()) {
    const isText = element.type === "text";
    items.push({
      kind: "element",
      id: element.id,
      ref: element,
      box: { x: element.x, y: element.y, w: element.w, h: element.h },
      isText,
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

function scaleHandlePoints(box) {
  return SCALE_HANDLES.map((handle) => ({ ...handle, x: box.x + box.w * handle.fx, y: box.y + box.h * handle.fy }));
}

// The current selection's screen-space AABB handle points, or null when there is nothing to
// resize. Shared by the draw pass and the hit-test so both agree on the exact same box.
function scaleHandleScreenBox(items, vp) {
  const box = unionBBox(items.map((item) => item.box));
  const origin = imageToScreenPoint({ x: box.x, y: box.y }, vp);
  return { x: origin.x, y: origin.y, w: box.w * vp.scale, h: box.h * vp.scale };
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
  const screenBox = scaleHandleScreenBox(items, state.viewport);
  for (const point of scaleHandlePoints(screenBox)) {
    if (Math.abs(screen.x - point.x) <= SCALE_HANDLE_TOL && Math.abs(screen.y - point.y) <= SCALE_HANDLE_TOL) return point;
  }
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
  const screenBox = scaleHandleScreenBox(items, vp);
  ctx.save();
  ctx.lineWidth = 1;
  for (const point of scaleHandlePoints(screenBox)) {
    ctx.fillStyle = "#77a7ff";
    ctx.fillRect(point.x - SCALE_HANDLE_HALF, point.y - SCALE_HANDLE_HALF, SCALE_HANDLE_HALF * 2, SCALE_HANDLE_HALF * 2);
    ctx.strokeStyle = "#1a1f2b";
    ctx.strokeRect(point.x - SCALE_HANDLE_HALF, point.y - SCALE_HANDLE_HALF, SCALE_HANDLE_HALF * 2, SCALE_HANDLE_HALF * 2);
  }
  ctx.restore();
}

// Begin a "scale" drag: snapshot every resize target's box ONCE at grab (drag.items), plus
// the selection's original AABB (drag.origAABB) the live preview and commit both key off of.
// `proportionalDefault` is true only when EVERY target is a plain image element (single or a
// pure multi-image block) -- "sprites keep proportions by default, Shift = free distort"
// (the design doc's default); a group or a text element in the mix defaults to free resize
// (Shift then LOCKS it), since neither has an intrinsic "aspect" the way a bitmap does.
function beginScaleDrag(handle, screen, world) {
  const items = collectResizeItems();
  if (!items.length) return;
  const origAABB = unionBBox(items.map((item) => item.box));
  const proportionalDefault = items.every((item) => item.kind === "element" && !item.isText);
  drag = { mode: "scale", startX: screen.x, startY: screen.y, grabWorld: world, handle, items, origAABB, proportionalDefault };
}

// Commit a finished scale drag. Element geometry (images + text) always batches through ONE
// elements-set call -- one journal entry / one undo, whether 1 or N elements resized (mirrors
// commitElementDrag, which does the same for a move). A selected GROUP's frame has no
// batched-resize op (T0232 §2b: patchGroup is per-group, frame-only, no `patchGroups`-style
// batch reused here) -- a gesture that resizes 2+ groups, or a MIX of elements and groups,
// therefore lands as more than one journal entry (one Ctrl+Z per group patch); a pure
// element-only block or a single solo group still commits as exactly one undo, matching the
// doc's two documented cases. A future combined resize op (moveNodes's resize sibling) would
// close that gap; out of scope here (no ops.mjs change in this increment).
function commitScaleDrag(finished) {
  const projectId = state.project.id;
  const elementPatches = [];
  const groupPatches = [];
  for (const item of finished.items) {
    const orig = item.box;
    if (item.kind === "group") {
      // No x/y here on purpose (see the mousemove "scale" case's group branch): only w/h
      // ever moved, so patchGroup's move-cascade (triggered by ANY x/y change) never fires.
      const w = Math.round(item.ref.w);
      const h = Math.round(item.ref.h);
      if (w !== Math.round(orig.w) || h !== Math.round(orig.h)) {
        groupPatches.push({ groupId: item.id, w, h });
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
  if (!elementPatches.length && !groupPatches.length) {
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
      applyMutation(result);
    } catch (error) {
      setStatus(error.message, true);
    }
  })();
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
  const inBox = (e) => world.x >= e.x && world.x <= e.x + e.w && world.y >= e.y && world.y <= e.y + e.h;
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
  return walk(null);
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
      const newAABB = resizeBox(drag.origAABB, drag.handle, { dx, dy }, { proportional, fromCenter, minSize: SCALE_MIN_SIZE });
      for (const item of drag.items) {
        const mapped = mapItemBox(item.box, drag.origAABB, newAABB);
        if (item.kind === "group") {
          // patchGroup translates its FULL descendant closure whenever x/y CHANGE (that is
          // exactly right for a group MOVE drag, commitGroupDrag's own case) -- but a resize
          // must NOT trigger that cascade, or "frame-only resize; children unchanged" (T0232
          // §2b) would silently drag every member along with a moved corner/edge handle. So a
          // group's origin stays PINNED at its grab-time x/y here; only w/h (still scaled by
          // the same block factors as every other item) tracks the drag. A known v1 skeleton
          // limitation: unlike an element, a solo/mixed group therefore only grows from its
          // fixed top-left regardless of which handle is dragged (flagged in the packet report).
          item.ref.w = Math.max(SCALE_MIN_SIZE, mapped.w);
          item.ref.h = Math.max(SCALE_MIN_SIZE, mapped.h);
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
  if (state.regionEditId && state.regionTool === "polygon") return;
  const world = screenToImagePoint(pointer(event), state.viewport);
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
  // A TEXT leaf has no regions, so double-click = inline text edit (T0219: text leaf
  // dblclick edits text INSTEAD of region-edit). An image leaf enters region-edit.
  if (hit.type === "text") {
    selectOnly(hit.id);
    openTextEditor(hit);
    refresh();
    return;
  }
  // The click already resolves to the leaf element in the entered scope. Only an
  // image that already HAS regions drills into region-edit; otherwise just select.
  if ((hit.regions || []).length) {
    enterRegionEdit(hit.id);
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
  render();
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
  state.tool = tool === "pan" ? "pan" : tool === "text" ? "text" : "select";
  const stage = el("stage");
  if (stage) stage.classList.toggle("pan-tool", state.tool === "pan");
  if (canvas) canvas.style.cursor = state.tool === "pan" ? "grab" : state.tool === "text" ? "text" : "default";
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
