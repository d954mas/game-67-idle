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
  elements,
  fileUrl,
  groupById,
  groups,
  hooks,
  memberElements,
  rangeSelectIds,
  refresh,
  selectGroupOnly,
  selectOnly,
  state,
  syncPrimaryGroup,
  toggleSelect,
  ungroupedElements,
} from "./app.js";
import { ancestorsOf, descendantsOf, nodeScope, orderedChildren } from "../tree.mjs";
import {
  assignElementsToGroup,
  renameElement,
  renameGroup,
  reorderNodeTo,
  reparentGroupTo,
  setElementVisible,
  setGroupVisible,
} from "./actions.js";
import { inlineEdit } from "./inline.js";
import { openContextMenu } from "./context_menu.js";

// Pointer-based layer-row drag (reparent OR reorder; kept deliberately simple). `kind`
// is "element" (reorder among siblings + reparent onto a group / root) or "group"
// (reorder among siblings, nest INTO a group's header middle, or reparent across scopes
// — the group's own subtree is an inert target so a cycle can't be dropped).
let layerDrag = null; // { id, kind, name, rowEl, startX, startY, active }

// Reused thumbnail <img> nodes, keyed by element id (with the src it was built for).
// A full layers rebuild (replaceChildren) detaches the old rows but we re-append the
// SAME img node, so its src is never reset and the browser neither re-downloads nor
// re-decodes an unrelated thumbnail on every op. Keyed by id (not src) so two elements
// sharing one content-addressed file each get their own node; the src guard rebuilds
// only when an element's image actually changes. serveFile marks files/ immutable, so
// even the first paint is cache-friendly.
const thumbCache = new Map(); // element.id -> { img, src }

function thumbFor(element) {
  let entry = thumbCache.get(element.id);
  if (!entry || entry.src !== element.src) {
    const img = document.createElement("img");
    img.className = "thumb";
    img.alt = "";
    img.draggable = false; // never start a native image drag when dragging a row
    img.src = fileUrl(element);
    entry = { img, src: element.src };
    thumbCache.set(element.id, entry);
  }
  return entry.img;
}

// Drop cached thumbnails for elements no longer in the project (keeps the map bounded
// across deletes / project switches; ids are globally unique so this is exact).
function pruneThumbCache() {
  const live = new Set([...ungroupedElements(), ...groups().flatMap((g) => memberElements(g.id))].map((e) => e.id));
  for (const id of thumbCache.keys()) if (!live.has(id)) thumbCache.delete(id);
}

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

// Anchor for Shift range-selection: the last plainly (or Ctrl-) clicked row.
let selectAnchorId = null;

// Figma-style Shift+click: select the contiguous run of VISIBLE rows between the
// anchor and the clicked row, in the panel's visual front-at-top order (collapsed
// group members aren't rendered, so they never silently join a range). Without a
// usable anchor it degrades to a plain click. The range MATH is the shared
// rangeSelectIds helper (T0224 item 5 — one helper, also used by the inspector Regions).
function selectRange(targetId) {
  const rows = el("layers-list")?.querySelectorAll(".layer-row") || [];
  const order = [...rows].map((row) => row.dataset.elementId);
  const ids = rangeSelectIds(order, selectAnchorId, targetId);
  if (!ids) {
    selectOnly(targetId);
    selectAnchorId = targetId;
    return;
  }
  state.selectedGroupId = null;
  state.selectedGroupIds = new Set(); // plural set too — stale ids would leak into node-batch actions
  state.selectedRegionIds = new Set();
  state.regionEditId = null;
  state.selectedIds = new Set(ids);
}

// Layers-row content preview for a text element (T0231): newlines collapsed to spaces,
// trimmed, truncated to ~24 chars with an ellipsis. Empty/whitespace-only content yields
// "" so the caller skips the preview entirely (no bare quotes on a still-blank element).
const TEXT_PREVIEW_MAX = 24;
function textPreview(content) {
  const flat = String(content || "")
    .replace(/\s+/g, " ")
    .trim();
  if (!flat) return "";
  return flat.length > TEXT_PREVIEW_MAX ? `${flat.slice(0, TEXT_PREVIEW_MAX)}…` : flat;
}

