// Layers panel: a flat, group-aware tree of ELEMENTS and GROUPS only (the region
// list lives in the inspector now, not here). Ungrouped elements sit at the top
// level; each group is a collapsible section (eye toggle + inline-rename name) with
// its member elements indented beneath. Clicking a row selects on the canvas (both
// ways through refresh); dragging a row reorders it among its siblings (a drop
// between two rows, shown by an insertion line) or reparents it (a drop onto a
// screen header / another group's row / the top level). Pure rendering/input — all
// mutations go through actions.
//
// Render is structure-signature guarded: a selection-only change does not rebuild
// the DOM, it only re-applies selection classes. That keeps a row's node stable
// across the two clicks of a double-click, so double-click rename opens its editor.
import {
  el,
  elementById,
  fileUrl,
  groups,
  hooks,
  memberElements,
  refresh,
  selectOnly,
  state,
  toggleSelect,
  ungroupedElements,
} from "./app.js";
import {
  assignElementsToGroup,
  renameElement,
  renameGroup,
  reorderElementTo,
  setElementVisible,
  setGroupVisible,
} from "./actions.js";
import { inlineEdit } from "./inline.js";
import { openContextMenu } from "./context_menu.js";

// Pointer-based element-row drag (reparent OR reorder; kept deliberately simple).
let layerDrag = null; // { id, name, rowEl, startX, startY, active }

function eyeButton(visible, onToggle) {
  const button = document.createElement("button");
  button.className = "eye";
  button.type = "button";
  button.title = visible ? "Hide" : "Show";
  button.textContent = visible ? "◉" : "◯"; // ◉ shown / ◯ hidden
  button.classList.toggle("off", !visible);
  button.addEventListener("mousedown", (event) => event.stopPropagation());
  button.addEventListener("click", (event) => {
    event.stopPropagation();
    onToggle();
  });
  return button;
}

function elementRow(element, indented) {
  const row = document.createElement("div");
  row.className = "layer-row";
  row.dataset.elementId = element.id;
  row.dataset.groupId = element.groupId || "";
  if (indented) row.classList.add("indented");
  if (state.selectedIds.has(element.id)) row.classList.add("selected");

  // Spacer keeps element names aligned with group rows (which carry a caret).
  const spacer = document.createElement("span");
  spacer.className = "caret empty";
  row.appendChild(spacer);

  const thumb = document.createElement("img");
  thumb.className = "thumb";
  thumb.src = fileUrl(element);
  thumb.alt = "";
  thumb.draggable = false; // never start a native image drag when dragging a row
  row.appendChild(thumb);

  const name = document.createElement("span");
  name.className = "layer-name";
  name.textContent = element.name || element.id;
  name.title = element.name || element.id;
  row.appendChild(name);
  // Rename on double-click anywhere on the row (not the eye button).
  row.addEventListener("dblclick", (event) => {
    if (event.target.closest("button")) return;
    event.stopPropagation();
    inlineEdit(name, element.name || "", (next) => renameElement(element.id, next));
  });

  const regions = element.regions || [];
  if (regions.length) {
    const badge = document.createElement("span");
    badge.className = "badge";
    badge.textContent = `${regions.length}r`;
    badge.title = `${regions.length} region(s) — edit them in the inspector`;
    row.appendChild(badge);
  }

  row.appendChild(eyeButton(element.visible !== false, () => setElementVisible(element.id, element.visible === false)));

  row.addEventListener("mousedown", (event) => {
    if (event.button !== 0 || event.target.closest("button")) return;
    layerDrag = {
      id: element.id,
      name: element.name || element.id,
      rowEl: row,
      startX: event.clientX,
      startY: event.clientY,
      active: false,
    };
  });
  row.addEventListener("click", (event) => {
    if (event.shiftKey || event.ctrlKey || event.metaKey) toggleSelect(element.id);
    else selectOnly(element.id);
    refresh();
  });
  // Same context menu as on the canvas (keeps the whole selection when the row is
  // part of a 2+ selection, else selects this element first).
  row.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    if (!state.selectedIds.has(element.id)) selectOnly(element.id);
    refresh();
    openContextMenu(event.clientX, event.clientY, { kind: "element", elementId: element.id });
  });
  return row;
}

