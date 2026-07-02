// Layers panel: a flat, group-aware tree. Ungrouped elements sit at the top level;
// each group is a collapsible section (eye toggle + inline-rename name) with its
// member elements indented beneath. An element with regions is itself expandable
// (a caret reveals indented region rows whose numbers match the canvas badges and
// the inspector). Clicking a row selects on the canvas (both ways through refresh);
// dragging an element row onto a group header (or out to the top level) reassigns
// it. Pure rendering/input — all mutations go through actions.
import {
  el,
  elementById,
  fileUrl,
  groups,
  hooks,
  memberElements,
  refresh,
  selectOnly,
  selectRegion,
  state,
  toggleSelect,
  ungroupedElements,
} from "./app.js";
import {
  assignElementsToGroup,
  createGroupOrDefault,
  renameElement,
  renameRegion,
  setElementVisible,
  renameGroup,
  setGroupVisible,
} from "./actions.js";
import { inlineEdit } from "./inline.js";
import { openContextMenu } from "./context_menu.js";

// Pointer-based element-row reparent drag (kept deliberately simple).
let layerDrag = null; // { id, startX, startY, active }

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

function caretButton(element, hasRegions) {
  const button = document.createElement("button");
  button.className = "caret";
  button.type = "button";
  if (!hasRegions) {
    button.classList.add("empty");
    button.disabled = true;
    return button; // spacer, keeps names aligned with group rows
  }
  const expanded = state.expandedElements.has(element.id);
  button.textContent = expanded ? "▾" : "▸";
  button.title = expanded ? "Collapse regions" : "Expand regions";
  button.addEventListener("mousedown", (event) => event.stopPropagation());
  button.addEventListener("click", (event) => {
    event.stopPropagation();
    if (expanded) state.expandedElements.delete(element.id);
    else state.expandedElements.add(element.id);
    hooks.renderLayers();
  });
  return button;
}

function regionRow(element, region, index, indented) {
  const row = document.createElement("div");
  row.className = "layer-row region-row";
  if (indented) row.classList.add("in-group");
  row.dataset.groupId = element.groupId || "";
  if (state.regionEditId === element.id && state.selectedRegionIds.has(region.id)) row.classList.add("selected");

  const num = document.createElement("span");
  num.className = "region-num";
  num.textContent = String(index + 1);
  row.appendChild(num);

  const rect = region.rect || region.content_bbox || [0, 0, 0, 0];
  const name = document.createElement("span");
  name.className = "layer-name";
  name.textContent = region.name || `${rect[2]}×${rect[3]}`;
  name.title = `region ${index + 1}: ${rect[2]}×${rect[3]} @ (${rect[0]}, ${rect[1]})`;
  row.appendChild(name);

  row.addEventListener("click", (event) => {
    selectRegion(element.id, region.id, event.shiftKey || event.ctrlKey || event.metaKey);
    refresh();
  });
  // Double-click the region row to rename it (journaled via setRegions).
  row.addEventListener("dblclick", (event) => {
    event.stopPropagation();
    inlineEdit(name, region.name || "", (next) => renameRegion(element.id, region.id, next));
  });
  row.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    selectRegion(element.id, region.id, false);
    refresh();
    openContextMenu(event.clientX, event.clientY, { kind: "region", elementId: element.id, regionId: region.id });
  });
  return row;
}

