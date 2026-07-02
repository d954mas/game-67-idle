// Right-click context menu. Its items depend on the target (an element, a region
// on the selected element, a group label/frame, or empty canvas); every item calls
// an existing action. The menu is positioned at the pointer, clamped to the
// viewport, supports one level of hover submenu ("Move to screen"), and closes on
// click-away or Escape. Pure rendering/input.
import { el, elementById, enterRegionEdit, groupById, groups, refresh, setStatus, state } from "./app.js";
import {
  assignElementsToGroup,
  bringElementForward,
  bringElementToFront,
  createGroupFromSelection,
  deleteElements,
  deleteGroupAction,
  deleteRegion,
  exportElementIds,
  pasteImageBlob,
  renderScreen,
  sendElementBackward,
  sendElementToBack,
  setGroupVisible,
  sliceRegionsFor,
  ungroup,
} from "./actions.js";

let open = false;
let submenu = null;
let submenuTimer = null;

export function closeContextMenu() {
  if (!open) return;
  open = false;
  closeSubmenu();
  const menu = el("context-menu");
  menu.classList.add("hidden");
  menu.replaceChildren();
  document.removeEventListener("mousedown", onDocMouseDown, true);
}

function onDocMouseDown(event) {
  if (!el("context-menu").contains(event.target)) closeContextMenu();
}

// Focus the inspector's first text input so a "Rename" item reuses the existing
// inline field rather than a browser prompt().
function focusInspectorName() {
  requestAnimationFrame(() => {
    const input = document.querySelector("#inspector .insp-input");
    if (input) {
      input.focus();
      input.select();
    }
  });
}

// Open the inline rename editor on the selected region's inspector row (mirrors the
// element "Rename" flow). The region is already selected when its menu opens, so we
// just re-trigger the row's dblclick inline editor.
function focusInspectorRegion() {
  requestAnimationFrame(() => {
    const row = document.querySelector("#inspector .insp-region-row.selected");
    if (row) row.dispatchEvent(new MouseEvent("dblclick", { bubbles: true }));
  });
}

async function pasteFromClipboard() {
  try {
    if (!navigator.clipboard || !navigator.clipboard.read) {
      setStatus("Clipboard paste unavailable; use Ctrl+V.", true);
      return;
    }
    const items = await navigator.clipboard.read();
    for (const item of items) {
      const type = item.types.find((value) => value.startsWith("image/"));
      if (type) {
        await pasteImageBlob(await item.getType(type));
        return;
      }
    }
    setStatus("No image in the clipboard.");
  } catch {
    setStatus("Clipboard read blocked; use Ctrl+V.", true);
  }
}

// The element ids a per-element action applies to: the whole current selection when
// the right-clicked element is part of a 2+ selection, else just that element.
function targetElementIds(elementId) {
  if (state.selectedIds.size > 1 && state.selectedIds.has(elementId)) return [...state.selectedIds];
  return [elementId];
}

// "Move to screen ▸" submenu items: every screen + "None" (top level).
function moveToScreenItems(elementId) {
  const ids = targetElementIds(elementId);
  const items = groups().map((group) => ({
    label: group.name || "Screen",
    onClick: () => assignElementsToGroup(ids, group.id),
  }));
  items.push({ separator: true });
  items.push({ label: "None (top level)", onClick: () => assignElementsToGroup(ids, null) });
  return items;
}

// "Order ▸" submenu: the four Figma z-order moves for one element (same actions as
// the Ctrl+]/[ shortcuts; each no-ops harmlessly when already at that edge).
function orderItems(elementId) {
  return [
    { label: "Bring to front", onClick: () => bringElementToFront(elementId) },
    { label: "Bring forward", onClick: () => bringElementForward(elementId) },
    { label: "Send backward", onClick: () => sendElementBackward(elementId) },
    { label: "Send to back", onClick: () => sendElementToBack(elementId) },
  ];
}