function groupSection(group) {
  const wrap = document.createElement("div");
  wrap.className = "layer-group";
  const collapsed = state.collapsedGroups.has(group.id);

  const head = document.createElement("div");
  head.className = "group-head";
  head.dataset.groupId = group.id;
  if (state.selectedGroupId === group.id) head.classList.add("selected");

  const caret = document.createElement("button");
  caret.className = "caret";
  caret.type = "button";
  caret.textContent = collapsed ? "▸" : "▾"; // ▸ / ▾
  caret.addEventListener("mousedown", (event) => event.stopPropagation());
  caret.addEventListener("click", (event) => {
    event.stopPropagation();
    if (collapsed) state.collapsedGroups.delete(group.id);
    else state.collapsedGroups.add(group.id);
    hooks.renderLayers();
  });
  head.appendChild(caret);

  const name = document.createElement("span");
  name.className = "group-name";
  name.textContent = group.name || "Group";
  name.title = "Double-click to rename";
  head.appendChild(name);
  head.addEventListener("dblclick", (event) => {
    if (event.target.closest("button")) return;
    event.stopPropagation();
    inlineEdit(name, group.name || "", (next) => renameGroup(group.id, next));
  });

  const members = memberElements(group.id);
  const count = document.createElement("span");
  count.className = "badge";
  count.textContent = `${members.length}`;
  head.appendChild(count);

  head.appendChild(eyeButton(group.visible !== false, () => setGroupVisible(group.id, group.visible === false)));

  head.addEventListener("click", () => {
    state.selectedGroupId = group.id;
    state.selectedIds = new Set();
    state.selectedRegionIds = new Set();
    refresh();
  });
  head.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    state.selectedGroupId = group.id;
    state.selectedIds = new Set();
    state.selectedRegionIds = new Set();
    refresh();
    openContextMenu(event.clientX, event.clientY, { kind: "group", groupId: group.id });
  });
  wrap.appendChild(head);

  if (!collapsed) {
    for (const element of members) wrap.appendChild(elementRow(element, true));
  }
  return wrap;
}

// ---- structure-signature guarded render --------------------------------------

let lastLayersSig = null;

// Everything that changes which rows exist and their text/badges/visibility —
// NOT the selection (that is applied as a lightweight overlay so selection clicks
// never rebuild the DOM out from under a pending double-click).
function layersSignature() {
  const parts = [];
  for (const e of ungroupedElements()) {
    parts.push(`e:${e.id}:${e.name || ""}:${e.visible !== false ? 1 : 0}:${(e.regions || []).length}`);
  }
  for (const g of groups()) {
    const collapsed = state.collapsedGroups.has(g.id);
    parts.push(`g:${g.id}:${g.name || ""}:${g.visible !== false ? 1 : 0}:${collapsed ? 1 : 0}`);
    if (!collapsed) {
      for (const m of memberElements(g.id)) {
        parts.push(` m:${m.id}:${m.name || ""}:${m.visible !== false ? 1 : 0}:${(m.regions || []).length}`);
      }
    }
  }
  return parts.join("\n");
}

function applyLayersSelection() {
  const list = el("layers-list");
  if (!list) return;
  for (const row of list.querySelectorAll(".layer-row[data-element-id]")) {
    row.classList.toggle("selected", state.selectedIds.has(row.dataset.elementId));
  }
  for (const head of list.querySelectorAll(".group-head[data-group-id]")) {
    head.classList.toggle("selected", state.selectedGroupId === head.dataset.groupId);
  }
}

export function renderLayers() {
  const list = el("layers-list");
  if (!list) return;
  // An open inline rename must survive selection-driven re-renders; only skip the
  // rebuild while the editor is still FOCUSED (a committed edit blurs to the stage).
  const editing = list.querySelector(".inline-input");
  if (editing && document.activeElement === editing) return;

  const sig = layersSignature();
  if (sig === lastLayersSig && list.childElementCount) {
    applyLayersSelection();
    return;
  }
  lastLayersSig = sig;
  list.replaceChildren();
  const ungrouped = ungroupedElements();
  const groupList = groups();
  if (!ungrouped.length && !groupList.length) {
    const empty = document.createElement("div");
    empty.className = "layers-empty";
    empty.textContent = "No layers yet.";
    list.appendChild(empty);
    return;
  }
  for (const element of ungrouped) list.appendChild(elementRow(element, false));
  for (const group of groupList) list.appendChild(groupSection(group));
  applyLayersSelection();
}

