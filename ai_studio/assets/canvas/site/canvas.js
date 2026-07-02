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
  selectedGroupId: null,
  history: { canUndo: false, canRedo: false },
  viewport: { scale: 1, offsetX: 0, offsetY: 0 },
};
const imageCache = new Map(); // element.src -> HTMLImageElement
let groupLabelRects = []; // {groupId, x, y, w, h} in screen space, rebuilt each render

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

function groups() {
  return (state.project && state.project.groups) || [];
}

function groupById(id) {
  return groups().find((g) => g.id === id) || null;
}

function selectedElements() {
  return elements().filter((e) => state.selectedIds.has(e.id));
}

function isSelected(element) {
  return state.selectedIds.has(element.id);
}

// A set of group ids whose frame (and members) are hidden.
function hiddenGroupIds() {
  return new Set(groups().filter((g) => g.visible === false).map((g) => g.id));
}

// An element is hidden if it is explicitly invisible or lives in a hidden group.
// Hidden elements are neither drawn nor hit-testable.
function isElementHidden(element, hiddenGroups) {
  if (element.visible === false) return true;
  return Boolean(element.groupId) && (hiddenGroups || hiddenGroupIds()).has(element.groupId);
}

function memberElements(groupId) {
  return elements().filter((e) => e.groupId === groupId);
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
  const hidden = hiddenGroupIds();
  // Elements first (in z-order), skipping hidden ones.
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
  // Group frames + labels on top. Hidden groups are skipped entirely.
  groupLabelRects = [];
  for (const group of groups()) {
    if (group.visible === false) continue;
    drawGroupFrame(group, vp);
  }
}

// One Figma-like frame per visible group: a thin outline plus a name label just
// ABOVE the top-left corner. The label rect (screen space) is cached so clicks
// and drags on it can select/move the group.
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
  const hidden = hiddenGroupIds();
  const boxes = [
    ...elements().filter((e) => !isElementHidden(e, hidden)),
    ...groups().filter((g) => g.visible !== false),
  ];
  if (!boxes.length) return { x: 0, y: 0, width: 1024, height: 768 };
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const e of boxes) {
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
  el("group-selection").disabled = selected.length < 2;
  const group = state.selectedGroupId ? groupById(state.selectedGroupId) : null;
  el("render-screen").disabled = !group;
  el("group-toggle").disabled = !group;
  el("group-toggle").textContent = group && group.visible === false ? "Show" : "Hide";
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
  state.selectedGroupId = null;
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
  if (state.selectedGroupId && !groupById(state.selectedGroupId)) state.selectedGroupId = null;
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
  const hidden = hiddenGroupIds();
  for (let i = items.length - 1; i >= 0; i -= 1) {
    const e = items[i];
    if (isElementHidden(e, hidden)) continue;
    if (
      worldPoint.x >= e.x && worldPoint.x <= e.x + e.w &&
      worldPoint.y >= e.y && worldPoint.y <= e.y + e.h
    ) {
      return e;
    }
  }
  return null;
}

// Hit-test the cached group name labels (screen space); returns a group id or null.
function hitGroupLabel(screenPoint) {
  for (let i = groupLabelRects.length - 1; i >= 0; i -= 1) {
    const r = groupLabelRects[i];
    if (screenPoint.x >= r.x && screenPoint.x <= r.x + r.w && screenPoint.y >= r.y && screenPoint.y <= r.y + r.h) {
      return r.groupId;
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
  const labelGroupId = hitGroupLabel(screen);
  if (labelGroupId) {
    // Click the label = select the group; drag it = move the whole screen. Group
    // selection is exclusive of element selection.
    state.selectedGroupId = labelGroupId;
    state.selectedIds = new Set();
    const group = groupById(labelGroupId);
    const members = memberElements(labelGroupId).map((element) => ({ element, origX: element.x, origY: element.y }));
    drag = { mode: "group", startX: screen.x, startY: screen.y, group, origGroup: { x: group.x, y: group.y }, members };
    canvas.classList.add("dragging");
    syncToolbar();
    render();
    return;
  }
  const hit = hitElement(world);
  if (hit) {
    state.selectedGroupId = null;
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
    state.selectedGroupId = null;
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
  } else if (drag.mode === "group") {
    // Preview: move the frame and all members together. The move is persisted on
    // drop via a single patchGroup, which translates members server-side.
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
});

window.addEventListener("mouseup", () => {
  if (!drag) return;
  const mode = drag.mode;
  const finished = drag;
  drag = null;
  canvas.classList.remove("dragging");
  if (mode === "element") {
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
  } else if (mode === "group") {
    // One patchGroup on drop: the server translates the (still-persisted) members
    // by the same delta, so we send only the new frame origin and then reload.
    const moved = Math.round(finished.group.x) !== Math.round(finished.origGroup.x)
      || Math.round(finished.group.y) !== Math.round(finished.origGroup.y);
    if (!moved) return;
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

el("group-selection").addEventListener("click", async () => {
  const selected = selectedElements();
  if (selected.length < 2) return;
  const name = prompt("Screen name", "New screen");
  if (name === null) return;
  try {
    const result = await api("POST", `/projects/${state.project.id}/groups`, {
      name,
      fromElements: selected.map((e) => e.id),
    });
    state.selectedIds = new Set();
    state.selectedGroupId = result.group.id;
    await reloadProject(`Grouped ${selected.length} element(s) into "${result.group.name}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("render-screen").addEventListener("click", async () => {
  const group = state.selectedGroupId ? groupById(state.selectedGroupId) : null;
  if (!group) return;
  try {
    setStatus(`Rendering "${group.name}"...`);
    const result = await api("POST", `/projects/${state.project.id}/groups/${group.id}/render`, {});
    setStatus(`Rendered ${result.manifest.width}x${result.manifest.height} screen to ${result.folder}\\${result.file}`);
  } catch (error) {
    setStatus(error.message, true);
  }
});

el("group-toggle").addEventListener("click", async () => {
  const group = state.selectedGroupId ? groupById(state.selectedGroupId) : null;
  if (!group) return;
  try {
    await api("PATCH", `/projects/${state.project.id}/groups/${group.id}`, { visible: group.visible === false });
    await reloadProject(group.visible === false ? "Showed screen." : "Hid screen.");
  } catch (error) {
    setStatus(error.message, true);
  }
});

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
    state.selectedGroupId = null;
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
