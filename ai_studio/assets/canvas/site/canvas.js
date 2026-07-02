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
  selectedId: null,
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

function selectedElement() {
  return elements().find((e) => e.id === state.selectedId) || null;
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
    if (element.id === state.selectedId) {
      ctx.strokeStyle = "#77a7ff";
      ctx.lineWidth = 2;
      ctx.strokeRect(origin.x, origin.y, w, h);
      drawRegions(element, vp);
    }
  }
}

function drawRegions(element, vp) {
  const regions = element.regions || [];
  ctx.lineWidth = 1;
  ctx.strokeStyle = "#3fc7ba";
  ctx.fillStyle = "rgba(63, 199, 186, 0.12)";
  for (const region of regions) {
    const rect = region.rect || region.content_bbox;
    if (!rect) continue;
    const p = imageToScreenPoint({ x: element.x + rect[0], y: element.y + rect[1] }, vp);
    const w = rect[2] * vp.scale;
    const h = rect[3] * vp.scale;
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
  const hasSelection = Boolean(selectedElement());
  el("project-title").textContent = state.project ? state.project.title : "No project";
  el("add-image").disabled = !hasProject;
  el("fit").disabled = !hasProject;
  el("detect-regions").disabled = !hasSelection;
  el("delete-element").disabled = !hasSelection;
}

async function loadProjects() {
  state.projects = (await api("GET", "/projects")).projects;
  renderProjectList();
}

async function openProject(id) {
  state.project = (await api("GET", `/projects/${id}`)).project;
  state.selectedId = null;
  imageCache.clear();
  renderProjectList();
  syncToolbar();
  fit();
  setStatus(`Opened ${state.project.title}.`);
}

function refreshProject(project) {
  state.project = project;
  renderProjectList();
  syncToolbar();
  render();
}

// ---- pointer interaction -----------------------------------------------------

let drag = null; // { mode: "pan"|"element", startX, startY, origOffset, element, origX, origY }
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
  moveSaveTimer = setTimeout(saveSelectedPosition, 300);
}

async function saveSelectedPosition() {
  const element = selectedElement();
  if (!element) return;
  try {
    const result = await api("PATCH", `/projects/${state.project.id}/elements/${element.id}`, {
      x: Math.round(element.x),
      y: Math.round(element.y),
    });
    // Keep the local element in sync without clobbering an in-progress drag.
    const stored = elements().find((e) => e.id === element.id);
    if (stored && !drag) Object.assign(stored, result.element);
  } catch (error) {
    setStatus(error.message, true);
  }
}

canvas.addEventListener("mousedown", (event) => {
  const screen = pointer(event);
  const world = screenToImagePoint(screen, state.viewport);
  const hit = hitElement(world);
  if (hit) {
    state.selectedId = hit.id;
    drag = { mode: "element", startX: screen.x, startY: screen.y, element: hit, origX: hit.x, origY: hit.y };
    canvas.classList.add("dragging");
    syncToolbar();
    render();
  } else {
    state.selectedId = null;
    drag = { mode: "pan", startX: screen.x, startY: screen.y, origOffset: { ...state.viewport } };
    canvas.classList.add("dragging");
    syncToolbar();
    render();
  }
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
    drag.element.x = drag.origX + dx;
    drag.element.y = drag.origY + dy;
    render();
    scheduleMoveSave();
  }
});

window.addEventListener("mouseup", () => {
  if (!drag) return;
  const wasElement = drag.mode === "element";
  drag = null;
  canvas.classList.remove("dragging");
  if (wasElement) {
    clearTimeout(moveSaveTimer);
    saveSelectedPosition();
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
    refreshProject(result.project);
    state.selectedId = result.element.id;
    syncToolbar();
    fit();
    setStatus(`Added ${file.name} (${result.element.w}x${result.element.h}).`);
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("detect-regions").addEventListener("click", async () => {
  const element = selectedElement();
  if (!element) return;
  try {
    setStatus("Detecting regions...");
    const result = await api("POST", `/projects/${state.project.id}/detect-regions`, { elementId: element.id });
    refreshProject(result.project);
    setStatus(`Detected ${result.regions.length} region(s).`);
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("delete-element").addEventListener("click", async () => {
  const element = selectedElement();
  if (!element) return;
  try {
    const result = await api("DELETE", `/projects/${state.project.id}/elements/${element.id}`);
    state.selectedId = null;
    refreshProject(result.project);
    setStatus("Element removed (image file kept on disk).");
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("fit").addEventListener("click", fit);

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