function elementRow(element, indented) {
  const wrap = document.createElement("div");
  wrap.className = "layer-item";

  const row = document.createElement("div");
  row.className = "layer-row";
  row.dataset.elementId = element.id;
  row.dataset.groupId = element.groupId || "";
  if (indented) row.classList.add("indented");
  if (state.selectedIds.has(element.id)) row.classList.add("selected");

  const regions = element.regions || [];
  const hasRegions = regions.length > 0;
  row.appendChild(caretButton(element, hasRegions));

  const thumb = document.createElement("img");
  thumb.className = "thumb";
  thumb.src = fileUrl(element);
  thumb.alt = "";
  thumb.draggable = false; // never start a native image drag when reparenting
  row.appendChild(thumb);

  const name = document.createElement("span");
  name.className = "layer-name";
  name.textContent = element.name || element.id;
  name.title = element.name || element.id;
  row.appendChild(name);
  // Rename on double-click anywhere on the row (not the eye/caret buttons).
  row.addEventListener("dblclick", (event) => {
    if (event.target.closest("button")) return;
    event.stopPropagation();
    inlineEdit(name, element.name || "", (next) => renameElement(element.id, next));
  });

  if (hasRegions) {
    const badge = document.createElement("span");
    badge.className = "badge";
    badge.textContent = `${regions.length}r`;
    badge.title = `${regions.length} region(s)`;
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
  wrap.appendChild(row);

  if (hasRegions && state.expandedElements.has(element.id)) {
    regions.forEach((region, index) => wrap.appendChild(regionRow(element, region, index, indented)));
  }
  return wrap;
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
  caret.addEventListener("click", (event) => {
    event.stopPropagation();
    if (collapsed) state.collapsedGroups.delete(group.id);
    else state.collapsedGroups.add(group.id);
    hooks.renderLayers();
  });
  head.appendChild(caret);

  const name = document.createElement("span");
  name.className = "group-name";
  name.textContent = group.name || "Screen";
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

export function renderLayers() {
  const list = el("layers-list");
  if (!list) return;
  // An open inline rename must survive selection-driven re-renders, but a COMMITTED
  // edit blurs the input and hands focus back to the stage; only skip the rebuild
  // while the editor is still FOCUSED. (Skipping on mere presence froze the tree: the
  // committed input is never removed, so every later render — including undo's —
  // short-circuited and the layers/region rows never reflected the change.)
  const editing = list.querySelector(".inline-input");
  if (editing && document.activeElement === editing) return;
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
}

// ---- reparent drag -----------------------------------------------------------

let dragGhost = null;

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

// The drop target under the pointer: a group id (a group header or any row tagged
// with that group), null for ANY other spot in the panel (top-level rows, empty
// space, or the panel header) so dropping to root is always reachable, or undefined
// only when the pointer left the panel entirely.
function dropTarget(clientX, clientY) {
  const node = document.elementFromPoint(clientX, clientY);
  if (!node) return undefined;
  const head = node.closest(".group-head");
  if (head && head.dataset.groupId) return head.dataset.groupId;
  const row = node.closest(".layer-row");
  if (row && row.dataset.groupId) return row.dataset.groupId;
  if (node.closest("#layers-panel")) return null; // panel header / empty space / top-level row = root
  return undefined;
}

function clearDropHint() {
  const panel = el("layers-panel");
  if (panel) panel.classList.remove("drop-root");
  for (const node of document.querySelectorAll(".group-head.drop-target")) node.classList.remove("drop-target");
}

function updateDropHint(clientX, clientY) {
  clearDropHint();
  const target = dropTarget(clientX, clientY);
  if (target === undefined) return;
  if (target === null) {
    el("layers-panel").classList.add("drop-root"); // "drop to top level" indicator
    return;
  }
  const head = document.querySelector(`.group-head[data-group-id="${target}"]`);
  if (head) head.classList.add("drop-target");
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
  updateDropHint(event.clientX, event.clientY);
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
  const target = dropTarget(event.clientX, event.clientY);
  if (target === undefined) return; // dropped outside the panel: no change
  // Move the whole selection when the dragged row is part of a 2+ selection.
  const ids = state.selectedIds.has(drag.id) && state.selectedIds.size > 1 ? [...state.selectedIds] : [drag.id];
  // Skip a no-op drop (every element already sits in the target).
  const allMatch = ids.every((id) => {
    const element = elementById(id);
    return element && (element.groupId || null) === (target || null);
  });
  if (allMatch) return;
  assignElementsToGroup(ids, target);
}

export function initLayers() {
  hooks.renderLayers = renderLayers;
  // "+ Screen": groups the current selection (same as Ctrl/Cmd+G), or creates an
  // empty default-size screen when nothing is selected.
  const newGroupBtn = el("layers-new-group");
  if (newGroupBtn) newGroupBtn.addEventListener("click", () => createGroupOrDefault("New screen"));
  window.addEventListener("mousemove", onLayerMouseMove);
  window.addEventListener("mouseup", onLayerMouseUp);
}
