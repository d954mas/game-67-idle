// Shared page state + plumbing for the canvas surface.
//
// The page is thin: every action is one call to the shared ops layer over the
// HTTP API. This module holds the mutable page state, the fetch helper, small
// read-only view helpers over project.json, the image cache, and a refresh bus.
// Feature modules (home/workspace/layers/inspector/context menu/dnd) register
// their renderers into `hooks` and read state/helpers from here. It contains no
// canvas/business logic beyond fetching and re-rendering.

import { childrenOf, isNodeHidden } from "../tree.mjs";
import {
  canvasApiUrl,
  decodeLastProject,
  encodeLastProject,
  projectCacheKey,
  projectFileUrl,
  projectStoreId,
  setStoreParams,
} from "./store_scope.js";
import { toastError, toastInfo, toastPinned } from "./toasts.js";

// Video-animation generation is frozen (lead, 2026-07-06): unreliable local motion model,
// slow, no economics for iteration. Passport: docs/FREEZE_VIDEO_ANIM_2026-07-06.md. Gates
// only the GENERATION UI (context menu's "Animate this image" / "New animation card", the
// inspector's anim-card Generate section) — offline playback (renderFlipbook / Play-Stop)
// is unaffected and stays on. Flip to false to bring the generation UI back once unfrozen.
export const VIDEO_ANIM_FROZEN = true;

export const el = (id) => document.getElementById(id);

export const state = {
  projects: [],
  project: null,
  storeId: "studio",
  selectedIds: new Set(),
  selectedGroupId: null,
  // Figma nested-selection scope: the group the user has "entered" (double-click drills
  // in, Esc steps out); null = root scope. A single click resolves to the top-most
  // container within this scope. Page-only, reset on empty-canvas click / clearSelection.
  enteredGroupId: null,
  // Groups selected as units (marquee / canvas). selectedGroupId stays the SINGLE-group
  // primary the inspector/context menu key on; syncPrimaryGroup keeps it = the sole
  // member when exactly one group and no elements are selected, else null.
  selectedGroupIds: new Set(),
  // The group a plain click would select at the current scope (hover affordance). Page-only.
  hoverGroupId: null,
  // Region isolation mode (increment 6). regionEditId is the element currently in
  // region-edit mode (mode B / Figma-style isolation); null = object mode (mode A,
  // regions are passive, non-hit-testable overlays). selectedRegionIds are the
  // regions selected WITHIN mode B. Both are page-only, like the tool state, and
  // reset whenever the element selection changes.
  regionEditId: null,
  selectedRegionIds: new Set(),
  // Region-edit tool + in-progress polygon draft (page-only, like regionEditId; never
  // journaled). regionTool ∈ {select, rect, polygon} drives the region-edit tool row;
  // polygonDraft holds the vertices [[x,y]...] being placed (SOURCE pixels of the edited
  // element) and polygonHover the live cursor point. All reset to select/[] on mode exit.
  regionTool: "select",
  polygonDraft: [],
  polygonHover: null,
  // The text element currently open in the inline textarea editor (T tool / dblclick),
  // or null. Page-only (like regionEditId); the workspace owns the overlay DOM and skips
  // painting this element's glyphs while it is being edited.
  editingTextId: null,
  history: { canUndo: false, canRedo: false },
  // Copy buffer for Ctrl+C/Ctrl+V of canvas nodes (T0227) — PAGE VIEW-STATE, never
  // journaled or persisted. { spec: <tree.buildNodesSpec output>, pastes: <count> };
  // null when nothing has been copied. Each repeat paste steps the offset again. The
  // journaled gesture is the pasteNodes op, not this buffer. Cleared on nothing.
  clipboard: null,
  viewport: { scale: 1, offsetX: 0, offsetY: 0 },
  tool: "select", // "select" | "pan"
  spacePan: false, // Space-hold temporarily activates pan
  // Alt-hold peeks at the clip-ghost of a selected element's clipped-out portion. View-
  // state only (never journaled/persisted) — the ghost is hidden by default (T0224 item 6).
  clipGhostPeek: false,
  // Group ids EXPANDED in the layers panel. Figma reveal model: groups collapse by
  // DEFAULT (only ids in this set show their children), the selection's ancestor path
  // auto-expands (revealSelectionPath), and the caret toggles membership. Page-only, kept
  // while the page lives (T0224 item 8a).
  expandedGroups: new Set(),
  expandedElements: new Set(), // element ids whose region tree is expanded in layers (page-only)
  cssWidth: 0, // stage size in CSS pixels (backing store is dpr-scaled)
  cssHeight: 0,
  // Journal seq recorded at region-edit entry: in-mode Ctrl+Z never undoes past
  // it (pre-mode ops need Esc first — lead's rule). Page-only, like regionEditId.
  regionEditBaseSeq: null,
};

