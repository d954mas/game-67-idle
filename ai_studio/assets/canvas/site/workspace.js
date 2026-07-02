// Workspace view: the pan/zoom canvas, its crisp DPR-aware rendering, tool rail,
// zoom controls, top bar sync, and all pointer interaction (select, drag-move,
// pan). Geometry is reused from the Asset Tools viewport module. Every persisted
// change goes through the shared actions/API; this module only renders and turns
// input into those calls.
import {
  api,
  clearSelection,
  el,
  elements,
  groupById,
  groups,
  hiddenGroupIds,
  hooks,
  isElementHidden,
  isSelected,
  imageFor,
  memberElements,
  refresh,
  selectedElements,
  selectOnly,
  setStatus,
  state,
  toggleSelect,
} from "./app.js";
import { renameProject, undo, redo } from "./actions.js";
import { inlineEdit } from "./inline.js";
import { openContextMenu } from "./context_menu.js";
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
  for (const element of elements()) {
    if (isElementHidden(element, hidden)) continue;
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
    if (isSelected(element)) {
      ctx.strokeStyle = "#77a7ff";
      ctx.lineWidth = 2;
      ctx.strokeRect(origin.x, origin.y, w, h);
      drawRegions(element, vp);
    }
  }

  groupLabelRects = [];
  for (const group of groups()) {
    if (group.visible === false) continue;
    drawGroupFrame(group, vp);
  }
  updateZoomIndicator();
  updateEmptyHint();
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

function drawRegions(element, vp) {
  const regions = element.regions || [];
  const sx = element.w / (element.source_w || element.w);
  const sy = element.h / (element.source_h || element.h);
  ctx.lineWidth = 1;
  ctx.strokeStyle = "#3fc7ba";
  ctx.fillStyle = "rgba(63, 199, 186, 0.12)";
  for (const region of regions) {
    const rect = region.rect || region.content_bbox;
    if (!rect) continue;
    const p = imageToScreenPoint({ x: element.x + rect[0] * sx, y: element.y + rect[1] * sy }, vp);
    ctx.fillRect(p.x, p.y, rect[2] * sx * vp.scale, rect[3] * sy * vp.scale);
    ctx.strokeRect(p.x, p.y, rect[2] * sx * vp.scale, rect[3] * sy * vp.scale);
  }
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
let moveSaveTimer = null;

function pointer(event) {
  const rect = canvas.getBoundingClientRect();
  return { x: event.clientX - rect.left, y: event.clientY - rect.top };
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

function scheduleMoveSave() {
  clearTimeout(moveSaveTimer);
  moveSaveTimer = setTimeout(saveDraggedPositions, 300);
}

async function saveDraggedPositions() {
  if (!drag || !drag.items) return;
  for (const item of drag.items) {
    try {
      await api("PATCH", `/projects/${state.project.id}/elements/${item.element.id}`, {
        x: Math.round(item.element.x),
        y: Math.round(item.element.y),
      });
    } catch (error) {
      setStatus(error.message, true);
    }
  }
}

function onMouseDown(event) {
  if (event.button !== 0 && event.button !== 1) return;
  const screen = pointer(event);
  const world = screenToImagePoint(screen, state.viewport);
  const wantPan = event.button === 1 || state.tool === "pan" || state.spacePan;

  if (wantPan) {
    drag = { mode: "pan", startX: screen.x, startY: screen.y, origOffset: { ...state.viewport } };
    canvas.classList.add("dragging");
    return;
  }

  const labelGroupId = hitGroupLabel(screen);
  if (labelGroupId) {
    state.selectedGroupId = labelGroupId;
    state.selectedIds = new Set();
    const group = groupById(labelGroupId);
    const members = memberElements(labelGroupId).map((element) => ({ element, origX: element.x, origY: element.y }));
    drag = { mode: "group", startX: screen.x, startY: screen.y, group, origGroup: { x: group.x, y: group.y }, members };
    canvas.classList.add("dragging");
    refresh();
    return;
  }

  const hit = hitElement(world);
  if (hit) {
    state.selectedGroupId = null;
    if (event.shiftKey || event.ctrlKey || event.metaKey) toggleSelect(hit.id);
    else if (!state.selectedIds.has(hit.id)) selectOnly(hit.id);
    const items = selectedElements().map((element) => ({ element, origX: element.x, origY: element.y }));
    drag = { mode: "element", startX: screen.x, startY: screen.y, items };
    canvas.classList.add("dragging");
  } else {
    if (!event.shiftKey) clearSelection();
    drag = { mode: "pan", startX: screen.x, startY: screen.y, origOffset: { ...state.viewport } };
    canvas.classList.add("dragging");
  }
  refresh();
}

function onMouseMove(event) {
  if (!drag) return;
  const screen = pointer(event);
  if (drag.mode === "pan") {
    state.viewport = {
      ...state.viewport,
      offsetX: drag.origOffset.offsetX + (screen.x - drag.startX),
      offsetY: drag.origOffset.offsetY + (screen.y - drag.startY),
    };
    render();
  } else if (drag.mode === "group") {
    const dx = (screen.x - drag.startX) / state.viewport.scale;
    const dy = (screen.y - drag.startY) / state.viewport.scale;
    drag.group.x = drag.origGroup.x + dx;
    drag.group.y = drag.origGroup.y + dy;
    for (const item of drag.members) {
      item.element.x = item.origX + dx;
      item.element.y = item.origY + dy;
    }
    render();
  } else {
    const dx = (screen.x - drag.startX) / state.viewport.scale;
    const dy = (screen.y - drag.startY) / state.viewport.scale;
    for (const item of drag.items) {
      item.element.x = item.origX + dx;
      item.element.y = item.origY + dy;
    }
    render();
    scheduleMoveSave();
  }
}

function onMouseUp() {
  if (!drag) return;
  const finished = drag;
  const mode = drag.mode;
  drag = null;
  canvas.classList.remove("dragging");
  if (mode === "element") {
    clearTimeout(moveSaveTimer);
    (async () => {
      for (const item of finished.items) {
        try {
          await api("PATCH", `/projects/${state.project.id}/elements/${item.element.id}`, {
            x: Math.round(item.element.x),
            y: Math.round(item.element.y),
          });
        } catch (error) {
          setStatus(error.message, true);
        }
      }
      const { refreshHistory, reloadProject } = await import("./app.js");
      await refreshHistory();
      await reloadProject();
    })();
  } else if (mode === "group") {
    const moved = Math.round(finished.group.x) !== Math.round(finished.origGroup.x)
      || Math.round(finished.group.y) !== Math.round(finished.origGroup.y);
    if (!moved) return;
    (async () => {
      try {
        await api("PATCH", `/projects/${state.project.id}/groups/${finished.group.id}`, {
          x: Math.round(finished.group.x),
          y: Math.round(finished.group.y),
        });
        const { reloadProject } = await import("./app.js");
        await reloadProject("Moved screen.");
      } catch (error) {
        setStatus(error.message, true);
      }
    })();
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
    refresh();
    openContextMenu(event.clientX, event.clientY, { kind: "group", groupId: labelGroupId });
    return;
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
  syncTopBar();
}