function itemsFor(target) {
  if (target.kind === "element") {
    const element = elementById(target.elementId);
    if (!element) return [];
    // Diet (T0217): a short menu of frequent object actions. Region work is one
    // "Edit regions" entry (double-click is the primary path; detect/slice/add live
    // in the inspector Regions section); rename is double-click, hide is the layers
    // eye dot. Export stays until T0206's inspector panel lands.
    const items = [
      {
        // Enabled for ANY image so a fresh sheet can draw its FIRST region in mode B.
        label: "Edit regions",
        onClick: () => {
          enterRegionEdit(element.id);
          refresh();
        },
      },
      { label: "Export", onClick: () => exportElementIds(targetElementIds(element.id)) },
    ];
    // Right-clicking a selected element keeps the whole multi-selection (see
    // workspace.js onContextMenu): a 2+ selection offers grouping; a single element
    // offers z-order (ambiguous across a multi-selection, so hidden there).
    if (state.selectedIds.size >= 2) {
      items.push({ label: "Group into screen", onClick: () => createGroupFromSelection("New screen") });
    } else {
      items.push({ label: "Order", submenu: orderItems(element.id) });
    }
    if (groups().length || element.groupId) {
      items.push({ label: "Move to screen", submenu: moveToScreenItems(element.id) });
    }
    items.push(
      { separator: true },
      { label: "Delete", danger: true, onClick: () => deleteElements(targetElementIds(element.id)) },
    );
    return items;
  }
  if (target.kind === "region") {
    return [
      { label: "Slice this region", onClick: () => sliceRegionsFor(target.elementId, [target.regionId]) },
      { label: "Rename region", onClick: () => focusInspectorRegion() },
      { separator: true },
      { label: "Delete region", danger: true, onClick: () => deleteRegion(target.elementId, target.regionId) },
    ];
  }
  if (target.kind === "group") {
    const group = groupById(target.groupId);
    if (!group) return [];
    const visible = group.visible !== false;
    return [
      { label: "Render screen", onClick: () => renderScreen(group.id, { scale: 1 }) },
      { label: "Rename", onClick: () => focusInspectorName() },
      { label: visible ? "Hide" : "Show", onClick: () => setGroupVisible(group.id, !visible) },
      { label: "Ungroup", onClick: () => ungroup(group.id) },
      { separator: true },
      { label: "Delete group", danger: true, onClick: () => deleteGroupAction(group.id) },
    ];
  }
  // empty canvas
  return [
    { label: "Add image", onClick: () => el("file-input").click() },
    { label: "Paste", onClick: () => pasteFromClipboard() },
    { label: "Fit", onClick: () => el("zoom-fit").click() },
  ];
}

function closeSubmenu() {
  clearTimeout(submenuTimer);
  if (submenu) {
    submenu.remove();
    submenu = null;
  }
}

function buildButton(item, onActivate) {
  const button = document.createElement("button");
  button.type = "button";
  button.className = "ctx-item";
  if (item.danger) button.classList.add("danger");
  button.textContent = item.label;
  button.disabled = Boolean(item.disabled);
  if (item.submenu) {
    button.classList.add("has-sub");
    const arrow = document.createElement("span");
    arrow.className = "ctx-arrow";
    arrow.textContent = "▸";
    button.appendChild(arrow);
    button.addEventListener("mouseenter", () => openSubmenu(button, item.submenu));
    button.addEventListener("click", (event) => event.stopPropagation());
  } else {
    button.addEventListener("mouseenter", () => {
      clearTimeout(submenuTimer);
      submenuTimer = setTimeout(closeSubmenu, 120);
    });
    button.addEventListener("click", () => {
      if (button.disabled) return;
      closeContextMenu();
      item.onClick();
    });
  }
  return button;
}

function openSubmenu(anchor, items) {
  clearTimeout(submenuTimer);
  if (submenu && submenu.dataset.anchor === anchor.dataset.key) return;
  closeSubmenu();
  const menu = el("context-menu");
  submenu = document.createElement("div");
  submenu.className = "ctx-submenu";
  submenu.dataset.anchor = anchor.dataset.key || "";
  submenu.addEventListener("mouseenter", () => clearTimeout(submenuTimer));
  submenu.addEventListener("mouseleave", () => {
    submenuTimer = setTimeout(closeSubmenu, 120);
  });
  for (const item of items) {
    if (item.separator) {
      const sep = document.createElement("div");
      sep.className = "ctx-sep";
      submenu.appendChild(sep);
      continue;
    }
    submenu.appendChild(buildButton(item));
  }
  menu.appendChild(submenu);
  // Position to the right of the anchor within the (fixed) menu; flip left if it
  // would overflow the viewport.
  let left = anchor.offsetLeft + anchor.offsetWidth + 2;
  submenu.style.left = `${left}px`;
  submenu.style.top = `${anchor.offsetTop}px`;
  const menuRect = menu.getBoundingClientRect();
  const subRect = submenu.getBoundingClientRect();
  if (menuRect.left + left + subRect.width > window.innerWidth - 8) {
    left = anchor.offsetLeft - subRect.width - 2;
    submenu.style.left = `${Math.max(-subRect.width, left)}px`;
  }
}

export function openContextMenu(clientX, clientY, target) {
  const menu = el("context-menu");
  closeSubmenu();
  menu.replaceChildren();
  let key = 0;
  for (const item of itemsFor(target)) {
    if (item.separator) {
      const sep = document.createElement("div");
      sep.className = "ctx-sep";
      menu.appendChild(sep);
      continue;
    }
    const button = buildButton(item);
    button.dataset.key = String((key += 1));
    menu.appendChild(button);
  }
  menu.classList.remove("hidden");
  // Clamp to the viewport.
  const rect = menu.getBoundingClientRect();
  const x = Math.min(clientX, window.innerWidth - rect.width - 8);
  const y = Math.min(clientY, window.innerHeight - rect.height - 8);
  menu.style.left = `${Math.max(4, x)}px`;
  menu.style.top = `${Math.max(4, y)}px`;
  open = true;
  document.addEventListener("mousedown", onDocMouseDown, true);
}

export function initContextMenu() {
  // Escape closing is also handled by the global keyboard handler.
}