// Panel renderers are registered by their modules during init; refresh() fans out
// to whichever are present so any module can trigger a coherent re-render.
export const hooks = {
  renderCanvas: () => {},
  renderLayers: () => {},
  renderInspector: () => {},
  renderHistory: () => {},
  syncTopBar: () => {},
  renderHome: () => {},
  openProject: () => {}, // set by canvas.js: (id, opts) => Promise
  showHome: () => {}, // set by canvas.js: () => void
};

export function refresh() {
  hooks.renderCanvas();
  hooks.renderLayers();
  hooks.renderInspector();
  hooks.renderHistory();
  hooks.syncTopBar();
}

// Return keyboard focus to the stage after a committed inline/inspector edit, so a
// following Ctrl+Z reaches the global shortcut handler instead of the browser's
// text-undo inside the (still-focused) input. #stage is tabindex="-1".
export function focusStage() {
  const stage = el("stage");
  if (stage) stage.focus();
}

// Feedback goes through the toast layer (T0203) — there is no permanent status bar.
// setStatus/setStatusLinks stay as the shared vocabulary every module already speaks;
// they just route to the right toast kind: a transient confirmation is an info toast
// (auto-hides), a failure is an error toast (persists until dismissed, never swallowed),
// and an export/render outcome with download links is a pinned-result toast. Long ops
// (detect/slice/render/export) bypass this and use runLongOp so their progress toast
// resolves in place — see actions.js + toasts.js.
export function setStatus(message, isError = false) {
  if (!message) return;
  if (isError) toastError(message);
  else toastInfo(message);
}

// A confirmation that also carries clickable download links (export / render result):
// a pinned toast that stays until dismissed. `links` is [{ href, label }].
export function setStatusLinks(message, links = []) {
  toastPinned(message, links);
}

