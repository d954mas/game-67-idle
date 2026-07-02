// Right inspector panel. Shows editable geometry + read-only detail for the
// current selection: a single element (name + collapsible Position/Size, Regions,
// Provenance, Meta sections), a group/screen (name + Position/Size, Render), a
// multi-selection (count + export), or an empty state. Sections are Figma-Design-tab
// style: a titled header with a collapse chevron whose collapsed state persists in
// localStorage per section (not per project). Numeric edits PATCH through actions on
// change/Enter. Pure rendering/input.
//
// Render is structure-signature guarded: a selection-only change (e.g. selecting a
// different region on the SAME element) does not rebuild the DOM — it only re-applies
// selection classes. That keeps the region rows stable across the two clicks of a
// double-click, so double-click rename actually opens its inline editor.
import {
  el,
  elementById,
  enterRegionEdit,
  focusStage,
  groupById,
  hooks,
  memberElements,
  refresh,
  selectedElements,
  selectRegion,
  state,
} from "./app.js";
import {
  addCenteredRegion,
  deleteRegion,
  detectRegionsFor,
  exportElementIds,
  patchElementBox,
  patchGroupBox,
  renameElement,
  renameGroup,
  renameRegion,
  renderScreen,
  setGroupVisible,
  sliceRegionsFor,
} from "./actions.js";
import { openContextMenu } from "./context_menu.js";
import { inlineEdit } from "./inline.js";

function field(label, node) {
  const row = document.createElement("label");
  row.className = "insp-field";
  const span = document.createElement("span");
  span.className = "insp-label";
  span.textContent = label;
  row.appendChild(span);
  row.appendChild(node);
  return row;
}

// Enter commits + returns focus to the stage (so Ctrl+Z hits the canvas op, not the
// input's text-undo); Escape reverts + blurs. Commit itself fires on the change
// event (also on blur), so a click-away still commits.
function textInput(value, onCommit) {
  const input = document.createElement("input");
  input.type = "text";
  input.className = "insp-input";
  input.value = value == null ? "" : String(value);
  const commit = () => {
    const next = input.value.trim();
    if (next && next !== String(value)) onCommit(next);
  };
  input.addEventListener("change", commit);
  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      input.blur();
      focusStage();
    } else if (event.key === "Escape") {
      event.preventDefault();
      input.value = value == null ? "" : String(value);
      input.blur();
      focusStage();
    }
  });
  return input;
}

function numberInput(value, onCommit) {
  const input = document.createElement("input");
  input.type = "number";
  input.className = "insp-input num";
  input.value = Number(value) || 0;
  const commit = () => {
    const next = Number(input.value);
    if (Number.isFinite(next) && next !== Number(value)) onCommit(next);
  };
  input.addEventListener("change", commit);
  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      input.blur();
      focusStage();
    } else if (event.key === "Escape") {
      event.preventDefault();
      input.value = Number(value) || 0;
      input.blur();
      focusStage();
    }
  });
  return input;
}

// A 2x2 grid of X/Y/W/H inputs bound to a patch function.
function boxGrid(box, onPatch) {
  const grid = document.createElement("div");
  grid.className = "insp-grid";
  const add = (key, label) => grid.appendChild(field(label, numberInput(box[key], (value) => onPatch({ [key]: value }))));
  add("x", "X");
  add("y", "Y");
  add("w", "W");
  add("h", "H");
  return grid;
}

function readOnly(label, value) {
  const row = document.createElement("div");
  row.className = "insp-ro";
  const span = document.createElement("span");
  span.className = "insp-label";
  span.textContent = label;
  const val = document.createElement("span");
  val.className = "insp-value";
  val.textContent = value;
  row.append(span, val);
  return row;
}

// ---- collapsible section -----------------------------------------------------

const COLLAPSE_KEY = "canvas.inspector.collapsed";

function loadCollapsed() {
  try {
    return JSON.parse(localStorage.getItem(COLLAPSE_KEY) || "{}") || {};
  } catch {
    return {};
  }
}

function isCollapsed(key) {
  return loadCollapsed()[key] === true;
}

function setCollapsed(key, collapsed) {
  const map = loadCollapsed();
  if (collapsed) map[key] = true;
  else delete map[key];
  try {
    localStorage.setItem(COLLAPSE_KEY, JSON.stringify(map));
  } catch {
    // Private mode / disabled storage: collapse still works this session, just no persist.
  }
}

