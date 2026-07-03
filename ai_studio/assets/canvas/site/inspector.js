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
  fileUrl,
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
  addPlateFromFile,
  alignSelection,
  alphaCutoutBatchFor,
  alphaCutoutFor,
  alphaDualPlateFor,
  alphaDualPlateGenerateFor,
  deleteRegion,
  detectRegionsFor,
  distributeSelection,
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
  selectedNodeIds,
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
  body.appendChild(actions);
}

// Alpha section (T0241 — lead: alpha is not a Regions concern, it gets its own section).
// T0247 (lead): the method choice is EXPLICIT — no "Auto" router in the UI ("я хочу явно
// выбирать"; ops/CLI keep accepting auto additively for the agent). "Key matte" keys the
// element's OWN pixels in place, region-aware (scoped to the selected regions when any
// are selected in region-edit mode, else the whole element). "Dual-plate (generate)" runs
// the automatic T0238 flow — flat-light-bg check -> generated dark plate (codex, minutes)
// -> gate -> ONE NEW cut element beside the source; no region scoping (the pair tool cuts
// the whole plate). Both long-op via the queue + progress toast (mirrors Slice).
function renderAlpha(element, root) {
  const body = collapsible(root, "alpha", "Alpha");
  const alphaRow = document.createElement("div");
  alphaRow.className = "insp-alpha-row";
  const methodSel = document.createElement("select");
  methodSel.className = "insp-input";
  for (const [value, label] of [["matte", "Key matte"], ["dual", "Dual-plate"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = label;
    methodSel.appendChild(option);
  }
  const alphaBtn = document.createElement("button");
  alphaBtn.type = "button";
  alphaBtn.className = "insp-btn insp-alpha-btn";
  // The run button says what the chosen method will actually do (the matte label also
  // carries the live region-scope count).
  const relabel = () => {
    if (methodSel.value === "dual") {
      alphaBtn.textContent = "Generate";
      return;
    }
    const ids = selectedRegionIdsFor(element);
    alphaBtn.textContent = ids.length ? `Alpha cutout (${ids.length})` : "Alpha cutout";
  };
  relabel();
  methodSel.addEventListener("change", relabel);
  // Recompute the region scope at click time so an overlay-updated selection is honored.
  alphaBtn.addEventListener("click", () => {
    if (methodSel.value === "dual") {
      alphaDualPlateGenerateFor(element.id, alphaBtn);
      return;
    }
    const ids = selectedRegionIdsFor(element);
    alphaCutoutFor(element.id, "matte", ids.length ? ids : undefined, alphaBtn);
  });
  alphaRow.append(field("Method", methodSel), alphaBtn);
  body.appendChild(alphaRow);

  renderAlphaPlates(element, body);
}

// Plate thumbnails (T0238 — the AUTOMATIC dual-plate-generate flow's fixed light/dark
// roles; any future writer of the same additive `meta.alpha.plates` shape shows here too).
// Compact: one row per plate — a small thumbnail (the file is served over the existing
// project-files route, same as every other element image), its role label, and an
// "Add to canvas" button that mints a normal journaled element from that plate's STORED
// file (no re-upload — addPlateFromFile / POST images-from-file reuses the content-
// addressed src). Placed stacked to the right of the source element so it never lands on
// top of the cut result that already sits there.
function renderAlphaPlates(element, root) {
  const plates = element.meta && element.meta.alpha && Array.isArray(element.meta.alpha.plates) ? element.meta.alpha.plates : null;
  if (!plates || !plates.length) return;

  const wrap = document.createElement("div");
  wrap.className = "insp-alpha-plates";
  plates.forEach((plate, index) => {
    if (!plate || !plate.src) return;
    const row = document.createElement("div");
    row.className = "insp-plate-row";

    const img = document.createElement("img");
    img.className = "insp-plate-thumb";
    img.src = fileUrl({ src: plate.src });
    img.alt = plate.role || `plate ${index + 1}`;
    img.title = plate.role || `plate ${index + 1}`;

    const label = document.createElement("span");
    label.className = "insp-plate-role";
    label.textContent = plate.role || `plate ${index + 1}`;

    const addBtn = smallBtn("Add to canvas", () => {
      const placement = {
        x: Math.round(Number(element.x) + Number(element.w) + 16),
        y: Math.round(Number(element.y) + index * (Number(element.h) + 16)),
      };
      addPlateFromFile(plate.src, `${element.name} ${plate.role || "plate"}`, placement, addBtn);
    });

    row.append(img, label, addBtn);
    wrap.appendChild(row);
  });
  root.appendChild(wrap);
}

// ---- export (Figma-style rows persisted on the element) ----------------------

// Additive multiplier suggestions for the Size number input's datalist — typing is
// always primary (T0235: two independent UX reviews rated the old preset-select +
// "Custom…" morph bad); a datalist is a hint the input never depends on.
// Per-mode value presets for the Size control's dropdown (lead spec 2026-07-03):
// scale multipliers, and power-of-two pixel sizes for the fixed W/H modes (the
// element's own base dimension is prepended at runtime as the first suggestion).
const SCALE_VALUE_PRESETS = [0.5, 1, 1.5, 2, 4];
const PX_VALUE_PRESETS = [32, 64, 128, 256, 512, 1024, 2048];

// One shared floating preset menu (a REAL menu, not a <datalist> — Chrome filters
// datalist options by the typed prefix, which is exactly the "only 1x selectable"
// trap this control started from). Closes on pick, Escape, or any outside press.
let presetMenuEl = null;
function closePresetMenu() {
  if (!presetMenuEl) return;
  presetMenuEl.remove();
  presetMenuEl = null;
}
function openPresetMenu(anchor, values, onPick) {
  closePresetMenu();
  const menu = document.createElement("div");
  menu.className = "insp-preset-menu";
  for (const preset of values) {
    const item = document.createElement("button");
    item.type = "button";
    item.textContent = String(preset);
    // mousedown would steal focus from the number input and fire its focusout
    // settle before the pick lands — swallow it, act on click.
    item.addEventListener("mousedown", (event) => event.preventDefault());
    item.addEventListener("click", () => {
      closePresetMenu();
      onPick(preset);
    });
    menu.appendChild(item);
  }
  const rect = anchor.getBoundingClientRect();
  menu.style.left = `${Math.round(rect.right - 78)}px`;
  menu.style.top = `${Math.round(rect.bottom + 2)}px`;
  document.body.appendChild(menu);
  presetMenuEl = menu;
  const onDown = (event) => {
    if (presetMenuEl && !presetMenuEl.contains(event.target) && event.target !== anchor) closePresetMenu();
    if (!presetMenuEl) document.removeEventListener("mousedown", onDown, true);
  };
  document.addEventListener("mousedown", onDown, true);
}
const EXPORT_FORMATS = ["png", "jpg", "webp"];
const EXPORT_BASES = ["source", "canvas"];

// The element's export rows, or the implicit single 1x-png default shown for a
// layer with no settings yet (matches Figma + the op's default).
function exportRowsOf(element) {
  const rows = Array.isArray(element.export) ? element.export : [];
  return rows.length ? rows.map((row) => ({ ...row })) : [{ scale: "1x", format: "png", resample: "lanczos" }];
}

// A row's effective base ("source" is the default/absent value — mirrors ops.mjs).
function rowBase(row) {
  // Canvas is the DEFAULT base (lead 2026-07-03): absent/anything-but-"source" = canvas.
  return row && row.base === "source" ? "source" : "canvas";
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

// Scale-token parser (mirrors ops.mjs parseScaleSpec; kept trivially small — the op
// stays the authority, this only feeds the UI's live readout + unit conversion).
function parseScaleToken(token) {
  const text = String(token == null ? "" : token).trim().toLowerCase();
  let match = /^(\d+(?:\.\d+)?)x?$/.exec(text);
  if (match && Number(match[1]) > 0) return { kind: "mul", value: Number(match[1]) };
  match = /^(\d+(?:\.\d+)?)(w|h)$/.exec(text);
  if (match && Number(match[1]) > 0) return { kind: match[2], value: Number(match[1]) };
  return { kind: "mul", value: 1 }; // unparseable: display as 1x, ops will complain on commit
}

// Mirror of ops.mjs resolveExportScale. `sw`/`sh` are the row's BASE dims (T0235):
// the caller picks source_w/h or the live element w/h before calling this — the
// function itself stays base-agnostic, just like the op.
function resolveScaleToken(spec, sw, sh) {
  if (!(sw > 0) || !(sh > 0)) return null;
  if (spec.kind === "mul") return { w: Math.max(1, Math.round(sw * spec.value)), h: Math.max(1, Math.round(sh * spec.value)) };
  if (spec.kind === "w") return { w: Math.max(1, Math.round(spec.value)), h: Math.max(1, Math.round(sh * (spec.value / sw))) };
  return { w: Math.max(1, Math.round(sw * (spec.value / sh))), h: Math.max(1, Math.round(spec.value)) };
}

// Export Size control (T0235 redesign — synthesized from two independent UX reviews
// that rated the old mode-select + preset-select + "Custom…" morph bad). ONE composite
// line: [number][unit ×|W|H]. Typing is primary; the unit select is LOCAL UI-only state
// that converts the DISPLAYED number to the output-equivalent when switched — it commits
// nothing by itself ("unit browsing = zero journal entries"). A commit fires once a value
// SETTLES (Enter / the number's native `change` / focusout of the whole composite), and
// only when the resolved output pixels actually differ from what's currently stored — so
// a plain unit switch (same output, different token string) never writes anything either.
// Returns { control, out }: `control` is the composite line for the field() row; `out` is
// the separate full-width result-readout line the caller mounts under it.
function scaleInput(element, row, commit) {
  // The row's BASE dims (T0235): "canvas" resolves against the element's CURRENT
  // on-canvas size; "source" (default) against the original source pixels — same
  // dims ops.mjs's exportElements picks at export time.
  const sw = rowBase(row) === "canvas" ? Number(element.w) || 0 : Number(element.source_w) || Number(element.w) || 0;
  const sh = rowBase(row) === "canvas" ? Number(element.h) || 0 : Number(element.source_h) || Number(element.h) || 0;
  const storedSpec = parseScaleToken(row.scale == null ? "1x" : row.scale);
  const storedResolved = resolveScaleToken(storedSpec, sw, sh);

  let unit = storedSpec.kind; // "mul" | "w" | "h" — LOCAL UI-only state
  let value = storedSpec.value; // the number shown, in the CURRENT unit
  let lastSettledDims = storedResolved; // dedupes a same-gesture double-fire (change + focusout)

  const control = document.createElement("div");
  control.className = "insp-size-ctl";

  const number = document.createElement("input");
  number.type = "number";
  number.step = "any";
  number.min = "0";
  number.className = "insp-input insp-size-num";

  const unitSelect = document.createElement("select");
  unitSelect.className = "insp-input insp-size-unit";
  for (const [key, label] of [["mul", "×"], ["w", "W"], ["h", "H"]]) {
    const option = document.createElement("option");
    option.value = key;
    option.textContent = label;
    unitSelect.appendChild(option);
  }

  const out = document.createElement("div");
  out.className = "insp-size-out";

  const specNow = () => ({ kind: unit, value });
  const resolvedNow = () => resolveScaleToken(specNow(), sw, sh);
  const tokenNow = () => (unit === "mul" ? `${value}x` : `${Math.max(1, Math.round(value))}${unit}`);

  const syncOut = (dims) => {
    out.textContent = dims ? `= ${dims.w} × ${dims.h} px` : "";
  };
  // Per-mode preset values for the ▾ menu (lead spec): scale multipliers, or pixel
  // sizes led by the element's CURRENT on-canvas size — "я поскейлил арт и хочу на
  // выходе размер как на канвасе" (lead round 3; NOT the source size).
  const presetsNow = () => {
    if (unit === "mul") return SCALE_VALUE_PRESETS;
    const axisCanvas = Math.round(Number(unit === "w" ? element.w : element.h));
    return axisCanvas > 0 ? [axisCanvas, ...PX_VALUE_PRESETS.filter((px) => px !== axisCanvas)] : PX_VALUE_PRESETS;
  };

  number.value = String(value);
  unitSelect.value = unit;
  syncOut(storedResolved);

  // Unit switch: LOCAL state only. Converts the DISPLAYED number to the output-equivalent
  // (same pixels, different token) — commits nothing.
  unitSelect.addEventListener("change", () => {
    const nextUnit = unitSelect.value;
    if (nextUnit === unit) return;
    const dims = resolvedNow(); // current output under the OLD unit/value
    if (dims) {
      if (nextUnit === "mul") value = sw > 0 ? Math.round((dims.w / sw) * 10000) / 10000 : value;
      else if (nextUnit === "w") value = dims.w;
      else value = dims.h;
    }
    unit = nextUnit;
    number.value = String(value);
    syncOut(resolvedNow());
  });

  number.addEventListener("input", () => {
    const raw = Number(number.value);
    if (Number.isFinite(raw) && raw > 0) {
      value = raw;
      syncOut(resolvedNow());
    } else {
      syncOut(null);
    }
  });

  // Settle: fires the ONE commit for this gesture, and only when the resolved output
  // pixels differ from what's already stored (a pure unit switch/re-display never
  // qualifies — "unit browsing = zero entries"). `lastSettledDims` also dedupes the
  // synchronous double-fire from `change` immediately followed by `focusout`.
  const settle = () => {
    const dims = resolvedNow();
    if (!dims || !Number.isFinite(value) || !(value > 0)) return;
    if (lastSettledDims && dims.w === lastSettledDims.w && dims.h === lastSettledDims.h) return;
    lastSettledDims = dims;
    commit({ scale: tokenNow() });
  };

  number.addEventListener("change", settle);
  number.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      settle();
      number.blur();
      focusStage();
    } else if (event.key === "Escape") {
      event.preventDefault();
      unit = storedSpec.kind;
      value = storedSpec.value;
      number.value = String(value);
      unitSelect.value = unit;
      syncOut(storedResolved);
      number.blur();
      focusStage();
    }
  });
  // Focusout of the WHOLE composite (not just tabbing between the number and the unit
  // select — that stays internal) is the third settle point.
  control.addEventListener("focusout", (event) => {
    if (control.contains(event.relatedTarget)) return;
    settle();
  });

  // Preset dropdown (▾): a real always-full menu of per-mode values; picking one
  // commits immediately (a pick IS a settled choice).
  const presetBtn = document.createElement("button");
  presetBtn.type = "button";
  presetBtn.className = "insp-size-preset";
  presetBtn.title = "Preset sizes";
  presetBtn.textContent = "▾";
  presetBtn.addEventListener("click", () => {
    openPresetMenu(presetBtn, presetsNow(), (picked) => {
      value = picked;
      number.value = String(picked);
      syncOut(resolvedNow());
      settle();
    });
  });

  // MODE FIRST, then the value (lead spec round 2): [×|W|H][number][▾].
  control.append(unitSelect, number, presetBtn);
  return { control, out };
}

