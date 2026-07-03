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
  hooks,
  memberElements,
  rangeSelectIds,
  refresh,
  selectedElements,
  selectRegion,
  selectRegionRange,
  state,
} from "./app.js";
import {
  alphaCutoutBatchFor,
  alphaCutoutFor,
  deleteRegion,
  detectRegionsFor,
  exportElementIds,
  exportProjectAction,
  fitGroupAction,
  patchElementBox,
  patchGroupBox,
  patchTextElement,
  renameElement,
  renameGroup,
  renameRegion,
  renderScreen,
  setExportRows,
  setGroupBackground,
  setGroupClip,
  setGroupsShared,
  setGroupVisible,
  sliceRegionsFor,
} from "./actions.js";
import { childrenOf, descendantsOf } from "../tree.mjs";
import { fontFamilies, fontWeights } from "./fonts.js";
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
//
// An empty value never commits — the RENAME guard every text field here relies on
// (element/group/text-name/scale never get clobbered by a blank). (The Export row suffix
// was the one field that opted out of this via allowEmpty; it is gone in T0229.)
function textInput(value, onCommit) {
  const input = document.createElement("input");
  input.type = "text";
  input.className = "insp-input";
  input.value = value == null ? "" : String(value);
  const commit = () => {
    const next = input.value.trim();
    if (next === String(value == null ? "" : value)) return; // unchanged: no commit
    if (!next) return; // empty is always blocked (rename/scale guard)
    onCommit(next);
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

// Anchor for Shift range-selection in the Regions list (mirrors the layers panel's own
// anchor). Module-local, page-only.
let regionAnchorId = null;

// Click-select a region row with Figma modifiers: Shift = the contiguous run from the last
// plain-clicked row (shared rangeSelectIds helper — same math as the layers panel, T0224
// item 5), Ctrl/Cmd = toggle one, plain = select only + set the anchor. Regions are always
// rendered, so the element.regions array order IS the visual (row) order.
function selectRegionRow(element, regionId, event) {
  if (event.shiftKey) {
    const order = (element.regions || []).map((region) => region.id);
    const ids = rangeSelectIds(order, regionAnchorId, regionId);
    if (ids) {
      selectRegionRange(element.id, ids);
      return; // Shift extends the range; the anchor stays put (like layers).
    }
    // No usable anchor: fall through to a plain single select below.
  }
  const additive = event.ctrlKey || event.metaKey;
  selectRegion(element.id, regionId, additive);
  regionAnchorId = regionId; // plain AND Ctrl clicks move the anchor (matches layers)
}

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
    selectRegionRow(element, region.id, event);
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
  // Pass the button as the triggering control so runLongOp disables it while detect is
  // queued/in flight (the canvas stays interactive).
  const detectBtn = smallBtn("Detect", () => detectRegionsFor(element.id, detectBtn));
  btnRow.append(detectBtn);
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
    sliceRegionsFor(element.id, ids.length ? ids : undefined, sliceBtn);
  });
  actions.appendChild(sliceBtn);

  // Alpha cutout (T0210): a method dropdown + run button on the selected IMAGE element.
  // Auto routes (and refuses a dual-plate soft zone loudly); Key matte forces key_matte.
  // Scoped to the selected regions when any are selected in region-edit mode, else the
  // whole element. Long-op via the queue + progress toast (mirrors Slice).
  const alphaRow = document.createElement("div");
  alphaRow.className = "insp-alpha-row";
  const methodSel = document.createElement("select");
  methodSel.className = "insp-input";
  for (const [value, label] of [["auto", "Auto"], ["matte", "Key matte"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = label;
    methodSel.appendChild(option);
  }
  const alphaBtn = document.createElement("button");
  alphaBtn.type = "button";
  alphaBtn.className = "insp-btn insp-alpha-btn";
  const alphaSelected = selectedRegionIdsFor(element);
  alphaBtn.textContent = alphaSelected.length ? `Alpha cutout (${alphaSelected.length})` : "Alpha cutout";
  // Recompute the region scope at click time so an overlay-updated selection is honored.
  alphaBtn.addEventListener("click", () => {
    const ids = selectedRegionIdsFor(element);
    alphaCutoutFor(element.id, methodSel.value, ids.length ? ids : undefined, alphaBtn);
  });
  alphaRow.append(field("Alpha", methodSel), alphaBtn);
  actions.appendChild(alphaRow);

  // Generate (dual-plate) stays a muted placeholder — it needs a white+black plate pair.
  const future = document.createElement("div");
  future.className = "insp-region-hint";
  future.textContent = "Coming soon: Generate (dual-plate pair)";
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
  return rows.length ? rows.map((row) => ({ ...row })) : [{ scale: "1x", format: "png", resample: "lanczos" }];
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

// Export size control (lead's spec 2026-07-03): a MODE switch Scale | W px | H px,
// a value field per mode, and the computed result size ("→ 2048×1536") so the
// source-pixel semantics are visible at a glance. Scale mode = preset select
// (0.5x…4x) + "Custom…" for a free fractional multiplier; W/H modes = a pixel
// number. All three write the same stored token ("0.75x"/"512w"/"512h") — the op
// layer is untouched. Switching modes converts the current setting to the
// EQUIVALENT token against the source pixels, so the output size never jumps.
function parseScaleToken(token) {
  const text = String(token == null ? "" : token).trim().toLowerCase();
  let match = /^(\d+(?:\.\d+)?)x?$/.exec(text);
  if (match && Number(match[1]) > 0) return { kind: "mul", value: Number(match[1]) };
  match = /^(\d+(?:\.\d+)?)(w|h)$/.exec(text);
  if (match && Number(match[1]) > 0) return { kind: match[2], value: Number(match[1]) };
  return { kind: "mul", value: 1 }; // unparseable: display as 1x, ops will complain on commit
}

// Mirror of ops.mjs resolveExportScale (kept trivially small; the op stays the
// authority — this only feeds the UI hint and mode conversion).
function resolveScaleToken(spec, sw, sh) {
  if (!(sw > 0) || !(sh > 0)) return null;
  if (spec.kind === "mul") return { w: Math.max(1, Math.round(sw * spec.value)), h: Math.max(1, Math.round(sh * spec.value)) };
  if (spec.kind === "w") return { w: Math.max(1, Math.round(spec.value)), h: Math.max(1, Math.round(sh * (spec.value / sw))) };
  return { w: Math.max(1, Math.round(sw * (spec.value / sh))), h: Math.max(1, Math.round(spec.value)) };
}

function scaleInput(element, row, commit) {
  const sw = Number(element.source_w) || Number(element.w) || 0;
  const sh = Number(element.source_h) || Number(element.h) || 0;
  const current = parseScaleToken(row.scale == null ? "1x" : row.scale);
  const resolved = resolveScaleToken(current, sw, sh);

  const wrap = document.createElement("div");
  wrap.className = "insp-size-ctl";

  // Mode switch. Changing it commits the EQUIVALENT token (same output pixels).
  const mode = document.createElement("select");
  mode.className = "insp-input insp-size-mode";
  for (const [value, label] of [["mul", "Scale"], ["w", "W px"], ["h", "H px"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = label;
    if (value === current.kind) option.selected = true;
    mode.appendChild(option);
  }
  mode.addEventListener("change", () => {
    const target = mode.value;
    if (target === current.kind) return;
    if (target === "w") commit({ scale: `${resolved ? resolved.w : 512}w` });
    else if (target === "h") commit({ scale: `${resolved ? resolved.h : 512}h` });
    else {
      const value = resolved && sw > 0 ? Math.round((resolved.w / sw) * 10000) / 10000 : 1;
      commit({ scale: `${value}x` });
    }
  });
  wrap.appendChild(mode);

  const mount = (node) => {
    wrap.insertBefore(node, hint);
  };

  // Result-size hint: makes "scale multiplies the SOURCE pixels" visible.
  const hint = document.createElement("span");
  hint.className = "insp-size-hint";
  hint.textContent = resolved ? `→ ${resolved.w}×${resolved.h}` : "";
  wrap.appendChild(hint);

  if (current.kind === "mul") {
    const token = `${current.value}x`;
    const values = SCALE_PRESETS.includes(token) ? [...SCALE_PRESETS] : [token, ...SCALE_PRESETS];
    const select = document.createElement("select");
    select.className = "insp-input";
    for (const value of values) {
      const option = document.createElement("option");
      option.value = value;
      option.textContent = value;
      if (value === token) option.selected = true;
      select.appendChild(option);
    }
    const custom = document.createElement("option");
    custom.value = "__custom";
    custom.textContent = "Custom…";
    select.appendChild(custom);
    select.addEventListener("change", () => {
      if (select.value !== "__custom") {
        commit({ scale: select.value });
        return;
      }
      // Free fractional multiplier (0.75 → "0.75x"). Committed edits rebuild the
      // panel; an aborted edit (Esc / blur, no change) falls back to the select.
      const input = numberInput(current.value, (value) => commit({ scale: `${value}x` }));
      input.step = "any";
      input.min = "0";
      input.addEventListener("blur", () => {
        if (wrap.contains(input)) {
          wrap.removeChild(input);
          mount(select);
        }
      });
      wrap.removeChild(select);
      mount(input);
      input.focus();
      input.select();
    });
    mount(select);
  } else {
    const input = numberInput(current.value, (value) => {
      const px = Math.max(1, Math.round(value));
      commit({ scale: `${px}${current.kind}` });
    });
    input.min = "1";
    mount(input);
  }
  return wrap;
}

// One export-setting row: scale + format on the header line, a quality slider only for
// the lossy formats, and a resample toggle. (T0229 removed the Suffix column — file
// names are automatic: element/screen name + a Figma scale marker only when several
// rows collide.) Every committed edit rebuilds ALL rows and calls setExportRows once
// (one journal entry per change).
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
  head.appendChild(field("Size", scaleInput(element, row, commit)));
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

// The Export section at the BOTTOM of the inspector: a list of rows, "+ Add export
// setting", and an Export button labeled by the target. Edits persist per element
// through setExportSettings (journaled/undoable). The destination is chosen in the
// save-file dialog at export time (T0229), so there is no destination hint line.
function renderExport(element, root) {
  const rows = exportRowsOf(element);
  const body = collapsible(root, "export", "Export");

  const list = document.createElement("div");
  list.className = "insp-export-rows";
  rows.forEach((_, index) => list.appendChild(exportRowNode(element, rows, index)));
  body.appendChild(list);

  const add = smallBtn("+ Add export setting", () => {
    setExportRows(element.id, [...rows, { scale: "1x", format: "png", resample: "lanczos" }]);
  });
  add.classList.add("insp-export-add");
  body.appendChild(add);

  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = `Export ${element.name || "element"}`;
  button.addEventListener("click", () => exportElementIds([element.id], button));
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

// ---- text element ------------------------------------------------------------

// A native color picker bound to a commit; seeds from the current #rrggbb value.
function colorInput(value, onCommit) {
  const input = document.createElement("input");
  input.type = "color";
  input.className = "insp-color";
  input.value = /^#[0-9a-fA-F]{6}$/.test(String(value || "")) ? value : "#111111";
  input.addEventListener("change", () => onCommit(input.value));
  return input;
}

// The TEXT section: font family + weight, size, line height, align, fill color, an
// Outline (stroke width + color) row, and a Drop shadow toggle with dx/dy + color. Each
// change commits ONE patchElement (style patch + re-measured box) via patchTextElement.
function renderTextStyle(element, style, root) {
  const body = collapsible(root, "text", "Text");
  const commit = (stylePatch) => patchTextElement(element.id, { style: stylePatch });

  const families = fontFamilies();
  const familySel = selectInput(style.fontFamily, families.length ? families : [style.fontFamily], (family) => {
    // Keep the weight valid for the new family (fall back to its first available weight).
    const weights = fontWeights(family);
    const weight = weights.includes(Number(style.fontWeight)) ? Number(style.fontWeight) : weights[0];
    commit({ fontFamily: family, fontWeight: weight });
  });
  body.appendChild(field("Font", familySel));

  const weights = fontWeights(style.fontFamily).map(String);
  const weightSel = selectInput(String(style.fontWeight), weights.length ? weights : [String(style.fontWeight)], (value) =>
    commit({ fontWeight: Number(value) }),
  );
  body.appendChild(field("Weight", weightSel));

  const sizeRow = document.createElement("div");
  sizeRow.className = "insp-grid";
  sizeRow.appendChild(field("Size", numberInput(style.fontSize, (v) => commit({ fontSize: v }))));
  sizeRow.appendChild(field("Line", numberInput(style.lineHeight, (v) => commit({ lineHeight: v }))));
  body.appendChild(sizeRow);

  body.appendChild(field("Align", selectInput(style.align || "left", ["left", "center", "right"], (v) => commit({ align: v }))));

  const fill = field("Fill", colorInput(style.color, (v) => commit({ color: v })));
  body.appendChild(fill);

  // Outline (stroke): width + color on one row. Width 0 = no outline.
  const stroke = style.stroke || { width: 0, color: "#000000" };
  const strokeRow = field("Outline", numberInput(stroke.width, (v) => commit({ stroke: { width: Math.max(0, v), color: stroke.color || "#000000" } })));
  strokeRow.appendChild(colorInput(stroke.color, (v) => commit({ stroke: { width: Number(stroke.width) || 0, color: v } })));
  body.appendChild(strokeRow);

  // Drop shadow: a hard offset (blur is 0 in v1). Toggle + dx/dy + color when on.
  const shadow = style.shadow || null;
  const shadowToggle = document.createElement("label");
  shadowToggle.className = "insp-check";
  const chk = document.createElement("input");
  chk.type = "checkbox";
  chk.checked = !!shadow;
  chk.addEventListener("change", () =>
    commit({ shadow: chk.checked ? { dx: 2, dy: 2, blur: 0, color: "#000000" } : null }),
  );
  const chkLabel = document.createElement("span");
  chkLabel.textContent = "Drop shadow";
  shadowToggle.append(chk, chkLabel);
  body.appendChild(shadowToggle);

  if (shadow) {
    const offRow = document.createElement("div");
    offRow.className = "insp-grid";
    offRow.appendChild(field("X", numberInput(shadow.dx, (v) => commit({ shadow: { dx: v } }))));
    offRow.appendChild(field("Y", numberInput(shadow.dy, (v) => commit({ shadow: { dy: v } }))));
    body.appendChild(offRow);
    body.appendChild(field("Shadow", colorInput(shadow.color, (v) => commit({ shadow: { color: v } }))));
  }
}

function renderTextElement(element, root) {
  const style = element.style || {};
  const name = field("Name", textInput(element.name, (next) => renameElement(element.id, next)));
  name.classList.add("insp-name");
  root.appendChild(name);

  const layout = collapsible(root, "layout", "Position & Size");
  const grid = document.createElement("div");
  grid.className = "insp-grid";
  grid.appendChild(field("X", numberInput(element.x, (v) => patchElementBox(element.id, { x: v }))));
  grid.appendChild(field("Y", numberInput(element.y, (v) => patchElementBox(element.id, { y: v }))));
  layout.appendChild(grid);
  layout.appendChild(readOnly("Size", `${element.w} x ${element.h} (auto-width)`));

  const hint = document.createElement("div");
  hint.className = "insp-region-hint";
  hint.textContent = "Double-click the text on the canvas to edit its content.";
  root.appendChild(hint);

  renderTextStyle(element, style, root);
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

  // Fit to content (Figma "Resize to fit"): one journaled fitGroup op sets the frame to
  // the union of the group's descendant closure + padding; children never move. Disabled
  // only for a trivially-empty group (no descendant elements or subgroups) — the op still
  // errors loudly on empty, so the context-menu path surfaces that as a toast.
  const closure = state.project ? descendantsOf(state.project, group.id) : { groups: [], elements: [] };
  const isEmpty = closure.elements.length === 0 && closure.groups.length === 0;
  const fitBtn = smallBtn("Fit to content", () => fitGroupAction(group.id));
  fitBtn.classList.add("insp-fit-btn");
  fitBtn.disabled = isEmpty;
  fitBtn.title = isEmpty ? "Nothing to fit — the group has no content" : "Resize the frame to fit its content";
  layout.appendChild(fitBtn);

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

  // Clip content: Figma frame clip — members outside the group bounds are cropped on
  // canvas AND in the subgroup render (journaled via patchGroup({clip}); default off).
  const clipRow = document.createElement("label");
  clipRow.className = "insp-check";
  const clipCheck = document.createElement("input");
  clipCheck.type = "checkbox";
  clipCheck.checked = group.clip === true;
  clipCheck.addEventListener("change", () => setGroupClip(group.id, clipCheck.checked));
  const clipLabel = document.createElement("span");
  clipLabel.textContent = "Clip content";
  clipRow.append(clipCheck, clipLabel);
  layout.appendChild(clipRow);

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
    renderScreen(
      group.id,
      {
        scale: Number(scale.value),
        background: bgMode.value === "color" ? color.value : undefined,
      },
      button,
    );
  });
  controls.appendChild(button);
  render.appendChild(controls);
}

// A tri-state shared checkbox for the multi-group inspector: CHECKED when every value
// agrees true, UNCHECKED when every value agrees false, INDETERMINATE when they disagree
// (an honest mixed state, not a lie). A click drives ALL selected groups to the box's
// resulting state — from indeterminate the browser resolves to checked on the first click
// (Figma), so onCommit(true) sets them all. Pure UI; onCommit(boolean) does the batched write.
function sharedToggle(label, values, onCommit) {
  const allTrue = values.length > 0 && values.every((value) => value === true);
  const allFalse = values.length > 0 && values.every((value) => value === false);
  const row = document.createElement("label");
  row.className = "insp-check";
  const check = document.createElement("input");
  check.type = "checkbox";
  check.checked = allTrue;
  check.indeterminate = !allTrue && !allFalse;
  check.addEventListener("change", () => onCommit(check.checked));
  const span = document.createElement("span");
  span.textContent = label;
  row.append(check, span);
  return row;
}

// Multi-GROUP selection (2+ groups, no loose elements): an honest shared-toggle inspector
// (T0224 item 1). Count header + shared Visible / Clip checkboxes editable when the groups
// agree, indeterminate when they disagree; clicking sets that field on EVERY selected group
// in ONE journaled patchGroups op (one undo). Per-group geometry stays out (it is not a
// shared value).
function renderMultiGroup(groupIds, root) {
  const selected = groupIds.map((id) => groupById(id)).filter(Boolean);
  const title = document.createElement("div");
  title.className = "insp-multi-title";
  title.textContent = `${selected.length} ${selected.length === 1 ? "group" : "groups"}`;
  root.appendChild(title);

  const shared = collapsible(root, "multigroup", "Shared");
  shared.appendChild(
    sharedToggle("Visible", selected.map((group) => group.visible !== false), (value) =>
      setGroupsShared(groupIds, { visible: value }),
    ),
  );
  shared.appendChild(
    sharedToggle("Clip content", selected.map((group) => group.clip === true), (value) =>
      setGroupsShared(groupIds, { clip: value }),
    ),
  );

  const note = document.createElement("div");
  note.className = "insp-export-note";
  note.textContent = "Shared toggles apply to all selected groups in one step.";
  root.appendChild(note);
}

// Batch ALPHA cutout (T0230): when EVERY selected element is an IMAGE, offer the same
// method dropdown as the single-element Alpha row plus one button that keys ALL of them
// in ONE journaled op ("Apply to N images" — one Ctrl+Z restores every element). No
// region scoping here — regions stay single-element (select one image and use its own
// Regions section). Long-op via the queue + progress toast (mirrors the single-element
// row and Slice); the canvas stays interactive while it runs.
function renderMultiAlpha(selected, root) {
  const body = collapsible(root, "multialpha", "Alpha");
  const alphaRow = document.createElement("div");
  alphaRow.className = "insp-alpha-row";
  const methodSel = document.createElement("select");
  methodSel.className = "insp-input";
  for (const [value, label] of [["auto", "Auto"], ["matte", "Key matte"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = label;
    methodSel.appendChild(option);
  }
  const alphaBtn = document.createElement("button");
  alphaBtn.type = "button";
  alphaBtn.className = "insp-btn insp-alpha-btn";
  alphaBtn.textContent = `Apply to ${selected.length} images`;
  alphaBtn.addEventListener("click", () => {
    alphaCutoutBatchFor(selected.map((element) => element.id), methodSel.value, alphaBtn);
  });
  alphaRow.append(field("Alpha", methodSel), alphaBtn);
  body.appendChild(alphaRow);
}

// Multi-select: keep it simple — each element exports with its OWN persisted rows
// (no shared row editing). The section just states that and offers one Export. When
// EVERY selected element is an image, an Alpha section (T0230) offers one batched
// alpha-cutout op ("Apply to N images") ahead of the export note.
function renderMulti(selected, root) {
  const title = document.createElement("div");
  title.className = "insp-multi-title";
  title.textContent = `${selected.length} elements selected`;
  root.appendChild(title);

  if (selected.every((element) => element.type === "image")) {
    renderMultiAlpha(selected, root);
  }

  const note = document.createElement("div");
  note.className = "insp-export-note";
  note.textContent = "Each element exports its own settings (1x png by default).";
  root.appendChild(note);

  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = `Export ${selected.length} elements`;
  button.addEventListener("click", () => exportElementIds(selected.map((element) => element.id), button));
  root.appendChild(button);
}

// Top-level VISIBLE screens — exactly what exportProject renders (every parentId-less
// visible group). Computed via the shared tree helper (childrenOf(root).groups), not a
// hand-rolled scan, so the button label never counts nested component groups (T0224 item 9:
// "Export project (N screens)" must match exportProject, which is top-level only).
function visibleScreenCount() {
  if (!state.project) return 0;
  return childrenOf(state.project, null).groups.filter((group) => group.visible !== false).length;
}

// Nothing selected: still offer a project-level export of every visible screen.
function renderEmpty(root) {
  const empty = document.createElement("div");
  empty.className = "insp-nothing";
  empty.textContent = "Nothing selected";
  root.appendChild(empty);

  const screens = visibleScreenCount();
  if (!screens) return;

  const button = document.createElement("button");
  button.type = "button";
  button.className = "primary insp-btn";
  button.textContent = `Export project (${screens} ${screens === 1 ? "screen" : "screens"})`;
  button.addEventListener("click", () => exportProjectAction(button));
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
    return `g:${group.id}|${group.name}|${group.x},${group.y},${group.w},${group.h}|${group.visible !== false}|${group.clip === true}|${memberElements(group.id).length}|${JSON.stringify(group.background || null)}`;
  }
  // Multi-group selection (2+ groups, no loose elements): the signature carries each
  // group's id + shared toggle state so a batched visible/clip change rebuilds the panel.
  if (state.selectedIds.size === 0 && state.selectedGroupIds.size >= 2) {
    const gs = [...state.selectedGroupIds].map((id) => groupById(id)).filter(Boolean);
    return `mg:${gs.map((g) => `${g.id}~${g.visible !== false ? 1 : 0}~${g.clip === true ? 1 : 0}`).join("|")}`;
  }
  if (selected.length === 1) {
    const e = selected[0];
    // A text element's structure is its content + style (family/size/align/stroke/
    // shadow) — any change must rebuild the Text section so the inputs reflect it.
    if (e.type === "text") {
      return `t:${e.id}|${e.name}|${e.x},${e.y},${e.w},${e.h}|${e.content}|${JSON.stringify(e.style || {})}`;
    }
    const regions = (e.regions || [])
      .map((r) => `${r.id}~${r.name || ""}~${(r.rect || r.content_bbox || []).join(",")}`)
      .join("|");
    // element.export is part of the structure: a row add/remove/edit must rebuild.
    return `e:${e.id}|${e.name}|${e.x},${e.y},${e.w},${e.h}|${e.source_w},${e.source_h}|${regions}|${JSON.stringify(e.export || [])}|${JSON.stringify(e.meta || {})}`;
  }
  if (selected.length > 1) return `m:${selected.map((e) => e.id).join(",")}`;
  // Empty state carries a project-export button gated by the top-level visible-screen count.
  return `empty:${visibleScreenCount()}`;
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
  const groupIds = [...state.selectedGroupIds];
  const multiGroup = selected.length === 0 && groupIds.length >= 2;
  if (group) {
    renderGroupInspector(group, root);
  } else if (multiGroup) {
    renderMultiGroup(groupIds, root);
  } else if (selected.length === 1) {
    if (selected[0].type === "text") renderTextElement(selected[0], root);
    else renderElement(selected[0], root);
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
