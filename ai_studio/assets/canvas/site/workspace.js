// Workspace view: the pan/zoom canvas, its crisp DPR-aware rendering, tool rail,
// zoom controls, top bar sync, and all pointer interaction. Geometry is reused
// from the Asset Tools viewport module and the region helpers in regions.js. Every
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
//   * Dropping an element with its centre inside another screen frame reparents it
//     (assignToGroup); positions persist ONCE on mouseup (no mid-drag journal spam).
import {
  api,
  clearSelection,
  el,
  elements,
  enterRegionEdit,
  exitRegionEdit,
  groupById,
  groups,
  hiddenGroupIds,
  hooks,
  isElementHidden,
  isSelected,
  imageFor,
  memberElements,
  refresh,
  regionEditElement,
  reloadProject,
  selectedElements,
  selectOnly,
  setStatus,
  state,
  toggleSelect,
} from "./app.js";
import { renameProject, setRegionsFor, undo, redo } from "./actions.js";
import { inlineEdit } from "./inline.js";
import { openContextMenu } from "./context_menu.js";
import {
  drawRegionsOverlay,
  hitRegion,
  hitRegionHandle,
  newRegionId,
  regionRect,
  scaleFactors,
} from "./regions.js";
import {
  clamp,
  fitViewport,
  imageToScreenPoint,
  screenToImagePoint,
  zoomViewportAt,
} from "../../viewer/asset_tools_viewport.mjs";

let canvas = null;
let ctx = null;
let groupLabelRects = []; // {groupId, x, y, w, h} in screen (CSS) space, per render

// ---- crisp DPR-aware sizing --------------------------------------------------

function resizeCanvas() {
  const stage = el("stage");
  const rect = stage.getBoundingClientRect();
  const dpr = window.devicePixelRatio || 1;
  state.cssWidth = Math.max(1, Math.floor(rect.width));
  state.cssHeight = Math.max(1, Math.floor(rect.height));
  // Backing store is scaled by devicePixelRatio; CSS keeps the element at rect
  // size (width/height:100%). setTransform then lets us draw in CSS pixels while
  // the extra backing resolution keeps lines and sprite edges crisp.
  canvas.width = Math.max(1, Math.round(state.cssWidth * dpr));
  canvas.height = Math.max(1, Math.round(state.cssHeight * dpr));
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}

// ---- rendering ---------------------------------------------------------------

