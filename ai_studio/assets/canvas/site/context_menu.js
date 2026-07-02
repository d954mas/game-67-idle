// Right-click context menu. Its items depend on the target (an element, a region
// on the selected element, a group label/frame, empty canvas, or the layers-panel
// background); every item calls an existing action. The menu is positioned at the
// pointer, clamped to the viewport, supports one level of hover submenu ("Move to
// group"), and closes on click-away or Escape. Pure rendering/input.
import { el, elementById, enterRegionEdit, groupById, groups, refresh, setStatus, state } from "./app.js";
import { descendantsOf, orderedChildren } from "../tree.mjs";
import {
  assignElementsToGroup,
  bringNodeForward,
  bringNodeToFront,
  createGroupFromSelection,
  createGroupOrDefault,
  deleteElements,
  deleteGroupAction,
  deleteRegion,
  fitGroupAction,
  pasteImageBlob,
  renderScreen,
  reparentGroupTo,
  sendNodeBackward,
  sendNodeToBack,
  setGroupBackground,
  setGroupClip,
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

// ---- Copy ID -------------------------------------------------------------------
// A paste-into-agent-chat reference: the canvas://<project>/<kind>/<id> part is
// canonical (bare ids the API/CLI take verbatim), the tail names the project and
// object so a human can tell references apart at a glance.
function projectRefBase() {
  const project = state.project;
  return project ? { uri: `canvas://${project.id}`, title: project.title || project.id } : null;
}

function elementRef(elementId) {
  const base = projectRefBase();
  const element = elementById(elementId);
  if (!base || !element) return null;
  return `${base.uri}/element/${element.id} — project "${base.title}", element "${element.name || element.id}"`;
}

async function copyRefs(refs) {
  const text = refs.filter(Boolean).join("\n");
  if (!text) return;
  try {
    await navigator.clipboard.writeText(text);
    setStatus(refs.length > 1 ? `Copied ${refs.length} ids.` : `Copied: ${text}`);
  } catch {
    setStatus("Clipboard write blocked by the browser.", true);
  }
}

function copyIdItemFor(target) {
  if (target.kind === "element") {
    const ids = targetElementIds(target.elementId);
    return {
      label: ids.length > 1 ? `Copy IDs (${ids.length})` : "Copy ID",
      onClick: () => copyRefs(ids.map((id) => elementRef(id))),
    };
  }
  if (target.kind === "region") {
    return {
      label: "Copy ID",
      onClick: () => {
        const base = projectRefBase();
        const element = elementById(target.elementId);
        const region = (element?.regions || []).find((item) => item.id === target.regionId);
        if (!base || !element || !region) return;
        copyRefs([
          `${base.uri}/element/${element.id}/region/${region.id} — project "${base.title}", element "${element.name || element.id}", region "${region.name || region.id}"`,
        ]);
      },
    };
  }
  if (target.kind === "group") {
    return {
      label: "Copy ID",
      onClick: () => {
        const base = projectRefBase();
        const group = groupById(target.groupId);
        if (!base || !group) return;
        copyRefs([`${base.uri}/group/${group.id} — project "${base.title}", group "${group.name || "Group"}"`]);
      },
    };
  }
  // Empty canvas / layers background: the project itself.
  return {
    label: "Copy project ID",
    onClick: () => {
      const base = projectRefBase();
      if (base) copyRefs([`${base.uri} — project "${base.title}"`]);
    },
  };
}

// Every group in scene-tree order with its nesting depth, optionally EXCLUDING a group's
// own subtree (invalid reparent targets). Backs the indented "Move to group" submenus.
function groupTree(excludeSubtreeOf) {
  const exclude = excludeSubtreeOf
    ? new Set([excludeSubtreeOf, ...descendantsOf(state.project, excludeSubtreeOf).groups.map((g) => g.id)])
    : new Set();
  const out = [];
  const walk = (scopeId, depth) => {
    for (const child of orderedChildren(state.project, scopeId)) {
      if (child.kind !== "group" || exclude.has(child.id)) continue;
      out.push({ group: child.ref, depth });
      walk(child.id, depth + 1);
    }
  };
  walk(null, 0);
  return out;
}

// Indent a submenu label by nesting depth with non-breaking spaces (plain spaces
// collapse in the menu button).
function indentLabel(name, depth) {
  return `${"  ".repeat(depth)}${name || "Group"}`;
}

// "Move to group ▸" submenu items for an ELEMENT: every group (nested indented) + "None".
function moveToScreenItems(elementId) {
  const ids = targetElementIds(elementId);
  const items = groupTree(null).map(({ group, depth }) => ({
    label: indentLabel(group.name, depth),
    onClick: () => assignElementsToGroup(ids, group.id),
  }));
  items.push({ separator: true });
  items.push({ label: "None (top level)", onClick: () => assignElementsToGroup(ids, null) });
  return items;
}

// "Move to group ▸" submenu items for a GROUP (reparent): every group NOT in its own
// subtree (cycle guard), nested indented, + "None" (top level).
function moveGroupItems(groupId) {
  const items = groupTree(groupId).map(({ group, depth }) => ({
    label: indentLabel(group.name, depth),
    onClick: () => reparentGroupTo(groupId, group.id, undefined),
  }));
  items.push({ separator: true });
  items.push({ label: "None (top level)", onClick: () => reparentGroupTo(groupId, null, undefined) });
  return items;
}

// "Background ▸" quick submenu for a group: None clears the fill; Pick color opens a
// native color input and sets a solid background (both journaled via setGroupBackground).
function backgroundItems(groupId) {
  return [
    { label: "None", onClick: () => setGroupBackground(groupId, null) },
    { label: "Pick color…", onClick: () => pickGroupColor(groupId) },
  ];
}

// A throwaway native <input type="color"> so the menu can set a group background
// without depending on the inspector DOM. Seeds from the current fill when present.
function pickGroupColor(groupId) {
  const group = groupById(groupId);
  const input = document.createElement("input");
  input.type = "color";
  input.value = group && group.background && group.background.color ? group.background.color : "#1a1f2b";
  input.style.position = "fixed";
  input.style.left = "-9999px";
  document.body.appendChild(input);
  input.addEventListener("change", () => {
    setGroupBackground(groupId, { type: "color", color: input.value });
    input.remove();
  });
  input.addEventListener("blur", () => setTimeout(() => input.remove(), 0));
  input.click();
}

// "Order ▸" submenu: the four Figma z-order moves for one NODE — an element OR a group
// (same actions as the Ctrl+]/[ shortcuts, computed over MERGED same-scope siblings; each
// no-ops harmlessly when already at that edge).
function orderItems(nodeId) {
  return [
    { label: "Bring to front", onClick: () => bringNodeToFront(nodeId) },
    { label: "Bring forward", onClick: () => bringNodeForward(nodeId) },
    { label: "Send backward", onClick: () => sendNodeBackward(nodeId) },
    { label: "Send to back", onClick: () => sendNodeToBack(nodeId) },
  ];
}

function itemsFor(target) {
  if (target.kind === "element") {
    const element = elementById(target.elementId);
    if (!element) return [];
    // Diet (T0217): a short menu of frequent object actions. Region work is one
    // "Edit regions" entry (double-click is the primary path; detect/slice/add live
    // in the inspector Regions section); rename is double-click, hide is the layers
    // eye dot. Export moved to the inspector Export section (T0206), so it is no
    // longer a context-menu item.
    const items = [
      {
        // Enabled for ANY image so a fresh sheet can draw its FIRST region in mode B.
        label: "Edit regions",
        onClick: () => {
          enterRegionEdit(element.id);
          refresh();
        },
      },
    ];
    // Right-clicking a selected element keeps the whole multi-selection (see
    // workspace.js onContextMenu): a 2+ selection offers grouping; a single element
    // offers z-order (ambiguous across a multi-selection, so hidden there).
    if (state.selectedIds.size >= 2) {
      items.push({ label: "Group selection", onClick: () => createGroupFromSelection("New group") });
    } else {
      items.push({ label: "Order", submenu: orderItems(element.id) });
    }
    if (groups().length || element.groupId) {
      items.push({ label: "Move to group", submenu: moveToScreenItems(element.id) });
    }
    items.push(
      { separator: true },
      copyIdItemFor(target),
      { label: "Delete", danger: true, onClick: () => deleteElements(targetElementIds(element.id)) },
    );
    return items;
  }
  if (target.kind === "region") {
    // Count-aware slice (lead): a right-click inside a multi-selection slices the
    // whole selection; otherwise just the clicked region.
    const selected = state.regionEditId === target.elementId ? [...state.selectedRegionIds] : [];
    const multi = selected.length > 1 && selected.includes(target.regionId);
    const sliceIds = multi ? selected : [target.regionId];
    return [
      {
        label: multi ? `Slice selected (${selected.length})` : "Slice region",
        onClick: () => sliceRegionsFor(target.elementId, sliceIds),
      },
      { label: "Rename region", onClick: () => focusInspectorRegion() },
      { separator: true },
      copyIdItemFor(target),
      { label: "Delete region", danger: true, onClick: () => deleteRegion(target.elementId, target.regionId) },
    ];
  }
  if (target.kind === "group") {
    const group = groupById(target.groupId);
    if (!group) return [];
    const visible = group.visible !== false;
    const items = [
      { label: "Render group", onClick: () => renderScreen(group.id, { scale: 1 }) },
      // Resize the frame to fit its content (one journaled fitGroup op; children never
      // move). Always enabled — an empty group surfaces the op's loud error as a toast.
      { label: "Fit to content", onClick: () => fitGroupAction(group.id) },
      { label: "Order", submenu: orderItems(group.id) },
      { label: "Background", submenu: backgroundItems(group.id) },
      // Checkmark via label prefix (the menu has no checkbox item type; mirrors the
      // Hide/Show label toggle). Toggles the Figma frame clip via patchGroup({clip}).
      { label: `${group.clip === true ? "✓ " : ""}Clip content`, onClick: () => setGroupClip(group.id, group.clip !== true) },
    ];
    // Nest this group under another (or out to top level) when there is a valid target.
    if (groupTree(group.id).length || group.parentId) {
      items.push({ label: "Move to group", submenu: moveGroupItems(group.id) });
    }
    items.push(
      { label: "Rename", onClick: () => focusInspectorName() },
      { label: visible ? "Hide" : "Show", onClick: () => setGroupVisible(group.id, !visible) },
      { label: "Ungroup", onClick: () => ungroup(group.id) },
      { separator: true },
      copyIdItemFor(target),
      { label: "Delete group", danger: true, onClick: () => deleteGroupAction(group.id) },
    );
    return items;
  }
  if (target.kind === "layers-empty") {
    // Layers-panel background: group creation lives here (the "+ Screen" header
    // button was removed — lead unified the naming on "group").
    return [
      { label: "Create group", onClick: () => createGroupOrDefault("New group") },
      copyIdItemFor(target),
    ];
  }
  // empty canvas
  return [
    { label: "Add image", onClick: () => el("file-input").click() },
    { label: "Paste", onClick: () => pasteFromClipboard() },
    { label: "Create group", onClick: () => createGroupOrDefault("New group") },
    { label: "Fit", onClick: () => el("zoom-fit").click() },
    { separator: true },
    copyIdItemFor(target),
  ];
}

function closeSubmenu() {
  clearTimeout(submenuTimer);
  if (submenu) {
    submenu.remove();
    submenu = null;
  }
}

// Submenu hover timing. A diagonal pointer path from a has-sub item to its open
// submenu momentarily clips a NEIGHBORING item, so: a click on a has-sub item opens
// its submenu immediately (the reliable path); hovering a DIFFERENT has-sub item
// while one is open swaps only after a short beat (clipping it in passing does not
// yank the submenu away); a plain TOP-LEVEL item closes the submenu only after a
// longer beat — long enough for the pointer to reach the submenu. A submenu's own
// items never arm a close; the submenu container owns its open/close lifecycle.
const SUBMENU_SWAP_MS = 150;
const SUBMENU_CLOSE_MS = 300;

function buildButton(item, inSubmenu = false) {
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
    button.addEventListener("mouseenter", () => {
      clearTimeout(submenuTimer);
      if (submenu && submenu.dataset.anchor === (button.dataset.key || "")) return; // already open
      if (submenu) submenuTimer = setTimeout(() => openSubmenu(button, item.submenu), SUBMENU_SWAP_MS);
      else openSubmenu(button, item.submenu); // nothing open yet — open immediately
    });
    button.addEventListener("click", (event) => {
      event.stopPropagation();
      clearTimeout(submenuTimer);
      openSubmenu(button, item.submenu); // reliable path: pin it open on click
    });
  } else {
    button.addEventListener("mouseenter", () => {
      clearTimeout(submenuTimer);
      // Only a top-level plain item closes an open submenu; a submenu's own items
      // must not arm a close (that is what made "Send to back" unreachable).
      if (!inSubmenu) submenuTimer = setTimeout(closeSubmenu, SUBMENU_CLOSE_MS);
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
    submenuTimer = setTimeout(closeSubmenu, SUBMENU_CLOSE_MS);
  });
  for (const item of items) {
    if (item.separator) {
      const sep = document.createElement("div");
      sep.className = "ctx-sep";
      submenu.appendChild(sep);
      continue;
    }
    submenu.appendChild(buildButton(item, true));
  }
  menu.appendChild(submenu);
  // Position to the right of the anchor within the (fixed) menu; flip left if it
  // would overflow the viewport. The submenu OVERLAPS the anchor by a few px (no
  // gap) so the pointer never has to cross empty space to reach it.
  let left = anchor.offsetLeft + anchor.offsetWidth - 4;
  submenu.style.left = `${left}px`;
  submenu.style.top = `${anchor.offsetTop}px`;
  const menuRect = menu.getBoundingClientRect();
  const subRect = submenu.getBoundingClientRect();
  if (menuRect.left + left + subRect.width > window.innerWidth - 8) {
    left = anchor.offsetLeft - subRect.width + 4;
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
