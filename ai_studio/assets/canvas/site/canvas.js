// Thin canvas page. All logic lives in the shared ops layer behind the HTTP API;
// this file only renders project.json and turns input into single API calls.
// Pan/zoom/fit geometry is reused from the Asset Tools viewport module.
import {
  clamp,
  fitViewport,
  imageToScreenPoint,
  screenToImagePoint,
  zoomViewportAt,
} from "../../viewer/asset_tools_viewport.mjs";

const el = (id) => document.getElementById(id);
const canvas = el("canvas");
const ctx = canvas.getContext("2d");

const state = {
  projects: [],
  project: null,
  selectedIds: new Set(),
  history: { canUndo: false, canRedo: false },
  viewport: { scale: 1, offsetX: 0, offsetY: 0 },
};
const imageCache = new Map(); // element.src -> HTMLImageElement

function setStatus(message, isError = false) {
  const node = el("status");
  node.textContent = message;
  node.classList.toggle("error", isError);
}

async function api(method, path, body) {
  const res = await fetch(`/api/canvas${path}`, {
    method,
    headers: body ? { "content-type": "application/json" } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || res.statusText);
  return data;
}

function fileUrl(element) {
  return `/api/canvas/projects/${state.project.id}/${element.src}`;
}

function imageFor(element) {
  if (imageCache.has(element.src)) return imageCache.get(element.src);
  const img = new Image();
  img.onload = render;
  img.src = fileUrl(element);
  imageCache.set(element.src, img);
  return img;
}

function elements() {
  return (state.project && state.project.elements) || [];
}

function selectedElements() {
  return elements().filter((e) => state.selectedIds.has(e.id));
}

function isSelected(element) {
  return state.selectedIds.has(element.id);
}

// ---- rendering ---------------------------------------------------------------

function resizeCanvas() {
  const rect = canvas.getBoundingClientRect();
  canvas.width = Math.max(1, Math.floor(rect.width));
  canvas.height = Math.max(1, Math.floor(rect.height));
}

function render() {
  resizeCanvas();
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  const vp = state.viewport;
  for (const element of elements()) {
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
}

function drawRegions(element, vp) {
  const regions = element.regions || [];
  // Region rects are in intrinsic source pixels; scale by the element box so the
  // overlay stays aligned even after the element is resized (source_w != w).
  const sx = element.w / (element.source_w || element.w);
  const sy = element.h / (element.source_h || element.h);
  ctx.lineWidth = 1;
  ctx.strokeStyle = "#3fc7ba";
  ctx.fillStyle = "rgba(63, 199, 186, 0.12)";
  for (const region of regions) {
    const rect = region.rect || region.content_bbox;
    if (!rect) continue;
    const p = imageToScreenPoint({ x: element.x + rect[0] * sx, y: element.y + rect[1] * sy }, vp);
    const w = rect[2] * sx * vp.scale;
    const h = rect[3] * sy * vp.scale;
    ctx.fillRect(p.x, p.y, w, h);
    ctx.strokeRect(p.x, p.y, w, h);
  }
}

function elementBounds() {
  const items = elements();
  if (!items.length) return { x: 0, y: 0, width: 1024, height: 768 };
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const e of items) {
    minX = Math.min(minX, e.x);
    minY = Math.min(minY, e.y);
    maxX = Math.max(maxX, e.x + e.w);
    maxY = Math.max(maxY, e.y + e.h);
  }
  return { x: minX, y: minY, width: Math.max(1, maxX - minX), height: Math.max(1, maxY - minY) };
}

function fit() {
  resizeCanvas();
  const bounds = elementBounds();
  const base = fitViewport({
    imageWidth: bounds.width,
    imageHeight: bounds.height,
    frameWidth: canvas.width,
    frameHeight: canvas.height,
    padding: 40,
  });
  state.viewport = {
    scale: base.scale,
    offsetX: base.offsetX - bounds.x * base.scale,
    offsetY: base.offsetY - bounds.y * base.scale,
  };
  render();
}

// ---- project list + open -----------------------------------------------------

function renderProjectList() {
  const list = el("project-list");
  list.innerHTML = "";
  for (const project of state.projects) {
    const li = document.createElement("li");
    li.textContent = project.title;
    if (state.project && project.id === state.project.id) li.classList.add("active");
    const meta = document.createElement("span");
    meta.className = "meta";
    meta.textContent = `${(project.elements || []).length} image(s)`;
    li.appendChild(meta);
    li.addEventListener("click", () => openProject(project.id));
    list.appendChild(li);
  }
}

function syncToolbar() {
  const hasProject = Boolean(state.project);
  const selected = selectedElements();
  const single = selected.length === 1 ? selected[0] : null;
  el("project-title").textContent = state.project ? state.project.title : "No project";
  el("add-image").disabled = !hasProject;
  el("fit").disabled = !hasProject;
  el("detect-regions").disabled = !single;
  el("slice").disabled = !(single && Array.isArray(single.regions) && single.regions.length > 0);
  el("delete-element").disabled = selected.length === 0;
  el("export-selected").disabled = selected.length === 0;
  el("undo").disabled = !state.history.canUndo;
  el("redo").disabled = !state.history.canRedo;
}

async function loadProjects() {
  state.projects = (await api("GET", "/projects")).projects;
  renderProjectList();
}

async function refreshHistory() {
  if (!state.project) {
    state.history = { canUndo: false, canRedo: false };
    return;
  }
  try {
    const history = await api("GET", `/projects/${state.project.id}/history`);
    state.history = { canUndo: history.canUndo, canRedo: history.canRedo };
  } catch {
    state.history = { canUndo: false, canRedo: false };
  }
}

async function openProject(id) {
  state.project = (await api("GET", `/projects/${id}`)).project;
  state.selectedIds = new Set();
  imageCache.clear();
  await refreshHistory();
  renderProjectList();
  syncToolbar();
  fit();
  setStatus(`Opened ${state.project.title}.`);
}

// Re-fetch the project from disk (source of truth after any mutating op) and
// re-render. Selection is pruned to still-existing elements.
async function reloadProject(message) {
  if (!state.project) return;
  state.project = (await api("GET", `/projects/${state.project.id}`)).project;
  const alive = new Set(elements().map((e) => e.id));
  state.selectedIds = new Set([...state.selectedIds].filter((id) => alive.has(id)));
  await refreshHistory();
  renderProjectList();
  syncToolbar();
  render();
  if (message) setStatus(message);
}

// ---- pointer interaction -----------------------------------------------------

let drag = null; // { mode, startX, startY, origOffset?, items?: [{element, origX, origY}] }
let moveSaveTimer = null;

function pointer(event) {
  const rect = canvas.getBoundingClientRect();
  return { x: event.clientX - rect.left, y: event.clientY - rect.top };
}

function hitElement(worldPoint) {
  const items = elements();
  for (let i = items.length - 1; i >= 0; i -= 1) {
    const e = items[i];
    if (
      worldPoint.x >= e.x && worldPoint.x <= e.x + e.w &&
      worldPoint.y >= e.y && worldPoint.y <= e.y + e.h
    ) {
      return e;
    }
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

canvas.addEventListener("mousedown", (event) => {
  const screen = pointer(event);
  const world = screenToImagePoint(screen, state.viewport);
  const hit = hitElement(world);
  if (hit) {
    if (event.shiftKey) {
      if (state.selectedIds.has(hit.id)) state.selectedIds.delete(hit.id);
      else state.selectedIds.add(hit.id);
    } else if (!state.selectedIds.has(hit.id)) {
      state.selectedIds = new Set([hit.id]);
    }
    // Drag all currently-selected elements together.
    const items = selectedElements().map((element) => ({ element, origX: element.x, origY: element.y }));
    drag = { mode: "element", startX: screen.x, startY: screen.y, items };
    canvas.classList.add("dragging");
  } else {
    if (!event.shiftKey) state.selectedIds = new Set();
    drag = { mode: "pan", startX: screen.x, startY: screen.y, origOffset: { ...state.viewport } };
    canvas.classList.add("dragging");
  }
  syncToolbar();
  render();
});

window.addEventListener("mousemove", (event) => {
  if (!drag) return;
  const screen = pointer(event);
  if (drag.mode === "pan") {
    state.viewport = {
      ...state.viewport,
      offsetX: drag.origOffset.offsetX + (screen.x - drag.startX),
      offsetY: drag.origOffset.offsetY + (screen.y - drag.startY),
    };
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
});

window.addEventListener("mouseup", () => {
  if (!drag) return;
  const wasElement = drag.mode === "element";
  const finished = drag;
  drag = null;
  canvas.classList.remove("dragging");
  if (wasElement) {
    clearTimeout(moveSaveTimer);
    // Save on the finished drag, then refresh history so undo/redo enable.
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
      await refreshHistory();
      syncToolbar();
    })();
  }
});

canvas.addEventListener(
  "wheel",
  (event) => {
    event.preventDefault();
    const screen = pointer(event);
    const factor = event.deltaY < 0 ? 1.1 : 1 / 1.1;
    state.viewport = zoomViewportAt(state.viewport, factor, screen);
    state.viewport.scale = clamp(state.viewport.scale, 0.05, 12);
    render();
  },
  { passive: false },
);

// ---- toolbar actions ---------------------------------------------------------

el("new-project").addEventListener("click", async () => {
  const title = prompt("Project title", "New canvas");
  if (title === null) return;
  try {
    const { project } = await api("POST", "/projects", { title });
    await loadProjects();
    await openProject(project.id);
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("add-image").addEventListener("click", () => el("file-input").click());

el("file-input").addEventListener("change", async (event) => {
  const file = event.target.files && event.target.files[0];
  event.target.value = "";
  if (!file || !state.project) return;
  try {
    setStatus(`Uploading ${file.name}...`);
    const bytes_base64 = await fileToBase64(file);
    const result = await api("POST", `/projects/${state.project.id}/images`, { name: file.name, bytes_base64 });
    state.selectedIds = new Set([result.element.id]);
    await reloadProject(`Added ${file.name} (${result.element.w}x${result.element.h}).`);
    fit();
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("detect-regions").addEventListener("click", async () => {
  const [element] = selectedElements();
  if (!element) return;
  try {
    setStatus("Detecting regions...");
    const result = await api("POST", `/projects/${state.project.id}/detect-regions`, { elementId: element.id });
    await reloadProject(`Detected ${result.regions.length} region(s).`);
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("slice").addEventListener("click", async () => {
  const [element] = selectedElements();
  if (!element) return;
  try {
    setStatus("Slicing regions...");
    const result = await api("POST", `/projects/${state.project.id}/slice`, { elementId: element.id });
    state.selectedIds = new Set(result.created.map((e) => e.id));
    await reloadProject(`Sliced ${result.created.length} region(s) into new elements.`);
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("delete-element").addEventListener("click", () => deleteSelected());

async function deleteSelected() {
  const selected = selectedElements();
  if (!selected.length) return;
  try {
    for (const element of selected) {
      await api("DELETE", `/projects/${state.project.id}/elements/${element.id}`);
    }
    state.selectedIds = new Set();
    await reloadProject(`Removed ${selected.length} element(s) (image files kept on disk).`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

el("export-selected").addEventListener("click", async () => {
  const selected = selectedElements();
  if (!selected.length) return;
  try {
    setStatus("Exporting...");
    const result = await api("POST", `/projects/${state.project.id}/export`, {
      elementIds: selected.map((e) => e.id),
    });
    setStatus(`Exported ${result.items.length} element(s) to ${result.folder}`);
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("undo").addEventListener("click", () => runUndo());
el("redo").addEventListener("click", () => runRedo());

async function runUndo() {
  if (!state.project || !state.history.canUndo) return;
  try {
    await api("POST", `/projects/${state.project.id}/undo`);
    await reloadProject("Undid last change.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

async function runRedo() {
  if (!state.project || !state.history.canRedo) return;
  try {
    await api("POST", `/projects/${state.project.id}/redo`);
    await reloadProject("Redid change.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

el("fit").addEventListener("click", fit);

// ---- keyboard ----------------------------------------------------------------

window.addEventListener("keydown", (event) => {
  const key = event.key.toLowerCase();
  const meta = event.ctrlKey || event.metaKey;
  if (meta && key === "z") {
    event.preventDefault();
    if (event.shiftKey) runRedo();
    else runUndo();
    return;
  }
  if (meta && key === "y") {
    event.preventDefault();
    runRedo();
    return;
  }
  if (key === "escape") {
    state.selectedIds = new Set();
    syncToolbar();
    render();
    return;
  }
  if (key === "delete" || key === "backspace") {
    if (state.selectedIds.size) {
      event.preventDefault();
      deleteSelected();
    }
  }
});

function fileToBase64(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error("could not read file"));
    reader.onload = () => resolve(String(reader.result).split(",")[1] || "");
    reader.readAsDataURL(file);
  });
}

window.addEventListener("resize", render);

loadProjects()
  .then(() => setStatus("Ready. Create or open a project."))
  .catch((error) => setStatus(error.message, true));