// A titled collapsible section. `key` persists the collapsed state per section
// (shared across selections, not per project). `badge` is an optional trailing
// header node (e.g. a count). Returns the body element to append content to.
function collapsible(root, key, title, badge) {
  const wrap = document.createElement("section");
  wrap.className = "insp-group";
  if (isCollapsed(key)) wrap.classList.add("collapsed");

  const head = document.createElement("button");
  head.type = "button";
  head.className = "insp-group-head";
  const chevron = document.createElement("span");
  chevron.className = "insp-chevron";
  chevron.textContent = "▾"; // ▾
  const label = document.createElement("span");
  label.className = "insp-group-title";
  label.textContent = title;
  head.append(chevron, label);
  if (badge) head.appendChild(badge);
  head.addEventListener("click", () => {
    const collapsed = !wrap.classList.contains("collapsed");
    wrap.classList.toggle("collapsed", collapsed);
    setCollapsed(key, collapsed);
  });

  const body = document.createElement("div");
  body.className = "insp-group-body";
  wrap.append(head, body);
  root.appendChild(wrap);
  return body;
}

function smallBtn(label, onClick) {
  const button = document.createElement("button");
  button.type = "button";
  button.className = "insp-btn-small";
  button.textContent = label;
  button.addEventListener("click", onClick);
  return button;
}

// ---- regions -----------------------------------------------------------------

// Compact region row: index + name-or-size + delete. Rect/coords live in the row
// tooltip to keep the panel calm; double-click the label to rename (journaled via
// setRegions), click to select on canvas, right-click for the region menu. The ×
// is a generous, always-present hit target (no hover-only reveal that would shift
// under the cursor). data-region-id lets the selection overlay update it in place.
function regionRowInspector(element, region, index) {
  const rect = region.rect || region.content_bbox || [0, 0, 0, 0];
  const row = document.createElement("div");
  row.className = "insp-region-row";
  row.dataset.regionId = region.id;
  if (state.regionEditId === element.id && state.selectedRegionIds.has(region.id)) row.classList.add("selected");
  row.title = `${rect[2]}×${rect[3]} @ (${rect[0]}, ${rect[1]})`;

  const idx = document.createElement("span");
  idx.className = "region-idx";
  idx.textContent = String(index + 1);

  const label = document.createElement("span");
  label.className = "region-label";
  label.textContent = region.name || `${rect[2]}×${rect[3]}`;

  const del = document.createElement("button");
  del.type = "button";
  del.className = "region-del";
  del.title = "Delete region";
  del.setAttribute("aria-label", "Delete region");
  del.textContent = "×"; // ×
  // Stop mousedown so the row's selection/drag wiring never eats the click, and
  // delete on click (journaled setRegions without this region; undoable).
  del.addEventListener("mousedown", (event) => event.stopPropagation());
  del.addEventListener("click", (event) => {
    event.stopPropagation();
    deleteRegion(element.id, region.id);
  });

  row.append(idx, label, del);
  row.addEventListener("click", (event) => {
    if (event.target.closest("button")) return;
    selectRegion(element.id, region.id, event.shiftKey || event.ctrlKey || event.metaKey);
    refresh();
  });
  row.addEventListener("dblclick", (event) => {
    if (event.target.closest("button")) return;
    event.stopPropagation();
    inlineEdit(label, region.name || "", (next) => renameRegion(element.id, region.id, next));
  });
  row.addEventListener("contextmenu", (event) => {
    event.preventDefault();
    selectRegion(element.id, region.id, false);
    refresh();
    openContextMenu(event.clientX, event.clientY, { kind: "region", elementId: element.id, regionId: region.id });
  });
  return row;
}

// Region ids currently selected on this element in region-edit mode.
function selectedRegionIdsFor(element) {
  const regions = element.regions || [];
  return [...state.selectedRegionIds].filter(
    (id) => state.regionEditId === element.id && regions.some((region) => region.id === id),
  );
}

