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
  focusStage,
  groupById,
  groups,
  hooks,
  memberElements,
  refresh,
  selectedElements,
  selectRegion,
  state,
} from "./app.js";
import {
  deleteRegion,
  detectRegionsFor,
  exportElementIds,
  exportProjectAction,
  patchElementBox,
  patchGroupBox,
  renameElement,
  renameGroup,
  renameRegion,
  renderScreen,
  setExportRows,
  setGroupBackground,
  setGroupVisible,
  sliceRegionsFor,
} from "./actions.js";
import { lastDestinationName } from "./export_dest.mjs";
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

// Compact region row: index + name + delete. Rect/coords live in the row
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
  // Regions are named at creation/detect; the sizes live in the tooltip, so an
  // unnamed (legacy) region falls back to its badge number, never raw sizes.
  label.textContent = region.name || `Region ${index + 1}`;

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
  // Only ops with no other UI path get buttons (lead: Edit/+Add were redundant —
  // dblclick enters the mode, dragging on the image draws a rect).
  const btnRow = document.createElement("div");
  btnRow.className = "insp-region-btnrow";
  btnRow.append(smallBtn("Detect", () => detectRegionsFor(element.id)));
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
  future.textContent = "Coming soon: Alpha cutout / Generate (matte pipeline)";
  actions.appendChild(future);
  body.appendChild(actions);
}

// ---- export (Figma-style rows persisted on the element) ----------------------

const SCALE_PRESETS = ["0.5x", "1x", "2x", "3x", "4x"];
const EXPORT_FORMATS = ["png", "jpg", "webp"];

// The element's export rows, or the implicit single 1x-png default shown for a
// layer with no settings yet (matches Figma + the op's default).
function exportRowsOf(element) {
  const rows = Array.isArray(element.export) ? element.export : [];
  return rows.length ? rows.map((row) => ({ ...row })) : [{ scale: "1x", suffix: "", format: "png", resample: "lanczos" }];
}

function selectInput(value, options, onCommit) {
  const select = document.createElement("select");
  select.className = "insp-input";
  for (const option of options) {
    const node = document.createElement("option");
    node.value = option;
    node.textContent = option;
    if (option === value) node.selected = true;
    select.appendChild(node);
  }
  select.addEventListener("change", () => onCommit(select.value));
  return select;
}

// One export-setting row: scale + suffix + format on the header line, a quality
// slider only for the lossy formats, and a resample toggle. Every committed edit
// rebuilds ALL rows and calls setExportRows once (one journal entry per change).
function exportRowNode(element, rows, index) {
  const row = rows[index];
  const commit = (patch) => {
    const next = rows.map((item) => ({ ...item }));
    next[index] = { ...next[index], ...patch };
    setExportRows(element.id, next);
  };

  const wrap = document.createElement("div");
  wrap.className = "insp-export-row";

  const head = document.createElement("div");
  head.className = "insp-export-head";
  head.appendChild(field("Scale", (() => {
    const input = textInput(row.scale, (value) => commit({ scale: value }));
    input.setAttribute("list", "insp-scale-presets");
    return input;
  })()));
  head.appendChild(field("Suffix", textInput(row.suffix || "", (value) => commit({ suffix: value }))));
  head.appendChild(field("Format", selectInput(row.format || "png", EXPORT_FORMATS, (value) => commit({ format: value }))));
  wrap.appendChild(head);

  // Quality only applies to the lossy formats (the lead wants visible "сжатие").
  if (row.format === "jpg" || row.format === "webp") {
    const quality = document.createElement("input");
    quality.type = "range";
    quality.min = "1";
    quality.max = "100";
    quality.value = String(row.quality == null ? 90 : row.quality);
    quality.className = "insp-range";
    const value = document.createElement("span");
    value.className = "insp-range-value";
    value.textContent = quality.value;
    quality.addEventListener("input", () => {
      value.textContent = quality.value;
    });
    quality.addEventListener("change", () => commit({ quality: Number(quality.value) }));
    const qRow = field("Quality", quality);
    qRow.appendChild(value);
    wrap.appendChild(qRow);
  }

  wrap.appendChild(field("Resample", selectInput(row.resample || "lanczos", ["lanczos", "nearest"], (value) => commit({ resample: value }))));

  const del = document.createElement("button");
  del.type = "button";
  del.className = "insp-export-del";
  del.title = "Remove export setting";
  del.setAttribute("aria-label", "Remove export setting");
  del.textContent = "×"; // ×
  del.addEventListener("click", () => {
    const next = rows.filter((_, i) => i !== index);
    setExportRows(element.id, next);
  });
  head.appendChild(del);
  return wrap;
}

// A small always-visible line showing WHERE the last export landed (info only; the
// picker opens at this folder every time). Filled asynchronously from IndexedDB.
function exportDestinationHint(projectId) {
  const hint = document.createElement("div");
  hint.className = "insp-export-dest";
  hint.textContent = "Destination: chosen on export";
  lastDestinationName(projectId).then((name) => {
    if (name) hint.textContent = `Last folder: ${name}`;
  }).catch(() => {});
  return hint;
}