// ---- drag: reparent + reorder ------------------------------------------------

let dragGhost = null;
let dropLine = null;

function makeGhost(text, x, y) {
  removeGhost();
  dragGhost = document.createElement("div");
  dragGhost.className = "layer-drag-ghost";
  dragGhost.textContent = text;
  document.body.appendChild(dragGhost);
  moveGhost(x, y);
}

function moveGhost(x, y) {
  if (dragGhost) {
    dragGhost.style.left = `${x + 12}px`;
    dragGhost.style.top = `${y + 8}px`;
  }
}

function removeGhost() {
  if (dragGhost) {
    dragGhost.remove();
    dragGhost = null;
  }
}

function scopeOf(id) {
  const element = elementById(id);
  return element ? element.groupId || null : null;
}

// Is this drag a single element (reorder-eligible) or a 2+ multi-selection drag
// (reparent only — reordering many at once is out of scope)?
function isSingleDrag(dragId) {
  return !(state.selectedIds.has(dragId) && state.selectedIds.size > 1);
}

// Resolve the drop under the pointer into a plan:
//   { kind: "reparent", groupId }  — join a screen (or null = top level)
//   { kind: "reorder", scope, overId, after, rect } — move among same-scope siblings
//   { kind: "none" } — outside the panel
function dropPlan(clientX, clientY, dragId) {
  const node = document.elementFromPoint(clientX, clientY);
  if (!node || !node.closest("#layers-panel")) return { kind: "none" };
  const head = node.closest(".group-head");
  if (head && head.dataset.groupId) return { kind: "reparent", groupId: head.dataset.groupId };
  const row = node.closest(".layer-row[data-element-id]");
  const dragScope = scopeOf(dragId);
  if (row) {
    const overId = row.dataset.elementId;
    const overScope = row.dataset.groupId || null;
    if (overScope === dragScope && overId !== dragId && isSingleDrag(dragId)) {
      const rect = row.getBoundingClientRect();
      const after = clientY > rect.top + rect.height / 2;
      return { kind: "reorder", scope: dragScope, overId, after, rect };
    }
    if (overScope !== dragScope) return { kind: "reparent", groupId: overScope };
    return { kind: "reparent", groupId: overScope }; // same-scope, same/multi -> no-op reparent
  }
  return { kind: "reparent", groupId: null }; // panel header / empty space / gap = top level
}

// Target sibling index for a reorder plan (standard remove-then-insert math).
function reorderTargetIndex(dragId, plan) {
  const siblings = plan.scope ? memberElements(plan.scope) : ungroupedElements();
  const overIndex = siblings.findIndex((e) => e.id === plan.overId);
  const dragIndex = siblings.findIndex((e) => e.id === dragId);
  if (overIndex < 0 || dragIndex < 0) return null;
  let insert = plan.after ? overIndex + 1 : overIndex;
  if (dragIndex < insert) insert -= 1; // account for removing the dragged row first
  insert = Math.max(0, Math.min(siblings.length - 1, insert));
  return insert === dragIndex ? null : insert;
}

function clearDropHint() {
  const panel = el("layers-panel");
  if (panel) panel.classList.remove("drop-root");
  for (const node of document.querySelectorAll(".group-head.drop-target")) node.classList.remove("drop-target");
  if (dropLine && dropLine.parentNode) dropLine.parentNode.removeChild(dropLine);
}

function showDropLine(rect, after) {
  const list = el("layers-list");
  if (!list) return;
  if (!dropLine) {
    dropLine = document.createElement("div");
    dropLine.className = "layer-drop-line";
  }
  const listRect = list.getBoundingClientRect();
  const y = (after ? rect.bottom : rect.top) - listRect.top + list.scrollTop;
  dropLine.style.top = `${y}px`;
  dropLine.style.left = `${rect.left - listRect.left}px`;
  dropLine.style.width = `${rect.width}px`;
  list.appendChild(dropLine);
}