function elementRow(element, depth) {
  const row = document.createElement("div");
  row.className = "layer-row";
  row.dataset.elementId = element.id;
  row.dataset.groupId = element.groupId || "";
  if (depth > 0) row.classList.add("indented");
  // --depth drives the CSS indent guides (thin vertical level lines, VS Code style —
  // T0224 item 8b): one guide per ancestor level, aligned with the 8px/level step.
  row.style.setProperty("--depth", String(depth));
  if (state.selectedIds.has(element.id)) row.classList.add("selected");

  // One caret-width spacer per nesting level (indent under the ancestor group heads);
  // top-level rows (depth 0) sit flush left — a spacer there reads as fake membership.
  for (let i = 0; i < depth; i += 1) {
    const spacer = document.createElement("span");
    spacer.className = "caret empty";
    row.appendChild(spacer);
  }

  if (element.type === "text") {
    // Text has no image file — show a "T" glyph placeholder instead of an <img>.
    const glyph = document.createElement("span");
    glyph.className = "thumb thumb-text";
    glyph.textContent = "T";
    glyph.setAttribute("aria-hidden", "true");
    row.appendChild(glyph);
  } else if (element.type === "note") {
    // A note has no image file either — a distinct "▤" glyph marks it as a note card (T0268).
    const glyph = document.createElement("span");
    glyph.className = "thumb thumb-note";
    glyph.textContent = "▤";
    glyph.setAttribute("aria-hidden", "true");
    row.appendChild(glyph);
  } else {
    row.appendChild(thumbFor(element)); // reused node — no re-download / re-decode on rebuild
  }

  const name = document.createElement("span");
  name.className = "layer-name";
  name.textContent = element.name || element.id;
  name.title = element.name || element.id;
  // Text rows (T0231): show the name PLUS a dimmed truncated content preview, visually
  // distinct so "Text" (the default name) doesn't read as the content. Both spans sit in
  // a shared flexible label so each ellipsizes on its own; a non-text or empty-content
  // element keeps the plain unwrapped name (unchanged from before).
  const preview = element.type === "text" || element.type === "note" ? textPreview(element.content) : "";
  if (preview) {
    const label = document.createElement("span");
    label.className = "layer-label";
    label.appendChild(name);
    const previewEl = document.createElement("span");
    previewEl.className = "layer-text-preview";
    previewEl.textContent = `"${preview}"`;
    previewEl.title = element.content || "";
    label.appendChild(previewEl);
    row.appendChild(label);
  } else {
    row.appendChild(name);
  }
  // Rename on double-click anywhere on the row (not the eye button). inlineEdit replaces
  // ONLY the `name` span's children, so it works the same whether `name` sits directly in
  // the row or inside the text-preview label wrapper above.
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
      kind: "element",
      name: element.name || element.id,
      rowEl: row,
      startX: event.clientX,
      startY: event.clientY,
      active: false,
    };
  });
  row.addEventListener("click", (event) => {
    // Figma-style: Shift = contiguous range from the last plain-clicked row,
    // Ctrl/Cmd = toggle one row, plain click = select only (and set the anchor).
    if (event.shiftKey) {
      selectRange(element.id);
    } else if (event.ctrlKey || event.metaKey) {
      toggleSelect(element.id);
      selectAnchorId = element.id;
    } else {
      selectOnly(element.id);
      selectAnchorId = element.id;
    }
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

function groupSection(group, depth) {
  const wrap = document.createElement("div");
  wrap.className = "layer-group";
  // Groups collapse by DEFAULT now (Figma reveal, T0224 item 8a): a group is expanded only
  // when its id is in state.expandedGroups (the selection's ancestor path is auto-added).
  const collapsed = !state.expandedGroups.has(group.id);

  const head = document.createElement("div");
  head.className = "group-head";
  head.dataset.groupId = group.id;
  if (depth > 0) head.classList.add("indented");
  head.style.setProperty("--depth", String(depth)); // CSS indent guides (item 8b)
  // Highlight keys on the PLURAL set: with 2+ groups selected the singular primary
  // is null (syncPrimaryGroup), and the rows looked unselected — "не работает".
  if (state.selectedGroupIds.has(group.id)) head.classList.add("selected");

  // Indent a nested group head one spacer per ancestor level (its own caret button
  // provides the baseline indent for its members).
  for (let i = 0; i < depth; i += 1) {
    const spacer = document.createElement("span");
    spacer.className = "caret empty";
    head.appendChild(spacer);
  }

  const caret = document.createElement("button");
  caret.className = "caret";
  caret.type = "button";
  caret.textContent = collapsed ? "▸" : "▾"; // ▸ / ▾
  caret.addEventListener("mousedown", (event) => event.stopPropagation());
  caret.addEventListener("click", (event) => {
    event.stopPropagation();
    if (collapsed) state.expandedGroups.add(group.id);
    else state.expandedGroups.delete(group.id);
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

  // Card-type chip (T0239-3 follow-up, lead: a recipe/style card row was indistinguishable
  // from a plain group in the tree). Same accents as the canvas chrome; the two blobs are
  // mutually exclusive by construction, so a plain either/or never shows two chips.
  if (group.recipe || group.style) {
    const chip = document.createElement("span");
    chip.className = `layer-card-chip ${group.recipe ? "recipe" : "style"}`;
    chip.textContent = group.recipe ? "Recipe" : "Style";
    head.appendChild(chip);
  }

  const members = memberElements(group.id);
  const count = document.createElement("span");
  count.className = "badge";
  count.textContent = `${members.length}`;
  head.appendChild(count);

  head.appendChild(eyeButton(group.visible !== false, () => setGroupVisible(group.id, group.visible === false)));

  // A group head is a drag source too: dragging it reorders the group among its siblings,
  // nests it into another group (drop on a header's middle), or reparents it across
  // scopes. Mirrors the element-row threshold; the eye/caret buttons stopPropagation, so
  // this never fires for them.
  head.addEventListener("mousedown", (event) => {
    if (event.button !== 0 || event.target.closest("button")) return;
    layerDrag = {
      id: group.id,
      kind: "group",
      name: group.name || "Group",
      rowEl: head,
      startX: event.clientX,
      startY: event.clientY,
      active: false,
    };
  });
  // selectGroupOnly (NOT hand-rolled state writes): it also fills the plural
  // selectedGroupIds set, which the Delete key and every node-batch action key on —
  // a hand-rolled write here left the set empty, so Delete from layers did nothing.
  // Ctrl/Shift+click toggles the group in the multi selection — mixed element+group
  // selections are legal (canvas Shift-click parity; range-select across mixed
  // element+group rows is not supported yet).
  head.addEventListener("click", (event) => {
    if (event.ctrlKey || event.metaKey || event.shiftKey) {
      if (state.selectedGroupIds.has(group.id)) state.selectedGroupIds.delete(group.id);
      else state.selectedGroupIds.add(group.id);
      state.selectedRegionIds = new Set();
      state.regionEditId = null;
      syncPrimaryGroup();
    } else {
      selectGroupOnly(group.id);
    }
    refresh();
  });
  head.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    selectGroupOnly(group.id);
    refresh();
    openContextMenu(event.clientX, event.clientY, { kind: "group", groupId: group.id });
  });
  wrap.appendChild(head);

  if (!collapsed) {
    // Recurse into the group's children (elements AND subgroups), one level deeper.
    renderScope(group.id, wrap, depth + 1);
  }
  return wrap;
}

// Render one scope's children into `container`, FRONT-at-top (reverse of the
// back-to-front computed order): a group row expands into its own subtree; an element
// is a leaf row indented by its depth. Recursive — the whole hierarchy from any scope.
function renderScope(scopeId, container, depth) {
  for (const child of [...orderedChildren(state.project, scopeId)].reverse()) {
    if (child.kind === "group") container.appendChild(groupSection(child.ref, depth));
    else container.appendChild(elementRow(child.ref, depth));
  }
}

// ---- structure-signature guarded render --------------------------------------

let lastLayersSig = null;

// Everything that changes which rows exist and their text/badges/visibility —
// NOT the selection (that is applied as a lightweight overlay so selection clicks
// never rebuild the DOM out from under a pending double-click).
function layersSignature() {
  const parts = [];
  const walk = (scopeId, depth) => {
    for (const child of orderedChildren(state.project, scopeId)) {
      if (child.kind === "group") {
        const g = child.ref;
        const collapsed = !state.expandedGroups.has(g.id);
        parts.push(`g:${depth}:${g.id}:${g.name || ""}:${g.visible !== false ? 1 : 0}:${collapsed ? 1 : 0}`);
        if (!collapsed) walk(g.id, depth + 1);
      } else {
        const e = child.ref;
        // Text content rides in the signature too (T0231): the layers row shows a content
        // preview for text elements, so an edit that only changes `content` (name/visible/
        // regions unchanged) must still trigger a rebuild.
        parts.push(
          `e:${depth}:${e.id}:${e.name || ""}:${e.visible !== false ? 1 : 0}:${(e.regions || []).length}:${e.type === "text" || e.type === "note" ? e.content || "" : ""}`,
        );
      }
    }
  };
  walk(null, 0);
  return parts.join("\n");
}

function applyLayersSelection() {
  const list = el("layers-list");
  if (!list) return;
  for (const row of list.querySelectorAll(".layer-row[data-element-id]")) {
    row.classList.toggle("selected", state.selectedIds.has(row.dataset.elementId));
  }
  for (const head of list.querySelectorAll(".group-head[data-group-id]")) {
    head.classList.toggle("selected", state.selectedGroupIds.has(head.dataset.groupId));
  }
}

// Figma reveal (T0224 item 8a): when the SELECTION changes, auto-expand the ancestor group
// path of every selected node so the selection is visible — without disturbing the user's
// manual expand/collapse otherwise (we only add on a genuine selection change, tracked by
// lastRevealKey, so re-collapsing a group with the selection unchanged sticks). A selected
// group reveals its OWN row (we expand its ancestors, not the group itself), so it shows
// collapsed until the user opens it.
let lastRevealKey = null;
function revealSelectionPath() {
  const selIds = [
    ...state.selectedIds,
    ...(state.selectedGroupId ? [state.selectedGroupId] : []),
    ...state.selectedGroupIds,
  ];
  const key = [...new Set(selIds)].sort().join(",");
  if (key === lastRevealKey) return;
  lastRevealKey = key;
  for (const id of selIds) {
    const node = elementById(id) || groupById(id);
    if (!node) continue;
    for (const ancestor of ancestorsOf(state.project, node)) state.expandedGroups.add(ancestor.id);
  }
}

export function renderLayers() {
  const list = el("layers-list");
  if (!list) return;
  // An open inline rename must survive selection-driven re-renders; only skip the
  // rebuild while the editor is still FOCUSED (a committed edit blurs to the stage).
  const editing = list.querySelector(".inline-input");
  if (editing && document.activeElement === editing) return;

  revealSelectionPath(); // expand the selection's ancestor path before signing/rebuilding
  const sig = layersSignature();
  if (sig === lastLayersSig && list.childElementCount) {
    applyLayersSelection();
    return;
  }
  lastLayersSig = sig;
  pruneThumbCache();
  list.replaceChildren();
  if (!elements().length && !groups().length) {
    const empty = document.createElement("div");
    empty.className = "layers-empty";
    empty.textContent = "No layers yet.";
    list.appendChild(empty);
    return;
  }
  // Figma orientation (lead 2026-07-02): top row = front of the canvas (painted last).
  // Render the whole scene tree from root, front-at-top, recursively (renderScope
  // reverses the back-to-front computed order at each scope).
  renderScope(null, list, 0);
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

// The parent scope of any node id (element or group), or null for root.
function scopeOf(id) {
  const node = elementById(id) || groupById(id);
  return node ? nodeScope(state.project, node) : null;
}

// A group's own subtree (the group + its descendant groups) — the INVALID drop targets
// for a group drag (dropping a group into its own subtree is a cycle).
function groupSubtreeIds(groupId) {
  return new Set([groupId, ...descendantsOf(state.project, groupId).groups.map((g) => g.id)]);
}

// Is this drag a single element (reorder-eligible) or a 2+ multi-selection drag
// (reparent only — reordering many at once is out of scope)?
function isSingleDrag(dragId) {
  return !(state.selectedIds.has(dragId) && state.selectedIds.size > 1);
}

// Resolve the drop under the pointer into a plan for the active `drag`:
//   { kind: "reparent", groupId }  — element joins a screen (or null = top level)
//   { kind: "reorder", scope, overId, after, rect } — move a node among same-scope siblings
//   { kind: "nest-into", parentId } — a group nests INTO a group (or null = root), front
//   { kind: "nest-at", scope, overId, after, rect } — a group reparents to scope at index
//   { kind: "none" } — outside the panel, or a group over its own subtree (cycle guard)
function dropPlan(clientX, clientY, drag) {
  const node = document.elementFromPoint(clientX, clientY);
  if (!node || !node.closest("#layers-panel")) return { kind: "none" };
  const dragScope = scopeOf(drag.id);
  const head = node.closest(".group-head");
  const row = node.closest(".layer-row[data-element-id]");

  if (drag.kind === "group") {
    const subtree = groupSubtreeIds(drag.id); // cycle guard: these targets are inert
    // Over a group HEADER: the middle nests INTO it; the top/bottom edge inserts it as a
    // SIBLING of that group (reorder in-scope, or reparent to that scope cross-scope).
    if (head && head.dataset.groupId) {
      const gid = head.dataset.groupId;
      if (subtree.has(gid)) return { kind: "none" };
      const rect = head.getBoundingClientRect();
      const zone = (clientY - rect.top) / rect.height;
      if (zone >= 0.25 && zone <= 0.75) return { kind: "nest-into", parentId: gid };
      const scope = scopeOf(gid);
      if (scope !== null && subtree.has(scope)) return { kind: "none" };
      const after = zone > 0.5;
      return scope === dragScope
        ? { kind: "reorder", scope, overId: gid, after, rect }
        : { kind: "nest-at", scope, overId: gid, after, rect };
    }
    // Between element rows: place among the OVER row's scope siblings (its indent already
    // encodes the target scope — the insertion line lands there).
    if (row) {
      const overId = row.dataset.elementId;
      if (overId === drag.id) return { kind: "none" };
      const scope = row.dataset.groupId || null;
      if (scope !== null && subtree.has(scope)) return { kind: "none" };
      const rect = row.getBoundingClientRect();
      const after = clientY > rect.top + rect.height / 2;
      return scope === dragScope
        ? { kind: "reorder", scope, overId, after, rect }
        : { kind: "nest-at", scope, overId, after, rect };
    }
    return { kind: "nest-into", parentId: null }; // panel background / gap = top level, front
  }

  // Element drag (unchanged intents), generalized to nested groups: a group header =
  // reparent (join it, works at any depth); a same-scope element row = reorder; a
  // foreign-scope element row = reparent to that scope; empty space = top level.
  if (head && head.dataset.groupId) return { kind: "reparent", groupId: head.dataset.groupId };
  if (row) {
    const overId = row.dataset.elementId;
    const overScope = row.dataset.groupId || null;
    if (overScope === dragScope && overId !== drag.id && isSingleDrag(drag.id)) {
      const rect = row.getBoundingClientRect();
      const after = clientY > rect.top + rect.height / 2;
      return { kind: "reorder", scope: dragScope, overId, after, rect };
    }
    return { kind: "reparent", groupId: overScope }; // foreign scope, or same-scope multi -> reparent/no-op
  }
  return { kind: "reparent", groupId: null }; // panel header / empty space / gap = top level
}

// Merged-sibling index (0 = back) for placing a group into a DIFFERENT scope via drag.
// The group is not in that scope, so the destination siblings are the full computed
// order; convert the front-first insertion (top/bottom half of the over row) to the op's
// back -> front index.
function nestTargetIndex(scope, overId, after) {
  const back = orderedChildren(state.project, scope); // back -> front (drag not present)
  const j = back.findIndex((sibling) => sibling.id === overId);
  if (j < 0) return undefined; // fall back to front
  return Math.max(0, Math.min(back.length, after ? j : j + 1));
}

// Target merged-sibling index (0 = back) for a reorder plan. The panel lists FRONT-first
// (Figma), the reverse of the back → front computed order — so run the remove-then-insert
// math in visual space over the scope's MERGED siblings (elements + groups), then map back
// to the op's back → front index. Works for an element OR a group drag (both are nodes in
// orderedChildren).
function reorderTargetIndex(dragId, plan) {
  const siblings = orderedChildren(state.project, plan.scope);
  const visual = [...siblings].reverse();
  const overIndex = visual.findIndex((sibling) => sibling.id === plan.overId);
  const dragIndex = visual.findIndex((sibling) => sibling.id === dragId);
  if (overIndex < 0 || dragIndex < 0) return null;
  let insert = plan.after ? overIndex + 1 : overIndex;
  if (dragIndex < insert) insert -= 1; // account for removing the dragged row first
  insert = Math.max(0, Math.min(visual.length - 1, insert));
  if (insert === dragIndex) return null;
  return visual.length - 1 - insert; // visual position -> back → front (op) index
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

function updateDropHint(clientX, clientY, drag) {
  clearDropHint();
  const plan = dropPlan(clientX, clientY, drag);
  if (plan.kind === "reorder" || plan.kind === "nest-at") {
    showDropLine(plan.rect, plan.after);
    return;
  }
  if (plan.kind === "nest-into") {
    if (plan.parentId === null) {
      el("layers-panel").classList.add("drop-root");
      return;
    }
    const head = document.querySelector(`.group-head[data-group-id="${plan.parentId}"]`);
    if (head) head.classList.add("drop-target");
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
  // "none" (a group over its own subtree, or outside the panel): no hint (inert target).
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
  updateDropHint(event.clientX, event.clientY, layerDrag);
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

  const plan = dropPlan(event.clientX, event.clientY, drag);
  if (plan.kind === "none") return;

  if (plan.kind === "reorder") {
    // ONE reorderNode per drop (element OR group) — one gesture, one journal entry.
    const target = reorderTargetIndex(drag.id, plan);
    if (target !== null) reorderNodeTo(drag.id, target);
    return;
  }

  if (plan.kind === "nest-into") {
    // ONE reparentGroup per drop: nest the group into a group (front) or out to root.
    reparentGroupTo(drag.id, plan.parentId, undefined);
    return;
  }

  if (plan.kind === "nest-at") {
    // ONE reparentGroup per drop: move the group to another scope at the drop index.
    reparentGroupTo(drag.id, plan.scope, nestTargetIndex(plan.scope, plan.overId, plan.after));
    return;
  }

  // Reparent is element-only (a group drag never yields "reparent"): a single row, or the
  // whole selection when the dragged row is in a 2+ set.
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

// ---- draggable panel width (T0224 item 8c) -----------------------------------
//
// The layers panel width is a persisted VIEW pref (localStorage, like
// canvas.layersCollapsed — the established view prefs here are GLOBAL, not per-project,
// so this follows that convention for a consistent panel across projects). Dragging the
// right-edge handle resizes it live (clamped) and saves on release.
const LAYERS_WIDTH_KEY = "canvas.layersWidth";
const LAYERS_MIN_W = 170;
const LAYERS_MAX_W = 520;

function applyLayersWidth(width) {
  const panel = el("layers-panel");
  if (!panel) return null;
  const w = Math.max(LAYERS_MIN_W, Math.min(LAYERS_MAX_W, Math.round(width)));
  panel.style.width = `${w}px`;
  return w;
}

function initLayersResize() {
  const handle = el("layers-resize");
  const panel = el("layers-panel");
  if (!handle || !panel) return;
  try {
    const saved = Number(localStorage.getItem(LAYERS_WIDTH_KEY));
    if (Number.isFinite(saved) && saved > 0) applyLayersWidth(saved);
  } catch {
    // Private mode / disabled storage: default width, no persist.
  }

  let resizing = null;
  handle.addEventListener("mousedown", (event) => {
    if (event.button !== 0) return;
    event.preventDefault();
    resizing = { startX: event.clientX, startW: panel.getBoundingClientRect().width, applied: null };
    document.body.classList.add("layers-resizing");
  });
  window.addEventListener("mousemove", (event) => {
    if (!resizing) return;
    resizing.applied = applyLayersWidth(resizing.startW + (event.clientX - resizing.startX));
    hooks.renderCanvas(); // the stage width changed — repaint crisp
  });
  window.addEventListener("mouseup", () => {
    if (!resizing) return;
    const finalW = resizing.applied ?? panel.getBoundingClientRect().width;
    resizing = null;
    document.body.classList.remove("layers-resizing");
    try {
      localStorage.setItem(LAYERS_WIDTH_KEY, String(Math.round(finalW)));
    } catch {
      // Private mode / disabled storage: this session keeps the width, just no persist.
    }
  });
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

  initLayersResize();

  window.addEventListener("mousemove", onLayerMouseMove);
  window.addEventListener("mouseup", onLayerMouseUp);
}