// The Export section at the BOTTOM of the inspector: a list of rows, "+ Add export
// setting", the destination hint, and an Export button labeled by the target. Edits
// persist per element through setExportSettings (journaled/undoable).
function renderExport(element, root) {
  const rows = exportRowsOf(element);
  const body = collapsible(root, "export", "Export");

  // Shared preset list for every row's scale input (0.5x/1x/2x/3x/4x + custom).
  const presets = document.createElement("datalist");
  presets.id = "insp-scale-presets";
  for (const preset of SCALE_PRESETS) {
    const option = document.createElement("option");
    option.value = preset;
    presets.appendChild(option);
  }
  body.appendChild(presets);

  const list = document.createElement("div");
  list.className = "insp-export-rows";
  rows.forEach((_, index) => list.appendChild(exportRowNode(element, rows, index)));
  body.appendChild(list);

  const add = smallBtn("+ Add export setting", () => {
    setExportRows(element.id, [...rows, { scale: "1x", suffix: "", format: "png", resample: "lanczos" }]);
  });
  add.classList.add("insp-export-add");
  body.appendChild(add);

  body.appendChild(exportDestinationHint(state.project ? state.project.id : ""));

  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = `Export ${element.name || "element"}`;
  button.addEventListener("click", () => exportElementIds([element.id]));
  body.appendChild(button);
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

  // Export section stays LAST (Figma keeps it at the bottom of the sidebar).
  renderExport(element, root);
}

// The group's BACKGROUND section: mode None/Solid + a color input (enabled for Solid).
// A change persists via patchGroup({background}) through the actions -> applyMutation
// flow (canvas + render honor it). The render-time bg dropdown in "Render group" stays
// a separate one-shot override.
function renderGroupBackground(group, root) {
  const body = collapsible(root, "background", "Background");
  const controls = document.createElement("div");
  controls.className = "insp-render";
  const current = group.background && group.background.type === "color" ? group.background : null;

  const mode = document.createElement("select");
  mode.className = "insp-input";
  for (const [value, text] of [["none", "None"], ["color", "Solid"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = text;
    if ((current ? "color" : "none") === value) option.selected = true;
    mode.appendChild(option);
  }

  const color = document.createElement("input");
  color.type = "color";
  color.value = current ? current.color : "#1a1f2b";
  color.className = "insp-color";
  color.disabled = !current;

  mode.addEventListener("change", () => {
    if (mode.value === "color") {
      color.disabled = false;
      setGroupBackground(group.id, { type: "color", color: color.value });
    } else {
      color.disabled = true;
      setGroupBackground(group.id, null);
    }
  });
  color.addEventListener("change", () => {
    if (mode.value === "color") setGroupBackground(group.id, { type: "color", color: color.value });
  });

  const row = field("Background", mode);
  row.appendChild(color);
  controls.appendChild(row);
  body.appendChild(controls);
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

  renderGroupBackground(group, root);

  const render = collapsible(root, "render", "Render group");
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
  button.textContent = "Render group";
  button.addEventListener("click", () => {
    renderScreen(group.id, {
      scale: Number(scale.value),
      background: bgMode.value === "color" ? color.value : undefined,
    });
  });
  controls.appendChild(button);
  render.appendChild(controls);
}

// Multi-select: keep it simple — each element exports with its OWN persisted rows
// (no shared row editing). The section just states that and offers one Export.
function renderMulti(selected, root) {
  const title = document.createElement("div");
  title.className = "insp-multi-title";
  title.textContent = `${selected.length} elements selected`;
  root.appendChild(title);

  const note = document.createElement("div");
  note.className = "insp-export-note";
  note.textContent = "Each element exports its own settings (1x png by default).";
  root.appendChild(note);

  root.appendChild(exportDestinationHint(state.project ? state.project.id : ""));

  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = `Export ${selected.length} elements`;
  button.addEventListener("click", () => exportElementIds(selected.map((element) => element.id)));
  root.appendChild(button);
}

// Nothing selected: still offer a project-level export of every visible screen.
function renderEmpty(root) {
  const empty = document.createElement("div");
  empty.className = "insp-nothing";
  empty.textContent = "Nothing selected";
  root.appendChild(empty);

  const visibleGroups = groups().filter((group) => group.visible !== false);
  if (!visibleGroups.length) return;

  root.appendChild(exportDestinationHint(state.project ? state.project.id : ""));

  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = `Export project (${visibleGroups.length} ${visibleGroups.length === 1 ? "screen" : "screens"})`;
  button.addEventListener("click", () => exportProjectAction());
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
    return `g:${group.id}|${group.name}|${group.x},${group.y},${group.w},${group.h}|${group.visible !== false}|${memberElements(group.id).length}|${JSON.stringify(group.background || null)}`;
  }
  if (selected.length === 1) {
    const e = selected[0];
    const regions = (e.regions || [])
      .map((r) => `${r.id}~${r.name || ""}~${(r.rect || r.content_bbox || []).join(",")}`)
      .join("|");
    // element.export is part of the structure: a row add/remove/edit must rebuild.
    return `e:${e.id}|${e.name}|${e.x},${e.y},${e.w},${e.h}|${e.source_w},${e.source_h}|${regions}|${JSON.stringify(e.export || [])}|${JSON.stringify(e.meta || {})}`;
  }
  if (selected.length > 1) return `m:${selected.map((e) => e.id).join(",")}`;
  // Empty state carries a project-export button gated by the visible-screen count.
  return `empty:${groups().filter((group) => group.visible !== false).length}`;
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
    renderEmpty(root);
  }
  applyInspectorRegionSelection();
}

export function initInspector() {
  hooks.renderInspector = renderInspector;
}