export function render() {
  if (!canvas || !state.project) return;
  resizeCanvas();
  const vp = state.viewport;
  ctx.clearRect(0, 0, state.cssWidth, state.cssHeight);
  // Crisp sprite pixels when zoomed in (>= 2x); smooth when zoomed out.
  ctx.imageSmoothingEnabled = vp.scale < 2;

  const hidden = hiddenGroupIds();
  const editEl = regionEditElement(); // mode B element, or null (mode A)
  for (const element of elements()) {
    if (isElementHidden(element, hidden)) continue;
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

  groupLabelRects = [];
  for (const group of groups()) {
    if (group.visible === false) continue;
    drawGroupFrame(group, vp);
  }
  drawGestureOverlay();
  updateBreadcrumb(editEl);
  updateZoomIndicator();
  updateEmptyHint();
}

function updateBreadcrumb(editEl) {
  const node = el("region-breadcrumb");
  if (!node) return;
  if (editEl) {
    const empty = !(editEl.regions || []).length;
    // Empty-state hint inside the mode so a fresh image tells you what to do.
    node.textContent = empty
      ? `Regions: ${editEl.name || editEl.id} — drag on the image to draw a region (Esc to exit)`
      : `Regions: ${editEl.name || editEl.id} — Esc to exit`;
    node.classList.remove("hidden");
  } else {
    node.classList.add("hidden");
  }
}

function drawGroupFrame(group, vp) {
  const origin = imageToScreenPoint({ x: group.x, y: group.y }, vp);
  const w = group.w * vp.scale;
  const h = group.h * vp.scale;
  const selected = state.selectedGroupId === group.id;
  ctx.lineWidth = selected ? 2 : 1;
  ctx.strokeStyle = selected ? "#d7a14a" : "#77a7ff";
  ctx.strokeRect(origin.x, origin.y, w, h);

  const label = group.name || "Screen";
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

// Live rubber-band overlays for the marquee and new-region gestures.
function drawGestureOverlay() {
  if (!drag) return;
  if (drag.mode !== "marquee" && drag.mode !== "region-create") return;
  const rect = normScreenRect(drag.startScreen, drag.lastScreen);
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
  const hidden = hiddenGroupIds();
  const boxes = [
    ...elements().filter((element) => !isElementHidden(element, hidden)),
    ...groups().filter((group) => group.visible !== false),
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
  const rect = canvas.getBoundingClientRect();
  return { x: event.clientX - rect.left, y: event.clientY - rect.top };
}

function setCursor(name) {
  if (canvas) canvas.style.cursor = name;
}

function hitElement(world) {
  const items = elements();
  const hidden = hiddenGroupIds();
  for (let i = items.length - 1; i >= 0; i -= 1) {
    const e = items[i];
    if (isElementHidden(e, hidden)) continue;
    if (world.x >= e.x && world.x <= e.x + e.w && world.y >= e.y && world.y <= e.y + e.h) return e;
  }
  return null;
}

function hitGroupLabel(screen) {
  for (let i = groupLabelRects.length - 1; i >= 0; i -= 1) {
    const r = groupLabelRects[i];
    if (screen.x >= r.x && screen.x <= r.x + r.w && screen.y >= r.y && screen.y <= r.y + r.h) return r.groupId;
  }
  return null;
}

// Topmost visible screen frame whose bounds contain the world point (for reparent).
function groupAtCenter(cx, cy) {
  const list = groups();
  for (let i = list.length - 1; i >= 0; i -= 1) {
    const g = list[i];
    if (g.visible === false) continue;
    if (cx >= g.x && cx <= g.x + g.w && cy >= g.y && cy <= g.y + g.h) return g.id;
  }
  return null;
}

// ---- region gesture starts ---------------------------------------------------

function beginRegionResize(element, grabbed, screen) {
  const { sx, sy } = scaleFactors(element);
  drag = {
    mode: "region-resize",
    element,
    region: grabbed.region,
    handle: grabbed.handle,
    orig: [...regionRect(grabbed.region)],
    startX: screen.x,
    startY: screen.y,
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
      .map((r) => ({ region: r, orig: [...regionRect(r)] }));
    drag = { mode: "region-move", element, items, startX: screen.x, startY: screen.y, sx, sy, changed: false };
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
  const screen = pointer(event);
  const world = screenToImagePoint(screen, state.viewport);
  const wantPan = event.button === 1 || state.tool === "pan" || state.spacePan;

  if (wantPan) {
    drag = { mode: "pan", startX: screen.x, startY: screen.y, origOffset: { ...state.viewport } };
    setCursor("grabbing");
    return;
  }

  // MODE B (region-edit isolation): only the isolated element's regions respond.
  const editEl = regionEditElement();
  if (editEl) {
    if (state.selectedRegionIds.size) {
      const grabbed = hitRegionHandle(screen, editEl, state.selectedRegionIds, state.viewport);
      if (grabbed) {
        beginRegionResize(editEl, grabbed, screen);
        return;
      }
    }
    if (pointInElement(world, editEl)) {
      const region = hitRegion(world, editEl);
      if (region) {
        beginRegionSelectMove(editEl, region, screen, event.shiftKey);
        return;
      }
      // Empty area of the isolated (locked) image -> rubber-band a NEW region.
      beginRegionCreate(editEl, screen, world);
      return;
    }
    // Clicked outside the isolated image -> exit isolation, then handle as mode A.
    exitRegionEdit();
    refresh();
  }

  const labelGroupId = hitGroupLabel(screen);
  if (labelGroupId) {
    state.selectedGroupId = labelGroupId;
    state.selectedIds = new Set();
    state.selectedRegionIds = new Set();
    state.regionEditId = null;
    const group = groupById(labelGroupId);
    const members = memberElements(labelGroupId).map((element) => ({ element, origX: element.x, origY: element.y }));
    drag = { mode: "group", startX: screen.x, startY: screen.y, group, origGroup: { x: group.x, y: group.y }, members };
    setCursor("move");
    refresh();
    return;
  }

  // MODE A (object mode): regions are passive; drag always moves the whole element.
  const hit = hitElement(world);
  if (hit) {
    state.selectedGroupId = null;
    if (event.shiftKey || event.ctrlKey || event.metaKey) toggleSelect(hit.id);
    else if (!state.selectedIds.has(hit.id)) selectOnly(hit.id);
    const items = selectedElements().map((element) => ({ element, origX: element.x, origY: element.y }));
    drag = { mode: "element", startX: screen.x, startY: screen.y, items };
    setCursor("move");
    refresh();
    return;
  }

  // Empty canvas -> marquee select (panning is Hand/Space/middle-mouse only).
  drag = {
    mode: "marquee",
    startScreen: screen,
    lastScreen: screen,
    base: event.shiftKey ? new Set(state.selectedIds) : new Set(),
  };
  if (!event.shiftKey) clearSelection();
  setCursor("crosshair");
  refresh();
}

function applyMarquee() {
  const a = screenToImagePoint(drag.startScreen, state.viewport);
  const b = screenToImagePoint(drag.lastScreen, state.viewport);
  const rx = Math.min(a.x, b.x);
  const ry = Math.min(a.y, b.y);
  const rw = Math.abs(b.x - a.x);
  const rh = Math.abs(b.y - a.y);
  const hidden = hiddenGroupIds();
  const inside = new Set(drag.base);
  for (const element of elements()) {
    if (isElementHidden(element, hidden)) continue;
    const outside = element.x + element.w < rx || element.x > rx + rw || element.y + element.h < ry || element.y > ry + rh;
    if (!outside) inside.add(element.id);
  }
  state.selectedGroupId = null;
  state.selectedRegionIds = new Set();
  state.selectedIds = inside;
}

function dragRegionMove(screen) {
  const el2 = drag.element;
  const sw = el2.source_w || el2.w;
  const sh = el2.source_h || el2.h;
  const dxSrc = Math.round((screen.x - drag.startX) / (state.viewport.scale * drag.sx));
  const dySrc = Math.round((screen.y - drag.startY) / (state.viewport.scale * drag.sy));
  for (const item of drag.items) {
    const [ox, oy, w, h] = item.orig;
    item.region.rect = [clamp(ox + dxSrc, 0, Math.max(0, sw - w)), clamp(oy + dySrc, 0, Math.max(0, sh - h)), w, h];
  }
  if (dxSrc !== 0 || dySrc !== 0) drag.changed = true;
}

function dragRegionResize(screen) {
  const el2 = drag.element;
  const sw = el2.source_w || el2.w;
  const sh = el2.source_h || el2.h;
  const MIN = 2;
  const dx = Math.round((screen.x - drag.startX) / (state.viewport.scale * drag.sx));
  const dy = Math.round((screen.y - drag.startY) / (state.viewport.scale * drag.sy));
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
  if (x !== ox || y !== oy || w !== ow || h !== oh) drag.changed = true;
}

function onMouseMove(event) {
  if (!drag) return;
  const screen = pointer(event);
  const vp = state.viewport;
  switch (drag.mode) {
    case "pan":
      state.viewport = {
        ...vp,
        offsetX: drag.origOffset.offsetX + (screen.x - drag.startX),
        offsetY: drag.origOffset.offsetY + (screen.y - drag.startY),
      };
      render();
      break;
    case "group": {
      const dx = (screen.x - drag.startX) / vp.scale;
      const dy = (screen.y - drag.startY) / vp.scale;
      drag.group.x = drag.origGroup.x + dx;
      drag.group.y = drag.origGroup.y + dy;
      for (const item of drag.members) {
        item.element.x = item.origX + dx;
        item.element.y = item.origY + dy;
      }
      render();
      break;
    }
    case "element": {
      const dx = (screen.x - drag.startX) / vp.scale;
      const dy = (screen.y - drag.startY) / vp.scale;
      for (const item of drag.items) {
        item.element.x = item.origX + dx;
        item.element.y = item.origY + dy;
      }
      render();
      break;
    }
    case "marquee":
      drag.lastScreen = screen;
      applyMarquee();
      refresh();
      break;
    case "region-move":
      dragRegionMove(screen);
      render();
      break;
    case "region-resize":
      dragRegionResize(screen);
      render();
      break;
    case "region-create":
      drag.lastScreen = screen;
      render();
      break;
    default:
      break;
  }
}

function commitElementDrag(finished) {
  const projectId = state.project.id;
  const moved = finished.items.filter(
    (it) => Math.round(it.element.x) !== Math.round(it.origX) || Math.round(it.element.y) !== Math.round(it.origY),
  );
  // Reparent: an element whose centre lands inside a frame it doesn't belong to
  // joins that screen; landing outside every frame while belonging to one clears it.
  // Group elements by target so it is ONE assign call per target group.
  const reassign = new Map();
  for (const it of finished.items) {
    const element = it.element;
    const target = groupAtCenter(element.x + element.w / 2, element.y + element.h / 2);
    const current = element.groupId || null;
    if (target !== current) {
      if (!reassign.has(target)) reassign.set(target, []);
      reassign.get(target).push(element.id);
    }
  }
  if (!moved.length && !reassign.size) {
    refresh();
    return;
  }
  (async () => {
    try {
      // Reparent FIRST, then persist positions LAST, so a single Ctrl+Z restores the
      // pre-drag position in ONE step (the newest journal entry is the position patch).
      for (const [groupId, ids] of reassign) {
        await api("POST", `/projects/${projectId}/assign-group`, { elementIds: ids, groupId });
      }
      for (const it of moved) {
        await api("PATCH", `/projects/${projectId}/elements/${it.element.id}`, {
          x: Math.round(it.element.x),
          y: Math.round(it.element.y),
        });
      }
      await reloadProject();
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
      await api("PATCH", `/projects/${state.project.id}/groups/${finished.group.id}`, {
        x: Math.round(finished.group.x),
        y: Math.round(finished.group.y),
      });
      await reloadProject("Moved screen.");
    } catch (error) {
      setStatus(error.message, true);
    }
  })();
}

function commitRegionCreate(finished) {
  const element = finished.element;
  const a = screenToImagePoint(finished.startScreen, state.viewport);
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
  setRegionsFor(element.id, [...(element.regions || []), { id, rect: [x0, y0, w, h] }], "Added region.");
}

function onMouseUp() {
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
    case "marquee":
      refresh(); // selection already applied live
      break;
    case "region-move":
    case "region-resize":
      if (finished.changed) setRegionsFor(finished.element.id, finished.element.regions);
      else refresh();
      break;
    case "region-create":
      commitRegionCreate(finished);
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
  if (hitGroupLabel(screen) || hitElement(world)) {
    setCursor("move");
    return;
  }
  setCursor("default");
}

function onHover(event) {
  if (drag) return;
  updateCursorAt(pointer(event));
}

// Double-click any image -> enter region-edit isolation (mode B). Works on a fresh
// image with no regions so the user can draw the FIRST region there.
function onDblClick(event) {
  const world = screenToImagePoint(pointer(event), state.viewport);
  const hit = hitElement(world);
  if (hit) {
    enterRegionEdit(hit.id);
    refresh();
  }
}

function onWheel(event) {
  event.preventDefault();
  const screen = pointer(event);
  const factor = event.deltaY < 0 ? 1.1 : 1 / 1.1;
  state.viewport = zoomViewportAt(state.viewport, factor, screen);
  state.viewport.scale = clamp(state.viewport.scale, 0.05, 12);
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

  el("layers-collapse").addEventListener("click", () => {
    el("layers-panel").classList.toggle("collapsed");
    render();
  });

  window.addEventListener("resize", () => {
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
  state.tool = tool === "pan" ? "pan" : "select";
  const stage = el("stage");
  if (stage) stage.classList.toggle("pan-tool", state.tool === "pan");
  if (canvas) canvas.style.cursor = state.tool === "pan" ? "grab" : "default";
  syncTopBar();
}