// Export base — card-level segmented [Source | Canvas] (T0235; NOT per row). Reflects
// the rows' current base (mixed rows -> majority, ties -> the first row's value) and
// writes the clicked value to EVERY row in ONE setExportRows call (one journal entry).
// Dimmed (still visible/clickable) when no row is a multiplier (×) — a fixed W/H row's
// PRIMARY axis ignores base, so the toggle has little to show for itself there.
function baseSegmented(element, rows) {
  const counts = { source: 0, canvas: 0 };
  for (const row of rows) counts[rowBase(row)] += 1;
  const active = counts.canvas > counts.source ? "canvas" : counts.source > counts.canvas ? "source" : rowBase(rows[0]);
  const hasMultiplierRow = rows.some((row) => parseScaleToken(row.scale == null ? "1x" : row.scale).kind === "mul");

  const wrap = document.createElement("div");
  wrap.className = "insp-segmented";
  if (!hasMultiplierRow) {
    wrap.classList.add("dim");
    wrap.title = "Base only matters for a multiplier (×) row — every row here targets a fixed W/H";
  } else {
    wrap.title = "Export at the source image's pixels, or at the element's current on-canvas size";
  }
  for (const value of EXPORT_BASES) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = value === active ? "insp-seg-btn primary" : "insp-seg-btn";
    button.textContent = value === "source" ? "Source" : "Canvas";
    button.addEventListener("click", () => {
      if (value === active) return;
      setExportRows(element.id, rows.map((row) => ({ ...row, base: value })));
    });
    wrap.appendChild(button);
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
  const size = scaleInput(element, row, commit);
  head.appendChild(field("Size", size.control));
  head.appendChild(size.out);
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

// The Export section at the BOTTOM of the inspector: the card-level Base toggle, a list
// of rows, "+ Add export setting", and an Export button labeled by the target. Edits
// persist per element through setExportSettings (journaled/undoable). The destination is
// chosen in the save-file dialog at export time (T0229), so there is no destination hint
// line.
function renderExport(element, root) {
  const rows = exportRowsOf(element);
  const body = collapsible(root, "export", "Export");

  body.appendChild(baseSegmented(element, rows));

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

// ---- align / distribute (T0232 increment 1) -----------------------------------
//
// One Align row (6 align keys + a Distribute row of 2 keys, Figma layout) shared by every
// selection context that can drive it: a multi-selection (2+ nodes -> the selection's
// union bbox), a multi-GROUP selection, and a SINGLE node that lives inside a parent group
// (align-to-frame — "center this widget inside the screen"). Each button is ONE API call
// -> ONE journaled op (alignNodes/distributeNodes) -> one undo restores the whole gesture.
// `reference` is left to the op's "auto" default in every caller here — auto already
// resolves both the 2+ union-bbox case and the 1-node-in-a-group parent-frame case, so
// the page never has to pick a mode itself.
// Pictographic SVG glyphs (T0245 — two-Opus review synthesis: letters/arrows read as
// unreadable noise, "по иконкам и буквам вообще ничего не понятно"). Consensus grammar
// of Figma/Illustrator/Affinity/Sketch/PowerPoint/Penpot: align = thin anchor line at the
// target edge/center + two rounded bars of unequal length flush to it; distribute = three
// equal bars with equal gaps, no line. fill="currentColor" (theming-free), 16x16 viewBox.
const ALIGN_BUTTONS = [
  [
    "left",
    '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="1.5" y="2" width="1.4" height="12" rx=".7"/><rect x="3.6" y="4" width="10" height="3" rx="1.5"/><rect x="3.6" y="9" width="6.5" height="3" rx="1.5"/></svg>',
    "Align left",
  ],
  [
    "hcenter",
    '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="7.3" y="2" width="1.4" height="12" rx=".7"/><rect x="3" y="4" width="10" height="3" rx="1.5"/><rect x="4.75" y="9" width="6.5" height="3" rx="1.5"/></svg>',
    "Align horizontal centers",
  ],
  [
    "right",
    '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="13.1" y="2" width="1.4" height="12" rx=".7"/><rect x="2.4" y="4" width="10" height="3" rx="1.5"/><rect x="5.9" y="9" width="6.5" height="3" rx="1.5"/></svg>',
    "Align right",
  ],
  [
    "top",
    '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="2" y="1.5" width="12" height="1.4" rx=".7"/><rect x="4" y="3.6" width="3" height="10" rx="1.5"/><rect x="9" y="3.6" width="3" height="6.5" rx="1.5"/></svg>',
    "Align top",
  ],
  [
    "vcenter",
    '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="2" y="7.3" width="12" height="1.4" rx=".7"/><rect x="4" y="3" width="3" height="10" rx="1.5"/><rect x="9" y="4.75" width="3" height="6.5" rx="1.5"/></svg>',
    "Align vertical centers",
  ],
  [
    "bottom",
    '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="2" y="13.1" width="12" height="1.4" rx=".7"/><rect x="4" y="2.4" width="3" height="10" rx="1.5"/><rect x="9" y="5.9" width="3" height="6.5" rx="1.5"/></svg>',
    "Align bottom",
  ],
];
const DISTRIBUTE_BUTTONS = [
  [
    "h",
    '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="2" y="3" width="2.5" height="10" rx="1.25"/><rect x="6.75" y="3" width="2.5" height="10" rx="1.25"/><rect x="11.5" y="3" width="2.5" height="10" rx="1.25"/></svg>',
    "Distribute horizontally (needs 3+ objects)",
  ],
  [
    "v",
    '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="3" y="2" width="10" height="2.5" rx="1.25"/><rect x="3" y="6.75" width="10" height="2.5" rx="1.25"/><rect x="3" y="11.5" width="10" height="2.5" rx="1.25"/></svg>',
    "Distribute vertically (needs 3+ objects)",
  ],
];

function renderAlignSection(nodeIds, root) {
  // Mode badge (T0245 UX critique): surfaces the invisible reference switch — auto
  // resolves to the selection's union bbox at 2+ nodes, else the single node's parent
  // frame. Muted like the Regions count badge's structure, not its cyan emphasis.
  const badge = document.createElement("span");
  badge.className = "insp-align-badge";
  badge.textContent = nodeIds.length >= 2 ? "to selection" : "to frame";
  const body = collapsible(root, "align", "Align", badge);

  const alignCaption = document.createElement("div");
  alignCaption.className = "insp-align-caption";
  alignCaption.textContent = "Align";
  body.appendChild(alignCaption);

  const alignRow = document.createElement("div");
  alignRow.className = "insp-align-row";
  for (const [align, svg, title] of ALIGN_BUTTONS) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "insp-align-btn";
    btn.innerHTML = svg; // trusted static string, not user data
    btn.title = title;
    btn.setAttribute("aria-label", title);
    btn.addEventListener("click", () => alignSelection(nodeIds, align));
    alignRow.appendChild(btn);
  }
  body.appendChild(alignRow);

  const distCaption = document.createElement("div");
  distCaption.className = "insp-align-caption";
  distCaption.textContent = "Distribute";
  body.appendChild(distCaption);

  const distRow = document.createElement("div");
  distRow.className = "insp-align-row";
  const canDistribute = nodeIds.length >= 3;
  for (const [axis, svg, title] of DISTRIBUTE_BUTTONS) {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "insp-align-btn";
    btn.innerHTML = svg; // trusted static string, not user data
    btn.title = title;
    btn.setAttribute("aria-label", title);
    btn.disabled = !canDistribute;
    btn.addEventListener("click", () => distributeSelection(nodeIds, axis));
    distRow.appendChild(btn);
  }
  body.appendChild(distRow);

  // Always-visible hint (not tooltip-only — a disabled button's title is not
  // discoverable) telling the lead why the distribute row is greyed out.
  if (!canDistribute) {
    const hint = document.createElement("div");
    hint.className = "insp-region-hint";
    hint.textContent = "Select 3+ objects to distribute.";
    body.appendChild(hint);
  }
}

// ---- element / group / multi views -------------------------------------------

function renderElement(element, root) {
  const name = field("Name", textInput(element.name, (next) => renameElement(element.id, next)));
  name.classList.add("insp-name");
  root.appendChild(name);

  const layout = collapsible(root, "layout", "Position & Size");
  layout.appendChild(boxGrid(element, (patch) => patchElementBox(element.id, patch)));
  layout.appendChild(readOnly("Source", `${element.source_w || element.w} x ${element.source_h || element.h}`));

  // Single node inside a parent group (screen or widget): align-to-frame — the "center
  // this widget inside the screen" case (Figma-auto reference, T0232 increment 1).
  if (element.groupId) renderAlignSection([element.id], root);

  renderRegions(element, root);
  renderAlpha(element, root);

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

  // Single node inside a parent group: align-to-frame (same rule as an image element).
  if (element.groupId) renderAlignSection([element.id], root);

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

  // A nested group (a widget frame inside a screen) aligns to ITS parent group's frame —
  // same Figma-auto rule as a single element inside a group.
  if (group.parentId) renderAlignSection([group.id], root);

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

  renderAlignSection(groupIds, root);

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
//
// Dual-plate cutout (T0237): when the selection is EXACTLY 2 images, a second button
// offers ops.alphaDualPlate — the SAME art on a white plate + a black plate (either
// order) -> ONE NEW cut element (non-destructive; both plates stay on the canvas). Same
// runLongOp treatment; disabled/spun independently from the batch-alpha button above it.
function renderMultiAlpha(selected, root) {
  const body = collapsible(root, "multialpha", "Alpha");
  // T0247: no "Auto" router in the UI — the batch is always an explicit Key matte (the
  // only batchable method; dual-plate generation is a per-element action on the single-
  // element Alpha section). With one method there is nothing to select, so the row is
  // just the labeled run button.
  const alphaBtn = document.createElement("button");
  alphaBtn.type = "button";
  alphaBtn.className = "insp-btn insp-alpha-btn";
  alphaBtn.textContent = `Key matte: apply to ${selected.length} images`;
  alphaBtn.addEventListener("click", () => {
    alphaCutoutBatchFor(selected.map((element) => element.id), "matte", alphaBtn);
  });
  body.appendChild(alphaBtn);

  if (selected.length === 2) {
    const dualBtn = document.createElement("button");
    dualBtn.type = "button";
    dualBtn.className = "insp-btn insp-alpha-btn";
    dualBtn.textContent = "Dual-plate cutout";
    dualBtn.addEventListener("click", () => {
      alphaDualPlateFor(selected.map((element) => element.id), dualBtn);
    });
    body.appendChild(dualBtn);
  }
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

  renderAlignSection(selectedNodeIds(), root);

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
    return `g:${group.id}|${group.name}|${group.x},${group.y},${group.w},${group.h}|${group.visible !== false}|${group.clip === true}|${memberElements(group.id).length}|${JSON.stringify(group.background || null)}|${group.parentId || ""}`;
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
      return `t:${e.id}|${e.name}|${e.x},${e.y},${e.w},${e.h}|${e.content}|${JSON.stringify(e.style || {})}|${e.groupId || ""}`;
    }
    const regions = (e.regions || [])
      .map((r) => `${r.id}~${r.name || ""}~${(r.rect || r.content_bbox || []).join(",")}`)
      .join("|");
    // element.export is part of the structure: a row add/remove/edit must rebuild.
    return `e:${e.id}|${e.name}|${e.x},${e.y},${e.w},${e.h}|${e.source_w},${e.source_h}|${regions}|${JSON.stringify(e.export || [])}|${JSON.stringify(e.meta || {})}|${e.groupId || ""}`;
  }
  // Group ids that ride along with a loose-element multi-selection are folded in too (the
  // Align row's nodeIds come from the FULL selectedNodeIds(), not just `selected`).
  if (selected.length > 1) return `m:${selected.map((e) => e.id).join(",")}|g:${[...state.selectedGroupIds].join(",")}`;
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