// The REGIONS section: a collapsible header + count badge, one compact row per
// region (numbers match the canvas badges), and the entry points now that the
// layers tree no longer duplicates this list: Edit (isolation mode), Detect, Add,
// and Slice (selected in region-edit mode, else all).
function renderRegions(element, root) {
  const regions = element.regions || [];
  const badge = document.createElement("span");
  badge.className = "badge";
  badge.textContent = String(regions.length);
  const body = collapsible(root, "regions", "Regions", badge);

  if (regions.length) {
    const list = document.createElement("div");
    list.className = "insp-regions";
    regions.forEach((region, index) => list.appendChild(regionRowInspector(element, region, index)));
    body.appendChild(list);
  } else {
    const empty = document.createElement("div");
    empty.className = "insp-region-empty";
    empty.textContent = "No regions yet. Double-click the image to draw one.";
    body.appendChild(empty);
  }

  const actions = document.createElement("div");
  actions.className = "insp-region-actions";
  const btnRow = document.createElement("div");
  btnRow.className = "insp-region-btnrow";
  btnRow.append(
    smallBtn("Edit", () => {
      enterRegionEdit(element.id);
      refresh();
    }),
    smallBtn("Detect", () => detectRegionsFor(element.id)),
    smallBtn("+ Add", () => addCenteredRegion(element.id)),
  );
  actions.appendChild(btnRow);

  const sliceBtn = document.createElement("button");
  sliceBtn.type = "button";
  sliceBtn.className = "primary insp-btn insp-slice-btn";
  const selectedIds = selectedRegionIdsFor(element);
  sliceBtn.disabled = regions.length === 0;
  sliceBtn.textContent = selectedIds.length
    ? `Slice selected (${selectedIds.length})`
    : regions.length
      ? `Slice all (${regions.length})`
      : "Slice";
  // Recompute the target at click time so the (possibly overlay-updated) selection
  // is always honored without a stale closure.
  sliceBtn.addEventListener("click", () => {
    const ids = selectedRegionIdsFor(element);
    sliceRegionsFor(element.id, ids.length ? ids : undefined);
  });
  actions.appendChild(sliceBtn);

  const future = document.createElement("div");
  future.className = "insp-region-hint";
  future.textContent = "Alpha / Generate — with the matte pipeline";
  actions.appendChild(future);
  body.appendChild(actions);
}

// ---- element / group / multi views -------------------------------------------

function renderElement(element, root) {
  const name = field("Name", textInput(element.name, (next) => renameElement(element.id, next)));
  name.classList.add("insp-name");
  root.appendChild(name);

  const layout = collapsible(root, "layout", "Position & Size");
  layout.appendChild(boxGrid(element, (patch) => patchElementBox(element.id, patch)));
  layout.appendChild(readOnly("Source", `${element.source_w || element.w} x ${element.source_h || element.h}`));

  renderRegions(element, root);

  if (element.meta && element.meta.parent) {
    const prov = collapsible(root, "provenance", "Provenance");
    const parent = element.meta.parent;
    const parentEl = parent.elementId ? elementById(parent.elementId) : null;
    prov.appendChild(readOnly("Parent sheet", parentEl ? parentEl.name : parent.sheetSrc || parent.elementId || "—"));
    prov.appendChild(readOnly("Region", String(parent.regionId || "—")));
  }

  const metaKeys = Object.keys(element.meta || {}).filter((key) => key !== "parent");
  if (metaKeys.length) {
    const meta = collapsible(root, "meta", "Meta");
    for (const key of metaKeys) {
      meta.appendChild(
        readOnly(key, typeof element.meta[key] === "object" ? JSON.stringify(element.meta[key]) : String(element.meta[key])),
      );
    }
  }
}

