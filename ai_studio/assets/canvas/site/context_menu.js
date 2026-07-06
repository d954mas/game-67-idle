// Right-click context menu. Its items depend on the target (an element, a region
// on the selected element, a group label/frame, empty canvas, or the layers-panel
// background); every item calls an existing action. The menu is positioned at the
// pointer, clamped to the viewport, supports one level of hover submenu ("Order"),
// and closes on click-away or Escape. Pure rendering/input. Kept deliberately
// SHORT (lead's menu diet): anything with an inspector/layers home stays out.
import { el, elementById, enterRegionEdit, groupById, groups, refresh, setStatus, state } from "./app.js";
import {
  addNoteAt,
  bringNodeForward,
  bringNodeToFront,
  createAnimCardAction,
  createGroupFromSelection,
  createGroupInside,
  createGroupOrDefault,
  createRecipeCardAction,
  createStyleCardAction,
  deleteElements,
  deleteGroupAction,
  deleteRegion,
  pasteImageBlob,
  reorderNodesBy,
  selectedNodeIds,
  sendNodeBackward,
  sendNodeToBack,
  sliceRegionsFor,
  toggleElementFlip,
  ungroup,
} from "./actions.js";
import { isNodeTransformed } from "../tree.mjs";

// R7 (T0232 increment 3a): the same grayed-out reason the inspector shows on its
// Detect/Slice/Alpha controls, here as a disabled-button title (mirrors the ops-layer
// refusal message).
const TRANSFORM_GUARD_TITLE = "Rotated/flipped — reset rotation/flip to edit regions or slice.";

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

// "Order ▸" submenu: the four Figma z-order moves (same actions as the Ctrl+]/[ shortcuts,
// computed over MERGED same-scope siblings; each no-ops harmlessly at the edge). Selection-
// aware: when `nodeId` is part of a 2+ selection the moves act on the whole selection as
// ONE block (reorderNodes, one undo); otherwise they nudge the single node.
function orderItems(nodeId) {
  const ids = selectedNodeIds();
  const block = ids.length >= 2 && ids.includes(nodeId);
  if (block) {
    return [
      { label: "Bring to front", onClick: () => reorderNodesBy("front") },
      { label: "Bring forward", onClick: () => reorderNodesBy("forward") },
      { label: "Send backward", onClick: () => reorderNodesBy("backward") },
      { label: "Send to back", onClick: () => reorderNodesBy("back") },
    ];
  }
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
    const transformed = isNodeTransformed(element);
    // Diet (T0217): a short menu of frequent object actions. Region work is one
    // "Edit regions" entry (double-click is the primary path; detect/slice/add live
    // in the inspector Regions section); rename is double-click, hide is the layers
    // eye dot. Export moved to the inspector Export section (T0206), so it is no
    // longer a context-menu item.
    const items = [
      {
        // Enabled for ANY image so a fresh sheet can draw its FIRST region in mode B.
        // R7 (T0232 increment 3a): disabled on a rotated/flipped element — regions read
        // untransformed source pixels (mirrors workspace.js's own dblclick guard).
        label: "Edit regions",
        disabled: transformed,
        title: transformed ? TRANSFORM_GUARD_TITLE : undefined,
        onClick: () => {
          enterRegionEdit(element.id);
          refresh();
        },
      },
    ];
    // Flip (T0232 increment 3a) — image-only, additive boolean flags; toggling either
    // is one patchElement (same path the inspector's Flip H/Flip V buttons use).
    if (element.type === "image") {
      items.push(
        { label: "Flip horizontal", onClick: () => toggleElementFlip(element.id, "h") },
        { label: "Flip vertical", onClick: () => toggleElementFlip(element.id, "v") },
      );
      // T0265 F6: promote this image into a new animation card as its first keyframe (ONE POST
      // with memberId — the server fits the box + moves the image in as one journal entry).
      // Cheap client courtesy: hide the item when the image is ALREADY a member of a card
      // (recipe/style/anim) so we don't offer an action that would only toast a refusal — the
      // server guard (which also refuses a claimed style ref) stays the law.
      const parentCard = element.groupId ? groupById(element.groupId) : null;
      if (!(parentCard && (parentCard.recipe || parentCard.style || parentCard.anim))) {
        items.push({ label: "Animate this image", onClick: () => createAnimCardAction(null, element) });
      }
    }
    // Right-clicking a selected element keeps the whole multi-selection (see
    // workspace.js onContextMenu): a 2+ selection also offers grouping, and Order acts on
    // the whole selection as one block; a single element orders just itself.
    if (state.selectedIds.size >= 2) {
      items.push({ label: "Group selection", onClick: () => createGroupFromSelection("New group") });
    }
    items.push({ label: "Order", submenu: orderItems(element.id) });
    // "Move to group" removed everywhere (lead 2026-07-02: with many groups the
    // submenu is unusable) — reparenting lives in the layers drag.
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
    // Diet (lead 2026-07-02, second pass): render/fit/background/clip live in the
    // inspector, rename is dblclick, hide is the layers eye, reparent is layers drag.
    // The menu keeps only what has no better home.
    return [
      { label: "Order", submenu: orderItems(group.id) },
      { label: "Create group inside", onClick: () => createGroupInside(group.id) },
      { label: "Ungroup", onClick: () => ungroup(group.id) },
      { separator: true },
      copyIdItemFor(target),
      { label: "Delete group", danger: true, onClick: () => deleteGroupAction(group.id) },
    ];
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
    // T0268: mint a sticky note at the click point (double-click it to write text).
    { label: "New note", onClick: () => addNoteAt(target.world) },
    { label: "Create group", onClick: () => createGroupOrDefault("New group") },
    // T0239 increment 1: mint a recipe card at the click point (a group with an
    // additive `recipe` blob — no generation yet, that lands in increment 2).
    { label: "New recipe card", onClick: () => createRecipeCardAction(target.world) },
    // T0239 increment 3: mint a style card at the click point (a group with an
    // additive `style` blob — name + style prompt + ONE ref + examples).
    { label: "New style card", onClick: () => createStyleCardAction(target.world) },
    // T0265: mint an animation card at the click point (a group with an additive `anim`
    // blob — the video-route flipbook workflow, design §1.1).
    { label: "New animation card", onClick: () => createAnimCardAction(target.world) },
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
  if (item.title) button.title = item.title;
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