function updateDropHint(clientX, clientY, dragId) {
  clearDropHint();
  const plan = dropPlan(clientX, clientY, dragId);
  if (plan.kind === "reorder") {
    showDropLine(plan.rect, plan.after);
    return;
  }
  if (plan.kind === "reparent") {
    if (plan.groupId === null) {
      el("layers-panel").classList.add("drop-root");
      return;
    }
    const head = document.querySelector(`.group-head[data-group-id="${plan.groupId}"]`);
    if (head) head.classList.add("drop-target");
  }
}

function onLayerMouseMove(event) {
  if (!layerDrag) return;
  if (!layerDrag.active) {
    if (Math.hypot(event.clientX - layerDrag.startX, event.clientY - layerDrag.startY) < 4) return;
    layerDrag.active = true;
    document.body.classList.add("layer-dragging");
    if (layerDrag.rowEl) layerDrag.rowEl.classList.add("dragging");
    const multi = state.selectedIds.has(layerDrag.id) && state.selectedIds.size > 1;
    makeGhost(multi ? `${state.selectedIds.size} layers` : layerDrag.name, event.clientX, event.clientY);
  }
  moveGhost(event.clientX, event.clientY);
  updateDropHint(event.clientX, event.clientY, layerDrag.id);
}

function onLayerMouseUp(event) {
  if (!layerDrag) return;
  const drag = layerDrag;
  layerDrag = null;
  if (drag.rowEl) drag.rowEl.classList.remove("dragging");
  removeGhost();
  document.body.classList.remove("layer-dragging");
  clearDropHint();
  if (!drag.active) return; // a plain click — let the row's click handler select

  const plan = dropPlan(event.clientX, event.clientY, drag.id);
  if (plan.kind === "none") return;

  if (plan.kind === "reorder") {
    const target = reorderTargetIndex(drag.id, plan);
    if (target !== null) reorderElementTo(drag.id, target);
    return;
  }

  // Reparent (single row, or the whole selection when the dragged row is in a 2+ set).
  const groupId = plan.groupId ?? null;
  const ids = state.selectedIds.has(drag.id) && state.selectedIds.size > 1 ? [...state.selectedIds] : [drag.id];
  const allMatch = ids.every((id) => {
    const element = elementById(id);
    return element && (element.groupId || null) === (groupId || null);
  });
  if (allMatch) return; // no-op drop (already in the target scope)
  assignElementsToGroup(ids, groupId);
}

// ---- collapse rail -----------------------------------------------------------

const LAYERS_COLLAPSE_KEY = "canvas.layersCollapsed";

function setLayersCollapsed(collapsed) {
  const panel = el("layers-panel");
  if (!panel) return;
  panel.classList.toggle("collapsed", collapsed);
  try {
    localStorage.setItem(LAYERS_COLLAPSE_KEY, collapsed ? "1" : "0");
  } catch {
    // Private mode / disabled storage: collapse still works this session.
  }
  hooks.renderCanvas(); // the stage width changed — resize + repaint the canvas
}

export function initLayers() {
  hooks.renderLayers = renderLayers;
  // Right-click on the empty area of the list: create a group (groups the current
  // selection like Ctrl/Cmd+G, or creates an empty default-size one). Row/group
  // rows own their contextmenu handlers, so only background clicks land here.
  el("layers-list")?.addEventListener("contextmenu", (event) => {
    if (event.target.closest(".layer-row, .group-head, button")) return;
    event.preventDefault();
    openContextMenu(event.clientX, event.clientY, { kind: "layers-empty" });
  });

  // Collapse to a slim rail (header ☰) / re-open from the rail (icon button).
  const panel = el("layers-panel");
  try {
    if (panel && localStorage.getItem(LAYERS_COLLAPSE_KEY) === "1") panel.classList.add("collapsed");
  } catch {
    // ignore storage errors
  }
  el("layers-collapse")?.addEventListener("click", () => setLayersCollapsed(true));
  el("layers-expand")?.addEventListener("click", () => setLayersCollapsed(false));

  window.addEventListener("mousemove", onLayerMouseMove);
  window.addEventListener("mouseup", onLayerMouseUp);
}