function renderGroupInspector(group, root) {
  const name = field("Name", textInput(group.name, (next) => renameGroup(group.id, next)));
  name.classList.add("insp-name");
  root.appendChild(name);

  const layout = collapsible(root, "layout", "Position & Size");
  layout.appendChild(boxGrid(group, (patch) => patchGroupBox(group.id, patch)));

  const visRow = document.createElement("label");
  visRow.className = "insp-check";
  const check = document.createElement("input");
  check.type = "checkbox";
  check.checked = group.visible !== false;
  check.addEventListener("change", () => setGroupVisible(group.id, check.checked));
  const visLabel = document.createElement("span");
  visLabel.textContent = "Visible";
  visRow.append(check, visLabel);
  layout.appendChild(visRow);
  layout.appendChild(readOnly("Members", String(memberElements(group.id).length)));

  const render = collapsible(root, "render", "Render screen");
  const controls = document.createElement("div");
  controls.className = "insp-render";

  const scale = document.createElement("select");
  scale.className = "insp-input";
  for (const value of [1, 2, 4]) {
    const option = document.createElement("option");
    option.value = String(value);
    option.textContent = `${value}x`;
    scale.appendChild(option);
  }
  controls.appendChild(field("Scale", scale));

  const bgMode = document.createElement("select");
  bgMode.className = "insp-input";
  for (const [value, text] of [["transparent", "Transparent"], ["color", "Color"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = text;
    bgMode.appendChild(option);
  }
  const color = document.createElement("input");
  color.type = "color";
  color.value = "#1a1f2b";
  color.className = "insp-color";
  color.disabled = true;
  bgMode.addEventListener("change", () => {
    color.disabled = bgMode.value !== "color";
  });
  const bgRow = field("Background", bgMode);
  bgRow.appendChild(color);
  controls.appendChild(bgRow);

  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = "Render screen";
  button.addEventListener("click", () => {
    renderScreen(group.id, {
      scale: Number(scale.value),
      background: bgMode.value === "color" ? color.value : undefined,
    });
  });
  controls.appendChild(button);
  render.appendChild(controls);
}

function renderMulti(selected, root) {
  const title = document.createElement("div");
  title.className = "insp-multi-title";
  title.textContent = `${selected.length} elements selected`;
  root.appendChild(title);
  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = "Export selected";
  button.addEventListener("click", () => exportElementIds(selected.map((element) => element.id)));
  root.appendChild(button);
}

// ---- render + selection overlay ----------------------------------------------

// Structure signature: everything that changes which nodes/values are rendered,
// EXCLUDING region selection + region-edit mode (those are a lightweight overlay).
// When only the region selection changes, the signature is unchanged and the DOM is
// left in place (the region rows survive a double-click's two clicks).
function inspectorSig() {
  const group = state.selectedGroupId ? groupById(state.selectedGroupId) : null;
  const selected = selectedElements();
  if (group) {
    return `g:${group.id}|${group.name}|${group.x},${group.y},${group.w},${group.h}|${group.visible !== false}|${memberElements(group.id).length}`;
  }
  if (selected.length === 1) {
    const e = selected[0];
    const regions = (e.regions || [])
      .map((r) => `${r.id}~${r.name || ""}~${(r.rect || r.content_bbox || []).join(",")}`)
      .join("|");
    return `e:${e.id}|${e.name}|${e.x},${e.y},${e.w},${e.h}|${e.source_w},${e.source_h}|${regions}|${JSON.stringify(e.meta || {})}`;
  }
  if (selected.length > 1) return `m:${selected.map((e) => e.id).join(",")}`;
  return "empty";
}

// Re-apply region selection to the already-built DOM (used on a skipped rebuild):
// row .selected classes + the Slice button label/disabled.
function applyInspectorRegionSelection() {
  const root = el("inspector");
  if (!root) return;
  const selected = selectedElements();
  if (selected.length !== 1) return;
  const element = selected[0];
  for (const row of root.querySelectorAll(".insp-region-row[data-region-id]")) {
    const on = state.regionEditId === element.id && state.selectedRegionIds.has(row.dataset.regionId);
    row.classList.toggle("selected", on);
  }
  const sliceBtn = root.querySelector(".insp-slice-btn");
  if (sliceBtn) {
    const regions = element.regions || [];
    const ids = selectedRegionIdsFor(element);
    sliceBtn.disabled = regions.length === 0;
    sliceBtn.textContent = ids.length
      ? `Slice selected (${ids.length})`
      : regions.length
        ? `Slice all (${regions.length})`
        : "Slice";
  }
}

let lastSig = null;

export function renderInspector() {
  const root = el("inspector");
  if (!root) return;
  // An open inline region-rename input must survive selection-driven re-renders;
  // only skip while it is actually focused (a committed edit blurs to the stage).
  const editing = root.querySelector(".inline-input");
  if (editing && document.activeElement === editing) return;

  const sig = inspectorSig();
  if (sig === lastSig && root.childElementCount) {
    applyInspectorRegionSelection();
    return;
  }
  lastSig = sig;
  root.replaceChildren();

  const group = state.selectedGroupId ? groupById(state.selectedGroupId) : null;
  const selected = selectedElements();
  if (group) {
    renderGroupInspector(group, root);
  } else if (selected.length === 1) {
    renderElement(selected[0], root);
  } else if (selected.length > 1) {
    renderMulti(selected, root);
  } else {
    const empty = document.createElement("div");
    empty.className = "insp-nothing";
    empty.textContent = "Nothing selected";
    root.appendChild(empty);
  }
  applyInspectorRegionSelection();
}

export function initInspector() {
  hooks.renderInspector = renderInspector;
}