export async function api(method, path, body) {
  const res = await fetch(canvasApiUrl(path, state.storeId), {
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

// Ancestor-aware visibility: hidden when the element itself or ANY ancestor group is
// hidden (delegates to the shared tree walk so canvas + panel agree, and nested groups
// cascade correctly). The `hidden` arg is kept for call-site compatibility (a precomputed
// set of directly-hidden group ids) but the tree walk is the source of truth now.
export function isElementHidden(element) {
  return isNodeHidden(state.project, element);
}

// Direct member elements of a group (its scope's elements), backed by the shared
// tree so the panel + canvas read one source. Native array order (== paint order for
// a v1 project), which the reorder/z-order math relies on.
export function memberElements(groupId) {
  return childrenOf(state.project, groupId).elements;
}

// Root-scope (ungrouped) elements, backed by the same tree walk.
export function ungroupedElements() {
  return childrenOf(state.project, null).elements;
}

export function regionCount(element) {
  return Array.isArray(element.regions) ? element.regions.length : 0;
}

// ---- image cache -------------------------------------------------------------

const imageCache = new Map(); // element.src -> HTMLImageElement

export function fileUrl(element) {
  return projectFileUrl(state.project, element.src);
}

// The first image element of a project (used for a project card cover thumbnail).
export function coverUrl(project) {
  const first = (project.elements || []).find((element) => element.type === "image" && element.src);
  return first ? projectFileUrl(project, first.src) : null;
}

// Content-addressed image cache lookup by a bare src string (T0265 F4). This is the ONE image
// cache for the app: element images (imageFor) and flipbook keyframes (workspace's flipbook
// player) both resolve through it, so there is no duplicated per-frame Map. Same
// onload -> repaint path as before; a src is stable + content-addressed, so the cache never
// goes stale.
export function imageForSrc(src) {
  const key = projectCacheKey(state.project, src);
  if (imageCache.has(key)) return imageCache.get(key);
  const img = new Image();
  img.onload = () => hooks.renderCanvas();
  img.src = projectFileUrl(state.project, src);
  imageCache.set(key, img);
  return img;
}

export function imageFor(element) {
  return imageForSrc(element.src);
}

export function clearImageCache() {
  imageCache.clear();
}

// ---- selection ---------------------------------------------------------------

export function selectOnly(elementId) {
  state.selectedGroupId = null;
  state.selectedGroupIds = new Set();
  state.selectedRegionIds = new Set();
  state.regionEditId = null;
  state.selectedIds = elementId ? new Set([elementId]) : new Set();
}

export function toggleSelect(elementId) {
  // Keeps any selected GROUPS: Ctrl/Shift-click builds a mixed element+group
  // selection (lead 2026-07-03) — marquee already produces mixed selections and
  // every node-batch op (move/reorder/delete/copy) handles them.
  state.selectedRegionIds = new Set();
  state.regionEditId = null;
  if (state.selectedIds.has(elementId)) state.selectedIds.delete(elementId);
  else state.selectedIds.add(elementId);
  syncPrimaryGroup();
}

// Select a single group as the whole selection (label click, layers head, canvas
// Figma-select, context menu). Sets both the singular primary and the plural set so the
// canvas + inspector agree. Does NOT touch enteredGroupId (scope is managed by callers).
export function selectGroupOnly(groupId) {
  state.selectedIds = new Set();
  state.selectedRegionIds = new Set();
  state.regionEditId = null;
  state.selectedGroupId = groupId || null;
  state.selectedGroupIds = groupId ? new Set([groupId]) : new Set();
}

// Keep selectedGroupId (the single-group primary the inspector/context menu key on) in
// sync with the plural selectedGroupIds set: the sole selected group when exactly one
// group and no elements are selected, else null (a mixed/multi selection has no primary).
export function syncPrimaryGroup() {
  state.selectedGroupId =
    state.selectedGroupIds.size === 1 && state.selectedIds.size === 0 ? [...state.selectedGroupIds][0] : null;
}

export function clearSelection() {
  state.selectedIds = new Set();
  state.selectedGroupId = null;
  state.selectedGroupIds = new Set();
  state.selectedRegionIds = new Set();
  state.regionEditId = null;
  state.enteredGroupId = null;
  state.editingTextId = null;
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
  if (state.regionEditId !== elementId) state.regionEditBaseSeq = state.history.seq ?? null;
  state.regionEditId = elementId;
  // Fresh isolation starts on the Select tool with no polygon draft.
  state.regionTool = "select";
  state.polygonDraft = [];
  state.polygonHover = null;
  state.expandedElements.add(elementId);
}

// Leave isolation: clears region selection, the draw tool + any draft, AND the mode
// (one Escape step).
export function exitRegionEdit() {
  state.regionEditId = null;
  state.regionEditBaseSeq = null;
  state.selectedRegionIds = new Set();
  state.regionTool = "select";
  state.polygonDraft = [];
  state.polygonHover = null;
}

// Reconcile region-edit isolation (page-only state) against a (re)loaded project.
// Undo/redo/reload can invalidate the mode: the edited element or some of its
// regions may be gone. Figma-style rule — undo/redo act on the document journal, not
// the mode, so this only fixes up the mode + region selection: exit the mode when the
// element is missing or hidden (nothing editable to isolate); otherwise STAY in it
// (even with zero regions — the empty-state hint shows) and prune the region
// selection to ids that still exist. Pure (no DOM / module state), so it is
// unit-testable in node and mirrors isElementHidden's visibility rule.
export function reconcileRegionEdit(project, regionEditId, selectedRegionIds) {
  const exited = { regionEditId: null, selectedRegionIds: new Set() };
  if (!regionEditId) return exited;
  const items = (project && project.elements) || [];
  const groupList = (project && project.groups) || [];
  const element = items.find((item) => item.id === regionEditId);
  if (!element) return exited;
  const hiddenGroups = new Set(groupList.filter((group) => group.visible === false).map((group) => group.id));
  const hidden = element.visible === false || (Boolean(element.groupId) && hiddenGroups.has(element.groupId));
  if (hidden) return exited;
  const liveRegionIds = new Set((element.regions || []).map((region) => region.id));
  const pruned = new Set([...(selectedRegionIds || [])].filter((id) => liveRegionIds.has(id)));
  return { regionEditId, selectedRegionIds: pruned };
}

// Select a region on its parent element (inspector rows / layers tree). Selecting a
// region implies entering mode B on that element; Shift toggles multi. The parent's
// region tree is auto-expanded so the row stays visible.
export function selectRegion(elementId, regionId, additive = false) {
  state.selectedGroupId = null;
  state.selectedIds = new Set([elementId]);
  if (state.regionEditId !== elementId) state.regionEditBaseSeq = state.history.seq ?? null;
  state.regionEditId = elementId;
  if (additive) {
    if (state.selectedRegionIds.has(regionId)) state.selectedRegionIds.delete(regionId);
    else state.selectedRegionIds.add(regionId);
  } else {
    state.selectedRegionIds = new Set([regionId]);
  }
  state.expandedElements.add(elementId);
}

// The contiguous run of ids between `anchorId` and `targetId` (inclusive) in the given
// visual order, or null when either id is absent (the caller then falls back to a plain
// select). PURE — shared by the layers panel and the inspector Regions list so Shift-range
// behaves identically in both (T0224 item 5: one helper, no copy).
export function rangeSelectIds(orderedIds, anchorId, targetId) {
  const from = orderedIds.indexOf(anchorId);
  const to = orderedIds.indexOf(targetId);
  if (from === -1 || to === -1) return null;
  const [lo, hi] = from <= to ? [from, to] : [to, from];
  return orderedIds.slice(lo, hi + 1);
}

// Select a SET of regions on one element (Shift-range in the inspector Regions list):
// enters region-edit isolation on the element (like selectRegion) and replaces the region
// selection with the given ids. Page-only.
export function selectRegionRange(elementId, regionIds) {
  state.selectedGroupId = null;
  state.selectedIds = new Set([elementId]);
  if (state.regionEditId !== elementId) state.regionEditBaseSeq = state.history.seq ?? null;
  state.regionEditId = elementId;
  state.selectedRegionIds = new Set(regionIds);
  state.expandedElements.add(elementId);
}

// ---- project lifecycle -------------------------------------------------------

const LAST_KEY = "canvas.lastProject";

export function rememberLastProject(id) {
  try {
    if (id) localStorage.setItem(LAST_KEY, encodeLastProject(id, state.storeId));
    else localStorage.removeItem(LAST_KEY);
  } catch {
    // Private mode / disabled storage: deep links still work, restore just no-ops.
  }
}

export function lastProjectId() {
  const ref = lastProjectRef();
  return ref ? ref.projectId : null;
}

export function lastProjectRef() {
  try {
    return decodeLastProject(localStorage.getItem(LAST_KEY));
  } catch {
    return null;
  }
}

export function setProjectParam(id, storeId = state.storeId) {
  const url = new URL(window.location.href);
  if (id) url.searchParams.set("project", id);
  else url.searchParams.delete("project");
  setStoreParams(url.searchParams, storeId);
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
    state.history = { canUndo: false, canRedo: false, seq: null };
    return;
  }
  try {
    const history = await api("GET", `/projects/${state.project.id}/history`);
    // history_seq = current journal head; the region-edit undo clamp compares
    // against the seq captured at mode entry.
    state.history = { canUndo: history.canUndo, canRedo: history.canRedo, seq: history.history_seq ?? null };
  } catch {
    state.history = { canUndo: false, canRedo: false, seq: null };
  }
}

// Adopt a new project object into page state: prune the selection to still-existing
// elements/groups and reconcile region-edit isolation (page-only state that undo/
// redo/reload can invalidate — the canvas then re-reads regions fresh, no stale rects,
// no dead mode). Shared by reloadProject (GET) and applyMutation (op response) so both
// paths treat the incoming project identically.
function ingestProject(project) {
  state.project = project;
  state.storeId = projectStoreId(project);
  const alive = new Set(elements().map((element) => element.id));
  state.selectedIds = new Set([...state.selectedIds].filter((id) => alive.has(id)));
  const aliveGroups = new Set(groups().map((group) => group.id));
  if (state.selectedGroupId && !aliveGroups.has(state.selectedGroupId)) state.selectedGroupId = null;
  state.selectedGroupIds = new Set([...state.selectedGroupIds].filter((id) => aliveGroups.has(id)));
  // The entered scope can vanish under undo/redo/reload (its group was deleted); fall
  // back to root so clicks never resolve against a dead scope.
  if (state.enteredGroupId && !aliveGroups.has(state.enteredGroupId)) state.enteredGroupId = null;
  const region = reconcileRegionEdit(state.project, state.regionEditId, state.selectedRegionIds);
  state.regionEditId = region.regionEditId;
  state.selectedRegionIds = region.selectedRegionIds;
  if (!state.regionEditId) {
    // Left the mode (element/regions gone): drop the draw tool + any polygon draft too.
    state.regionEditBaseSeq = null;
    state.regionTool = "select";
    state.polygonDraft = [];
    state.polygonHover = null;
  }
}

// Drive the page from a mutating op's RESPONSE — the fast path with ZERO follow-up
// GETs. Every mutating API response carries the updated {project} and the folded
// {history} flags, so the page applies both straight from the result instead of the
// old reload double-GET (GET project + GET /history). A response missing a project
// (or a null result) falls back to a full reloadProject resync. Returns a promise so
// callers can `await` it uniformly with reloadProject.
export function applyMutation(result, message) {
  if (!result || !result.project) return reloadProject(message);
  ingestProject(result.project);
  if (result.history) {
    state.history = {
      canUndo: !!result.history.canUndo,
      canRedo: !!result.history.canRedo,
      seq: result.history.seq ?? null,
    };
  }
  refresh();
  if (message !== undefined) setStatus(message);
  return Promise.resolve();
}

// Re-fetch the project from disk (the source of truth) and re-render — the resync
// path for genuine reloads (initial open, or a response that carried no project).
// Mutating actions use applyMutation instead, so no op triggers this double-GET.
export async function reloadProject(message) {
  if (!state.project) return;
  ingestProject((await api("GET", `/projects/${state.project.id}`)).project);
  await refreshHistory();
  refresh();
  if (message !== undefined) setStatus(message);
}
