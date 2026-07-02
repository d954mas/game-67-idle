// Shared page state + plumbing for the canvas surface.
//
// The page is thin: every action is one call to the shared ops layer over the
// HTTP API. This module holds the mutable page state, the fetch helper, small
// read-only view helpers over project.json, the image cache, and a refresh bus.
// Feature modules (home/workspace/layers/inspector/context menu/dnd) register
// their renderers into `hooks` and read state/helpers from here. It contains no
// canvas/business logic beyond fetching and re-rendering.

export const el = (id) => document.getElementById(id);

export const state = {
  projects: [],
  project: null,
  selectedIds: new Set(),
  selectedGroupId: null,
  // Region isolation mode (increment 6). regionEditId is the element currently in
  // region-edit mode (mode B / Figma-style isolation); null = object mode (mode A,
  // regions are passive, non-hit-testable overlays). selectedRegionIds are the
  // regions selected WITHIN mode B. Both are page-only, like the tool state, and
  // reset whenever the element selection changes.
  regionEditId: null,
  selectedRegionIds: new Set(),
  history: { canUndo: false, canRedo: false },
  viewport: { scale: 1, offsetX: 0, offsetY: 0 },
  tool: "select", // "select" | "pan"
  spacePan: false, // Space-hold temporarily activates pan
  collapsedGroups: new Set(), // group ids collapsed in the layers panel (page-only)
  expandedElements: new Set(), // element ids whose region tree is expanded in layers (page-only)
  cssWidth: 0, // stage size in CSS pixels (backing store is dpr-scaled)
  cssHeight: 0,
};

// Panel renderers are registered by their modules during init; refresh() fans out
// to whichever are present so any module can trigger a coherent re-render.
export const hooks = {
  renderCanvas: () => {},
  renderLayers: () => {},
  renderInspector: () => {},
  syncTopBar: () => {},
  renderHome: () => {},
  openProject: () => {}, // set by canvas.js: (id, opts) => Promise
  showHome: () => {}, // set by canvas.js: () => void
};

export function refresh() {
  hooks.renderCanvas();
  hooks.renderLayers();
  hooks.renderInspector();
  hooks.syncTopBar();
}

// Return keyboard focus to the stage after a committed inline/inspector edit, so a
// following Ctrl+Z reaches the global shortcut handler instead of the browser's
// text-undo inside the (still-focused) input. #stage is tabindex="-1".
export function focusStage() {
  const stage = el("stage");
  if (stage) stage.focus();
}

// Both views carry a status line (#status in the workspace, #home-status on
// home) so errors from home ops (create/delete) are never reported into a
// hidden node.
function statusNodes() {
  return [el("status"), el("home-status")].filter(Boolean);
}

export function setStatus(message, isError = false) {
  for (const node of statusNodes()) {
    node.innerHTML = "";
    node.textContent = message;
    node.classList.toggle("error", isError);
  }
}

// Append a status line that also carries clickable download links (used after
// export / render screen). `links` is [{ href, label }].
export function setStatusLinks(message, links = []) {
  for (const node of statusNodes()) {
    node.classList.remove("error");
    node.textContent = `${message} `;
    for (const link of links) {
      const a = document.createElement("a");
      a.href = link.href;
      a.textContent = link.label;
      a.target = "_blank";
      a.rel = "noreferrer";
      a.className = "dl";
      node.appendChild(a);
      node.appendChild(document.createTextNode(" "));
    }
  }
}

