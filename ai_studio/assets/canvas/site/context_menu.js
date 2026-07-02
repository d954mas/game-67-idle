// Right-click context menu. Its items depend on the target (an element, a group
// label/frame, or empty canvas); every item calls an existing action. The menu is
// positioned at the pointer, clamped to the viewport, and closes on click-away or
// Escape. Pure rendering/input.
import { el, elementById, groupById, regionCount, setStatus } from "./app.js";
import {
  addImageFiles,
  deleteElements,
  deleteGroupAction,
  detectRegionsFor,
  exportElementIds,
  pasteImageBlob,
  renderScreen,
  setElementVisible,
  setGroupVisible,
  sliceRegionsFor,
  ungroup,
} from "./actions.js";

let open = false;

export function closeContextMenu() {
  if (!open) return;
  open = false;
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

function itemsFor(target) {
  if (target.kind === "element") {
    const element = elementById(target.elementId);
    if (!element) return [];
    const visible = element.visible !== false;
    return [
      { label: "Detect regions", onClick: () => detectRegionsFor(element.id) },
      { label: "Slice regions", disabled: regionCount(element) === 0, onClick: () => sliceRegionsFor(element.id) },
      { label: "Export", onClick: () => exportElementIds([element.id]) },
      { label: "Rename", onClick: () => focusInspectorName() },
      { label: visible ? "Hide" : "Show", onClick: () => setElementVisible(element.id, !visible) },
      { separator: true },
      { label: "Delete", danger: true, onClick: () => deleteElements([element.id]) },
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

export function openContextMenu(clientX, clientY, target) {
  const menu = el("context-menu");
  menu.replaceChildren();
  for (const item of itemsFor(target)) {
    if (item.separator) {
      const sep = document.createElement("div");
      sep.className = "ctx-sep";
      menu.appendChild(sep);
      continue;
    }
    const button = document.createElement("button");
    button.type = "button";
    button.className = "ctx-item";
    if (item.danger) button.classList.add("danger");
    button.textContent = item.label;
    button.disabled = Boolean(item.disabled);
    button.addEventListener("click", () => {
      closeContextMenu();
      item.onClick();
    });
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
