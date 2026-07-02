// Right inspector panel. Shows editable geometry + read-only detail for the
// current selection: a single element (name, X/Y/W/H, source size, provenance,
// regions, meta), a group/screen (name, X/Y/W/H, visible, member count, render
// controls), a multi-selection (count + export), or an empty state. Numeric edits
// PATCH through actions on change/Enter. Pure rendering/input.
import {
  el,
  elementById,
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
    if (event.key === "Enter") input.blur();
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
    if (event.key === "Enter") input.blur();
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

function section(title) {
  const node = document.createElement("div");
  node.className = "insp-section";
  node.textContent = title;
  return node;
}

// Compact region row: index + name-or-size + delete. Rect/coords live in the row
// tooltip to keep the panel calm; double-click the label to rename (journaled via
// setRegions), click to select on canvas, right-click for the region menu.
function regionRowInspector(element, region, index) {
  const rect = region.rect || region.content_bbox || [0, 0, 0, 0];
  const row = document.createElement("div");
  row.className = "insp-region-row";
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
  del.textContent = "×";
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

// The REGIONS section: a header with a count badge, one compact row per region
// (numbers match the canvas badges + layers tree), discoverable + Add / Slice
// buttons, and a single muted placeholder line for the future matte pipeline.
function renderRegions(element, root) {
  const regions = element.regions || [];

  const head = document.createElement("div");
  head.className = "insp-section insp-region-head";
  const title = document.createElement("span");
  title.textContent = "Regions";
  const badge = document.createElement("span");
  badge.className = "badge";
  badge.textContent = String(regions.length);
  head.append(title, badge);
  root.appendChild(head);

  if (regions.length) {
    const list = document.createElement("div");
    list.className = "insp-regions";
    regions.forEach((region, index) => list.appendChild(regionRowInspector(element, region, index)));
    root.appendChild(list);
  } else {
    const empty = document.createElement("div");
    empty.className = "insp-region-empty";
    empty.textContent = "No regions yet.";
    root.appendChild(empty);
  }

  const actions = document.createElement("div");
  actions.className = "insp-region-actions";

  const addBtn = document.createElement("button");
  addBtn.type = "button";
  addBtn.className = "insp-btn";
  addBtn.textContent = "+ Add region";
  addBtn.addEventListener("click", () => addCenteredRegion(element.id));
  actions.appendChild(addBtn);

  const selectedIds = [...state.selectedRegionIds].filter(
    (id) => state.regionEditId === element.id && regions.some((region) => region.id === id),
  );
  const sliceBtn = document.createElement("button");
  sliceBtn.type = "button";
  sliceBtn.className = "primary insp-btn";
  sliceBtn.textContent = selectedIds.length ? `Slice selected (${selectedIds.length})` : "Slice selected region(s)";
  sliceBtn.disabled = selectedIds.length === 0;
  sliceBtn.addEventListener("click", () => sliceRegionsFor(element.id, selectedIds));
  actions.appendChild(sliceBtn);

  const future = document.createElement("div");
  future.className = "insp-region-hint";
  future.textContent = "Alpha / Generate — with the matte pipeline";
  actions.appendChild(future);
  root.appendChild(actions);
}

function renderElement(element, root) {
  root.appendChild(section("Element"));
  root.appendChild(field("Name", textInput(element.name, (next) => renameElement(element.id, next))));
  root.appendChild(boxGrid(element, (patch) => patchElementBox(element.id, patch)));
  root.appendChild(readOnly("Source", `${element.source_w || element.w} x ${element.source_h || element.h}`));

  renderRegions(element, root);

  if (element.meta && element.meta.parent) {
    root.appendChild(section("Provenance"));
    const parent = element.meta.parent;
    const parentEl = parent.elementId ? elementById(parent.elementId) : null;
    root.appendChild(readOnly("Parent sheet", parentEl ? parentEl.name : parent.sheetSrc || parent.elementId || "—"));
    root.appendChild(readOnly("Region", String(parent.regionId || "—")));
  }

  const metaKeys = Object.keys(element.meta || {}).filter((key) => key !== "parent");
  if (metaKeys.length) {
    root.appendChild(section("Meta"));
    for (const key of metaKeys) {
      root.appendChild(readOnly(key, typeof element.meta[key] === "object" ? JSON.stringify(element.meta[key]) : String(element.meta[key])));
    }
  }
}

function renderGroupInspector(group, root) {
  root.appendChild(section("Screen"));
  root.appendChild(field("Name", textInput(group.name, (next) => renameGroup(group.id, next))));
  root.appendChild(boxGrid(group, (patch) => patchGroupBox(group.id, patch)));

  const visRow = document.createElement("label");
  visRow.className = "insp-check";
  const check = document.createElement("input");
  check.type = "checkbox";
  check.checked = group.visible !== false;
  check.addEventListener("change", () => setGroupVisible(group.id, check.checked));
  const label = document.createElement("span");
  label.textContent = "Visible";
  visRow.append(check, label);
  root.appendChild(visRow);

  root.appendChild(readOnly("Members", String(memberElements(group.id).length)));

  root.appendChild(section("Render screen"));
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
  root.appendChild(controls);
}

function renderMulti(selected, root) {
  root.appendChild(section("Selection"));
  root.appendChild(readOnly("Selected", `${selected.length} elements`));
  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = "Export selected";
  button.addEventListener("click", () => exportElementIds(selected.map((element) => element.id)));
  root.appendChild(button);
}

export function renderInspector() {
  const root = el("inspector");
  if (!root) return;
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
}

export function initInspector() {
  hooks.renderInspector = renderInspector;
}