export async function api(method, path, body) {
  const res = await fetch(`/api/canvas${path}`, {
    method,
    headers: body ? { "content-type": "application/json" } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || res.statusText);
  return data;
}

// ---- read-only helpers over the current project ------------------------------

export function elements() {
  return (state.project && state.project.elements) || [];
}

export function groups() {
  return (state.project && state.project.groups) || [];
}

export function groupById(id) {
  return groups().find((group) => group.id === id) || null;
}

export function elementById(id) {
  return elements().find((element) => element.id === id) || null;
}

export function selectedElements() {
  return elements().filter((element) => state.selectedIds.has(element.id));
}

export function isSelected(element) {
  return state.selectedIds.has(element.id);
}

export function hiddenGroupIds() {
  return new Set(groups().filter((group) => group.visible === false).map((group) => group.id));
}

export function isElementHidden(element, hidden) {
  if (element.visible === false) return true;
  return Boolean(element.groupId) && (hidden || hiddenGroupIds()).has(element.groupId);
}

export function memberElements(groupId) {
  return elements().filter((element) => element.groupId === groupId);
}

export function ungroupedElements() {
  return elements().filter((element) => !element.groupId);
}

export function regionCount(element) {
  return Array.isArray(element.regions) ? element.regions.length : 0;
}

// ---- image cache -------------------------------------------------------------

const imageCache = new Map(); // element.src -> HTMLImageElement

export function fileUrl(element) {
  return `/api/canvas/projects/${state.project.id}/${element.src}`;
}

// The first image element of a project (used for a project card cover thumbnail).
export function coverUrl(project) {
  const first = (project.elements || []).find((element) => element.type === "image" && element.src);
  return first ? `/api/canvas/projects/${project.id}/${first.src}` : null;
}

export function imageFor(element) {
  if (imageCache.has(element.src)) return imageCache.get(element.src);
  const img = new Image();
  img.onload = () => hooks.renderCanvas();
  img.src = fileUrl(element);
  imageCache.set(element.src, img);
  return img;
}

export function clearImageCache() {
  imageCache.clear();
}

// ---- selection ---------------------------------------------------------------

export function selectOnly(elementId) {
  state.selectedGroupId = null;
  state.selectedRegionIds = new Set();
  state.regionEditId = null;
  state.selectedIds = elementId ? new Set([elementId]) : new Set();
}

export function toggleSelect(elementId) {
  state.selectedGroupId = null;
  state.selectedRegionIds = new Set();
  state.regionEditId = null;
  if (state.selectedIds.has(elementId)) state.selectedIds.delete(elementId);
  else state.selectedIds.add(elementId);
}

export function clearSelection() {
  state.selectedIds = new Set();
  state.selectedGroupId = null;
  state.selectedRegionIds = new Set();
  state.regionEditId = null;
}

// The element currently in region-edit isolation (mode B): its regions become the
// only hit-testable things on the canvas. Null in object mode (mode A).
export function regionEditElement() {
  if (!state.regionEditId) return null;
  const element = elementById(state.regionEditId);
  if (!element || isElementHidden(element)) {
    state.regionEditId = null;
    return null;
  }
  return element;
}

// Enter region-edit isolation for an element (double-click / "Edit regions" menu /
// "+ Add region"). Like Figma enter-group: nothing region-selected yet.
export function enterRegionEdit(elementId) {
  const element = elementById(elementId);
  if (!element) return;
  state.selectedGroupId = null;
  state.selectedIds = new Set([elementId]);
  state.regionEditId = elementId;
  state.expandedElements.add(elementId);
}

// Leave isolation: clears region selection AND the mode (one Escape step).
export function exitRegionEdit() {
  state.regionEditId = null;
  state.selectedRegionIds = new Set();
}

// Select a region on its parent element (inspector rows / layers tree). Selecting a
// region implies entering mode B on that element; Shift toggles multi. The parent's
// region tree is auto-expanded so the row stays visible.
export function selectRegion(elementId, regionId, additive = false) {
  state.selectedGroupId = null;
  state.selectedIds = new Set([elementId]);
  state.regionEditId = elementId;
  if (additive) {
    if (state.selectedRegionIds.has(regionId)) state.selectedRegionIds.delete(regionId);
    else state.selectedRegionIds.add(regionId);
  } else {
    state.selectedRegionIds = new Set([regionId]);
  }
  state.expandedElements.add(elementId);
}

// ---- project lifecycle -------------------------------------------------------

const LAST_KEY = "canvas.lastProject";

export function rememberLastProject(id) {
  try {
    if (id) localStorage.setItem(LAST_KEY, id);
    else localStorage.removeItem(LAST_KEY);
  } catch {
    // Private mode / disabled storage: deep links still work, restore just no-ops.
  }
}

export function lastProjectId() {
  try {
    return localStorage.getItem(LAST_KEY);
  } catch {
    return null;
  }
}

export function setProjectParam(id) {
  const url = new URL(window.location.href);
  if (id) url.searchParams.set("project", id);
  else url.searchParams.delete("project");
  url.searchParams.delete("select"); // the debug select hook is one-shot
  url.searchParams.delete("regions"); // the debug region-edit hook is one-shot
  window.history.replaceState(null, "", url);
}

export async function loadProjects() {
  state.projects = (await api("GET", "/projects")).projects;
  return state.projects;
}

export async function refreshHistory() {
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

// Re-fetch the project from disk (the source of truth after any mutating op) and
// re-render. Selection is pruned to still-existing elements/groups.
export async function reloadProject(message) {
  if (!state.project) return;
  state.project = (await api("GET", `/projects/${state.project.id}`)).project;
  const alive = new Set(elements().map((element) => element.id));
  state.selectedIds = new Set([...state.selectedIds].filter((id) => alive.has(id)));
  if (state.selectedGroupId && !groupById(state.selectedGroupId)) state.selectedGroupId = null;
  await refreshHistory();
  refresh();
  if (message !== undefined) setStatus(message);
}
