// Layers panel: a flat, group-aware list. Ungrouped elements sit at the top
// level; each group is a collapsible section (eye toggle + inline-rename name)
// with its member elements indented beneath. Every row has a 24px thumbnail, an
// inline-rename name, an eye toggle, and a region-count badge. Clicking a row
// selects on the canvas (selection syncs both ways through refresh()). The
// header's "+ Screen" button is the discoverable counterpart to Ctrl/Cmd+G. Pure
// rendering/input — all mutations go through actions.
import {
  el,
  fileUrl,
  groups,
  hooks,
  memberElements,
  regionCount,
  refresh,
  selectOnly,
  state,
  toggleSelect,
  ungroupedElements,
} from "./app.js";
import { createGroupOrDefault, renameElement, setElementVisible, renameGroup, setGroupVisible } from "./actions.js";
import { inlineEdit } from "./inline.js";

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
  if (indented) row.classList.add("indented");
  if (state.selectedIds.has(element.id)) row.classList.add("selected");

  const thumb = document.createElement("img");
  thumb.className = "thumb";
  thumb.src = fileUrl(element);
  thumb.alt = "";
  row.appendChild(thumb);

  const name = document.createElement("span");
  name.className = "layer-name";
  name.textContent = element.name || element.id;
  name.title = element.name || element.id;
  row.appendChild(name);
  // Rename on double-click anywhere on the row (not just the narrow name span) —
  // the eye/other buttons are excluded so toggling fast doesn't open an editor.
  row.addEventListener("dblclick", (event) => {
    if (event.target.closest("button")) return;
    event.stopPropagation();
    inlineEdit(name, element.name || "", (next) => renameElement(element.id, next));
  });

  const count = regionCount(element);
  if (count > 0) {
    const badge = document.createElement("span");
    badge.className = "badge";
    badge.textContent = `${count}r`;
    badge.title = `${count} region(s)`;
    row.appendChild(badge);
  }

  row.appendChild(eyeButton(element.visible !== false, () => setElementVisible(element.id, element.visible === false)));

  row.addEventListener("click", (event) => {
    if (event.shiftKey || event.ctrlKey || event.metaKey) toggleSelect(element.id);
    else selectOnly(element.id);
    refresh();
  });
  return row;
}

function groupSection(group) {
  const wrap = document.createElement("div");
  wrap.className = "layer-group";
  const collapsed = state.collapsedGroups.has(group.id);

  const head = document.createElement("div");
  head.className = "group-head";
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
    refresh();
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
  // An open inline rename must survive selection-driven re-renders: skip the
  // rebuild while the editor is up — the commit's reload re-renders anyway.
  if (list.querySelector(".inline-input")) return;
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

export function initLayers() {
  hooks.renderLayers = renderLayers;
  // "+ Screen": groups the current selection (same as Ctrl/Cmd+G), or creates an
  // empty default-size screen when nothing is selected.
  const newGroupBtn = el("layers-new-group");
  if (newGroupBtn) newGroupBtn.addEventListener("click", () => createGroupOrDefault("New screen"));
}
