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
  selectOnly,
  selectRegion,
  selectRegionRange,
  setStatus,
  state,
  VIDEO_ANIM_FROZEN,
} from "./app.js";
import {
  addPlateFromFile,
  alignSelection,
  alphaCutoutBatchFor,
  alphaCutoutFor,
  alphaDualPlateFor,
  alphaDualPlateGenerateFor,
  animateElementFromTextAction,
  bakeFiltersBatchFor,
  bakeFiltersFor,
  deleteRegion,
  detectRegionsFor,
  distributeSelection,
  expandRecipePromptAction,
  exportElementIds,
  exportProjectAction,
  extractElementAction,
  fitGroupAction,
  generateAnimFromCardAction,
  generateFromRecipeAction,
  packPreviewAction,
  packSliceAction,
  patchAnimAction,
  patchElementBox,
  patchElementsBatch,
  patchGroupBox,
  patchNoteStyle,
  patchRecipeAction,
  patchStyleAction,
  patchTextElement,
  promoteRecipeAction,
  promoteStyleAction,
  renameElement,
  renameGroup,
  renameRegion,
  renderScreen,
  selectedNodeIds,
  setElementAnimationAction,
  setExportRows,
  setGroupBackground,
  setGroupClip,
  setGroupScreen,
  setGroupsShared,
  setGroupVisible,
  setNoteBackground,
  setSlice9Action,
  sliceRegionsFor,
  toggleElementFlip,
} from "./actions.js";
import { childrenOf, descendantsOf, isNodeTransformed } from "../tree.mjs";
import { validateAnimation } from "../animation.mjs";
import { NOTE_BACKGROUND_PRESETS } from "../fonts.mjs";
import { fontFamilies, fontWeights } from "./fonts.js";
import { openContextMenu } from "./context_menu.js";
import { inlineEdit } from "./inline.js";
import { activeCleanupDialogTool, openCleanupDialog, syncCleanupDialog } from "./cleanup_dialog.js";
import { clearCleanupPreview, getCleanupPreview, isAnimationPreviewing, toggleAnimationPreview } from "./workspace.js";

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

// Multi-line variant of textInput for the Recipe card's Prompt field. Commits on change
// (blur/click-away) like textInput, plus Enter (Shift+Enter inserts a newline instead —
// a prompt is often more than one line, unlike the rename/scale fields textInput guards).
// Unlike textInput, an EMPTY commit is allowed: a cleared prompt is a valid draft state
// (ops.patchRecipe's prompt validation only rejects a non-string, not an empty one).
function textareaInput(value, onCommit) {
  const textarea = document.createElement("textarea");
  textarea.className = "insp-input";
  textarea.rows = 3;
  textarea.value = value == null ? "" : String(value);
  const commit = () => {
    const next = textarea.value;
    if (next === String(value == null ? "" : value)) return; // unchanged: no commit
    onCommit(next);
  };
  textarea.addEventListener("change", commit);
  textarea.addEventListener("keydown", (event) => {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault();
      textarea.blur();
      focusStage();
    } else if (event.key === "Escape") {
      event.preventDefault();
      textarea.value = value == null ? "" : String(value);
      textarea.blur();
      focusStage();
    }
  });
  return textarea;
}

// `step` (optional, T0233) sets the input's step attribute (e.g. 0.1 for the
// Slice-9 Scale field) — every other caller keeps the browser default (1).
function numberInput(value, onCommit, { step } = {}) {
  const input = document.createElement("input");
  input.type = "number";
  input.className = "insp-input num";
  input.value = Number(value) || 0;
  if (step) input.step = String(step);
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

// ---- aspect-ratio lock (T0272 — lead: W/H edits ignore the image's proportions) -----
//
// A "keep proportions" toggle sits between the W/H fields in boxGrid below, so every
// panel that reuses boxGrid (element, group, note) gets it for free. Locked: editing W
// or H recomputes the OTHER field from the box's CURRENT ratio at edit time and commits
// both in the SAME onPatch call the field already made (one undo, per the module's
// one-gesture-one-entry law). Unlocked: exactly the pre-T0272 free-edit behavior. The
// lock is page-only view state (never journaled), keyed by node id so a manual toggle
// survives re-renders of the SAME selection but resets to the computed default the
// moment the selection moves to a different node (including back to a node seen before —
// re-selecting IS a fresh "selection changes" event).
let aspectLockNodeId = null;
let aspectLockValue = true;

const ASPECT_RATIO_EPSILON = 0.01; // ~1% — absorbs integer-pixel rounding between w/h and source_w/source_h

// Pure: the default lock state for a box with optional source_w/source_h. ON when there
// is no source to compare against (groups/notes carry neither field; text never reaches
// boxGrid — renderTextElement's size is read-only), or the box's CURRENT ratio already
// matches the source ratio within tolerance. OFF when the box is already stretched/
// squashed relative to its source.
export function defaultAspectLock(box) {
  const sw = Number(box && box.source_w);
  const sh = Number(box && box.source_h);
  if (!(sw > 0) || !(sh > 0)) return true; // no source dims: default ON (groups, notes)
  const w = Number(box && box.w);
  const h = Number(box && box.h);
  if (!(w > 0) || !(h > 0)) return true; // degenerate current box: nothing to compare
  const sourceRatio = sw / sh;
  return Math.abs(w / h - sourceRatio) <= sourceRatio * ASPECT_RATIO_EPSILON;
}

// Pure: the OTHER dimension for a locked W/H edit, from the box's ratio at edit time.
// Returns null on a degenerate ratio (w or h <= 0) or a non-finite/non-positive new
// value — the caller falls back to a free single-field edit in that case (never divides
// by zero, never returns a non-finite number for the ops layer to reject).
export function linkedDimension(currentW, currentH, editedKey, newValue) {
  const w = Number(currentW);
  const h = Number(currentH);
  if (!(w > 0) || !(h > 0)) return null;
  const value = Number(newValue);
  if (!Number.isFinite(value) || !(value > 0)) return null;
  if (editedKey === "w") return Math.round(value * (h / w));
  if (editedKey === "h") return Math.round(value * (w / h));
  return null;
}

// Resolves the current lock value for `box`, recomputing the default the moment the
// selection has moved to a different node id since the last call (a manual toggle
// persists across re-renders of the SAME node only).
function aspectLockFor(box) {
  if (box.id !== aspectLockNodeId) {
    aspectLockNodeId = box.id;
    aspectLockValue = defaultAspectLock(box);
  }
  return aspectLockValue;
}

// The Position & Size box: X/Y in a plain 2-col grid, then a W/H row with the
// aspect-lock toggle wedged between the two fields (see aspectLockFor/linkedDimension
// above). Shared by the element, group, and note panels.
function boxGrid(box, onPatch) {
  const wrap = document.createElement("div");
  wrap.className = "insp-boxgrid";

  const xyGrid = document.createElement("div");
  xyGrid.className = "insp-grid";
  xyGrid.appendChild(field("X", numberInput(box.x, (value) => onPatch({ x: value }))));
  xyGrid.appendChild(field("Y", numberInput(box.y, (value) => onPatch({ y: value }))));
  wrap.appendChild(xyGrid);

  let locked = aspectLockFor(box);
  const commitLinked = (editedKey, otherKey) => (value) => {
    if (locked) {
      const linked = linkedDimension(box.w, box.h, editedKey, value);
      if (linked != null) {
        onPatch({ [editedKey]: value, [otherKey]: linked });
        return;
      }
    }
    onPatch({ [editedKey]: value });
  };

  const whRow = document.createElement("div");
  whRow.className = "insp-wh-row";
  whRow.appendChild(field("W", numberInput(box.w, commitLinked("w", "h"))));

  const lockBtn = document.createElement("button");
  lockBtn.type = "button";
  lockBtn.title = "Сохранять пропорции";
  lockBtn.setAttribute("aria-label", "Keep W/H proportions linked");
  const syncLockBtn = () => {
    lockBtn.className = locked ? "insp-aspect-lock active" : "insp-aspect-lock";
    lockBtn.setAttribute("aria-pressed", locked ? "true" : "false");
    lockBtn.textContent = locked ? "🔗" : "🔓"; // 🔗🔓
  };
  syncLockBtn();
  lockBtn.addEventListener("click", () => {
    locked = !locked;
    aspectLockNodeId = box.id;
    aspectLockValue = locked;
    syncLockBtn();
  });
  whRow.appendChild(lockBtn);
  whRow.appendChild(field("H", numberInput(box.h, commitLinked("h", "w"))));
  wrap.appendChild(whRow);

  return wrap;
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

// ---- prompt modal (T0250) -----------------------------------------------------
//
// A full-viewport modal for reading/editing a long prompt string (lead: "промпт тяжело
// вот так читать" — the 3-row Recipe textarea is too small to read a real prompt).
// Reused for BOTH the Recipe card's LIVE prompt (editable — Save commits through the
// caller's `onSave`, e.g. patchRecipeAction, so it is still just one journal entry) and a
// minted element's FROZEN prompt_snapshot in the Generation section (read-only — pass
// `{ readOnly: true }`; `onSave` is never called). Escape / a click on the dimmed
// overlay / Cancel all close without committing; Ctrl+Enter is a Save shortcut in
// editable mode. Positioned above every other floating layer in this file (preset menu /
// toasts are z-index:40, the context menu is z-index:50/51, the layer-drag-ghost is
// z-index:60) so it can always be opened on top of them.
function openPromptModal(title, initialValue, onSave, { readOnly: viewOnly = false } = {}) {
  const overlay = document.createElement("div");
  overlay.className = "prompt-modal-overlay";

  const panel = document.createElement("div");
  panel.className = "prompt-modal-panel";

  const head = document.createElement("div");
  head.className = "prompt-modal-head";
  head.textContent = title;

  const textarea = document.createElement("textarea");
  textarea.className = "prompt-modal-textarea";
  textarea.value = initialValue == null ? "" : String(initialValue);
  // readOnly, NOT disabled: a disabled textarea refuses focus (so the frozen prompt
  // couldn't be selected/copied, and Escape would land on a non-typing target and fall
  // through to the canvas handler below).
  textarea.readOnly = viewOnly;

  const footer = document.createElement("div");
  footer.className = "prompt-modal-footer";

  const close = () => {
    document.removeEventListener("keydown", onKeydown, true);
    overlay.remove();
  };
  const save = () => {
    if (viewOnly) return;
    // Match the inline textarea's change-event semantics: an unedited Save/Ctrl+Enter
    // is a plain close, never a no-op journal entry. A save whose onSave THROWS (T0260's
    // Animation JSON editor validates JSON.parse + the spec client-side) keeps the modal
    // OPEN and toasts the reason, so the lead fixes the text instead of losing it — every
    // pre-existing caller's onSave never throws, so their close-on-save is unchanged.
    if (textarea.value !== (initialValue == null ? "" : String(initialValue))) {
      try {
        onSave(textarea.value);
      } catch (error) {
        setStatus(error.message, true);
        return;
      }
    }
    close();
  };

  if (viewOnly) {
    footer.appendChild(smallBtn("Close", close));
  } else {
    footer.append(smallBtn("Cancel", close), (() => {
      const btn = smallBtn("Save", save);
      btn.classList.add("primary");
      return btn;
    })());
  }

  // stopPropagation: this capture-phase handler runs before the canvas's window-level
  // keydown (canvas.js) — without it, Escape would ALSO unwind canvas selection/scope
  // behind the closing modal.
  const onKeydown = (event) => {
    if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      close();
    } else if (event.key === "Enter" && event.ctrlKey) {
      event.preventDefault();
      event.stopPropagation();
      save();
    }
  };
  document.addEventListener("keydown", onKeydown, true);
  overlay.addEventListener("mousedown", (event) => {
    if (event.target === overlay) close();
  });

  panel.append(head, textarea, footer);
  overlay.appendChild(panel);
  document.body.appendChild(overlay);
  textarea.focus();
  textarea.setSelectionRange(textarea.value.length, textarea.value.length);
  return overlay;
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

// R7 (T0232 increment 3a, lead-approved refusal): the ONE reason shown wherever a
// region-edit control is grayed out on a rotated/flipped element — mirrors the ops-layer
// refusal message (ops.mjs refuseIfTransformed) and the page's region-edit-entry blocks
// (workspace.js onDblClick, context_menu.js "Edit regions") so every surface explains the
// same thing the same way.
const TRANSFORM_GUARD_REASON = "Rotated/flipped — reset rotation/flip to edit regions or slice.";

// The REGIONS section: a collapsible header + count badge, one compact row per
// region (numbers match the canvas badges), and the entry points now that the
// layers tree no longer duplicates this list: Edit (isolation mode), Detect, Add,
// and Slice (selected in region-edit mode, else all).
function renderRegions(element, root) {
  const regions = element.regions || [];
  const transformed = isNodeTransformed(element);
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
  detectBtn.disabled = transformed;
  btnRow.append(detectBtn);
  actions.appendChild(btnRow);

  const sliceBtn = document.createElement("button");
  sliceBtn.type = "button";
  sliceBtn.className = "primary insp-btn insp-slice-btn";
  const selectedIds = selectedRegionIdsFor(element);
  sliceBtn.disabled = regions.length === 0 || transformed;
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

  if (transformed) {
    const hint = document.createElement("div");
    hint.className = "insp-region-hint";
    hint.textContent = TRANSFORM_GUARD_REASON;
    body.appendChild(hint);
  }
}

// Alpha section (T0241 — lead: alpha is not a Regions concern, it gets its own section).
// T0247 (lead): the method choice is EXPLICIT — no "Auto" router in the UI ("я хочу явно
// выбирать"; ops/CLI keep accepting auto additively for the agent). "Key matte" keys the
// element's OWN pixels in place, region-aware (scoped to the selected regions when any
// are selected in region-edit mode, else the whole element). "CorridorKey (green glow)"
// (T0261) is the neural green-screen matte for soft glow/translucent art — GREEN keys only
// (a non-green key is a loud refusal pointing at Key matte), whole-element (no region scope),
// and a ~15s GPU run behind the same long-op busy toast. "Dual-plate (generate)" runs
// the automatic T0238 flow — flat-light-bg check -> generated dark plate (codex, minutes)
// -> gate -> ONE NEW cut element beside the source; no region scoping (the pair tool cuts
// the whole plate). Both long-op via the queue + progress toast (mirrors Slice).
function renderAlpha(element, root) {
  const transformed = isNodeTransformed(element);
  const body = collapsible(root, "alpha", "Alpha");
  const alphaRow = document.createElement("div");
  alphaRow.className = "insp-alpha-row";
  const methodSel = document.createElement("select");
  methodSel.className = "insp-input";
  // T0261: "CorridorKey (green glow)" is the explicit neural green-screen matte for soft
  // glow/translucent art — green-only, whole-element, and a ~15s GPU run (its own busy toast).
  for (const [value, label] of [["matte", "Key matte"], ["corridorkey", "CorridorKey (green glow)"], ["dual", "Dual-plate"]]) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = label;
    methodSel.appendChild(option);
  }
  const alphaBtn = document.createElement("button");
  alphaBtn.type = "button";
  alphaBtn.className = "insp-btn insp-alpha-btn";
  alphaBtn.disabled = transformed;
  // The run button says what the chosen method will actually do (the matte label also
  // carries the live region-scope count).
  const relabel = () => {
    if (methodSel.value === "dual") {
      alphaBtn.textContent = "Generate";
      return;
    }
    if (methodSel.value === "corridorkey") {
      // CorridorKey is whole-element only (no region scope) and neural (~15s) — say so.
      alphaBtn.textContent = "Alpha cutout (neural ~15s)";
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
    if (methodSel.value === "corridorkey") {
      // Whole-element only — never pass region ids (the op refuses corridorkey+regions loudly).
      alphaCutoutFor(element.id, "corridorkey", undefined, alphaBtn);
      return;
    }
    const ids = selectedRegionIdsFor(element);
    alphaCutoutFor(element.id, "matte", ids.length ? ids : undefined, alphaBtn);
  });
  alphaRow.append(field("Method", methodSel), alphaBtn);
  body.appendChild(alphaRow);

  // R7 (T0232 increment 3a): alphaCutout (single AND the "Dual-plate generate" flow) both
  // read the element's untransformed source pixels — same reason the Regions section grays
  // out Detect/Slice.
  if (transformed) {
    const hint = document.createElement("div");
    hint.className = "insp-region-hint";
    hint.textContent = TRANSFORM_GUARD_REASON;
    body.appendChild(hint);
  }

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

// Cleanup launchers (T0207; UX redesigned 2026-07-04 after the lead's live verify — «кажется
// что это про одно и что это настройки», then his own pick: «почему мы не делаем модальным
// окном?»): Quantize and Denoise are TWO INDEPENDENT destructive one-shot tools; each opens
// its own floating Photoshop-style dialog over the stage (cleanup_dialog.js) with the live
// preview on the canvas itself. ONE dialog at a time (the "only one uncommitted preview"
// rule from the competitor audit), so both launchers are disabled while one is open. The
// dialog owns all controls; this section is just the entry point + the applied-provenance
// line («как откатить обратно») + the shared transform guard (R7: cleanup reads/writes
// source-space pixels, so a rotated/flipped element refuses).
function renderCleanup(element, root) {
  const transformed = isNodeTransformed(element);
  const body = collapsible(root, "cleanup", "Cleanup");

  if (transformed) {
    const hint = document.createElement("div");
    hint.className = "insp-region-hint";
    hint.textContent = TRANSFORM_GUARD_REASON;
    body.appendChild(hint);
  }

  // Rollback answer for an ALREADY-applied cleanup («как откатить обратно»): meta.cleanup
  // is the op's own provenance record — surface it with the undo route.
  if (element.meta && element.meta.cleanup) {
    const applied = element.meta.cleanup;
    const p = applied.params || {};
    const what = applied.tool === "denoise"
      ? `denoise strength ${p.strength}`
      : `quantize ${p.colors} colors${p.dither ? " + dither" : ""}`;
    const appliedLine = document.createElement("div");
    appliedLine.className = "insp-cleanup-report";
    appliedLine.textContent = `Applied: ${what} — Ctrl+Z reverts to the original.`;
    body.appendChild(appliedLine);
  }

  const row = document.createElement("div");
  row.className = "insp-cleanup-actions";
  const openTool = activeCleanupDialogTool();
  const syncLaunchers = (open) => {
    const disabled = transformed || open !== null;
    quantizeBtn.disabled = disabled;
    denoiseBtn.disabled = disabled;
  };
  const launcher = (tool, label) => {
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "insp-btn";
    btn.textContent = label;
    btn.disabled = transformed || openTool !== null;
    btn.addEventListener("click", () => {
      openCleanupDialog(tool, element, {
        // The dialog outlives inspector rebuilds; if THIS row is still on screen when it
        // closes, re-enable in place (a rebuilt row re-derives from activeCleanupDialogTool).
        onClose: () => {
          if (row.isConnected) syncLaunchers(null);
        },
      });
      syncLaunchers(tool);
    });
    return btn;
  };
  const quantizeBtn = launcher("quantize", "Quantize…");
  const denoiseBtn = launcher("denoise", "Denoise…");
  row.append(quantizeBtn, denoiseBtn);
  body.appendChild(row);
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
  // Both axes in ONE gesture (lead: art dropped into a card lands far away — wants one
  // button to center it in the frame). Glyph = both anchor lines crossed + one centered bar.
  [
    "center",
    '<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="7.3" y="2" width="1.4" height="12" rx=".7"/><rect x="2" y="7.3" width="12" height="1.4" rx=".7"/><rect x="4.75" y="5.75" width="6.5" height="4.5" rx="1.5"/></svg>',
    "Center (both axes)",
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

// T0232 increment 2 (R5): a "Reset to source size" affordance for the scale gizmo's
// unavoidable companion — once a sprite has been dragged away from its native pixels,
// there needs to be a one-click way back. Image-only (renderElement is never called for a
// text element — renderTextElement owns that view); commits patchElement({w:source_w,
// h:source_h}) — x/y (top-left) stays fixed, same rule boxGrid's own W/H edits follow. No
// new op: the CLI already covers this via `element-set --w <source_w> --h <source_h>`.
function renderResetToSourceButton(element) {
  const sw = element.source_w || element.w;
  const sh = element.source_h || element.h;
  const button = document.createElement("button");
  button.type = "button";
  button.className = "insp-btn";
  button.textContent = "Reset to source size";
  button.disabled = element.w === sw && element.h === sh;
  button.addEventListener("click", () => patchElementBox(element.id, { w: sw, h: sh }));
  return button;
}

// T0232 increment 3a / T0249 (lead: "не видно есть ли флип, угол не удобно задавать и
// сбрасывать" — you can't SEE whether flip is on, and the angle is awkward to set and
// reset). Rotation (finite degrees, CW about the box center) + flip (additive booleans,
// image-only) both commit through the SAME patchElement path the agent's element-set/
// elements-set use (strict tool parity — the op layer validates and normalizes either
// way). NO rotate handle here — the drag-to-rotate gizmo lives on the canvas; the
// inspector stays the precise-entry path.

// Rotation row: the number input (commits on change/Enter, unchanged) plus quick -90/+90
// step buttons and a Reset — each button is ONE journaled patchElementBox call, so a step
// or reset is a single undo step. Reset is ALWAYS rendered but disabled at rotation 0:
// appearing/disappearing shifted the -90/+90 buttons mid-interaction and the lead's second
// click missed ("нажал на -90 появился сброс кнопка -90 уехала") — reserved space beats a
// hidden dead control. Shared by images (renderTransformControls) and text elements
// (renderTextElement) — rotation applies to both, flip does not.
function renderRotationRow(element) {
  const rotation = Number(element.rotation) || 0;
  const commit = (next) => patchElementBox(element.id, { rotation: next });

  const row = document.createElement("div");
  row.className = "insp-rotation-row";
  row.appendChild(numberInput(rotation, commit));

  const minus90 = smallBtn("−90°", () => commit(rotation - 90));
  minus90.classList.add("insp-rotation-btn");
  minus90.title = "Rotate -90°";
  row.appendChild(minus90);

  const plus90 = smallBtn("+90°", () => commit(rotation + 90));
  plus90.classList.add("insp-rotation-btn");
  plus90.title = "Rotate +90°";
  row.appendChild(plus90);

  const reset = smallBtn("↺", () => commit(0));
  reset.classList.add("insp-rotation-btn", "insp-rotation-reset");
  reset.title = "Reset rotation";
  reset.setAttribute("aria-label", "Reset rotation");
  reset.disabled = rotation === 0;
  row.appendChild(reset);

  return field("Rotation", row);
}

// Flip toggles (image-only): two INDEPENDENT buttons, each reflecting its own on/off
// state — NOT a single-select segmented control (that was exactly the lead's complaint:
// .insp-seg-btn only ever highlights ONE choice, so two simultaneously-on flips read
// identically to zero flips). Active = accent border + tinted fill + aria-pressed; idle =
// a normal button.
function renderFlipRow(element) {
  const row = document.createElement("div");
  row.className = "insp-flip-row";

  const makeFlip = (axis, label, title) => {
    const btn = document.createElement("button");
    btn.type = "button";
    const active = axis === "h" ? !!element.flipH : !!element.flipV;
    btn.className = active ? "insp-flip-btn active" : "insp-flip-btn";
    btn.textContent = label;
    btn.title = title;
    btn.setAttribute("aria-pressed", active ? "true" : "false");
    btn.addEventListener("click", () => toggleElementFlip(element.id, axis));
    return btn;
  };
  row.append(
    makeFlip("h", "Flip H", "Mirror the image left-right"),
    makeFlip("v", "Flip V", "Mirror the image top-bottom"),
  );
  return field("Flip", row);
}

function renderTransformControls(element) {
  const wrap = document.createElement("div");
  wrap.className = "insp-render";
  wrap.appendChild(renderRotationRow(element));
  wrap.appendChild(renderFlipRow(element));
  return wrap;
}

// Position & Size header badge: surfaces rotation/flip state even while the section is
// collapsed (T0249 — the lead's "не видно" complaint applied to the whole section, not
// just the open controls). Rendered ONLY when there's something to show — no dead "0°"
// badge. Text elements have no flip, so they fall through to a rotation-only badge (or
// none) via the same helper.
function transformBadge(element) {
  const rotation = Math.round(Number(element.rotation) || 0);
  const parts = [];
  if (rotation !== 0) parts.push(`${rotation}°`);
  const flips = [];
  if (element.flipH) flips.push("H");
  if (element.flipV) flips.push("V");
  if (flips.length) parts.push(`flip ${flips.join("+")}`);
  if (!parts.length) return null;
  const badge = document.createElement("span");
  badge.className = "insp-align-badge";
  badge.textContent = parts.join(" · ");
  return badge;
}

// ---- Image filters (T0273) ------------------------------------------------------
//
// Non-destructive brightness/saturation/contrast/tint (element.filters) + the existing
// element.opacity — image elements only (renderElement is only reached for a non-text
// element). Every slider drags LIVE: the `input` event mutates the in-memory element
// directly and repaints the canvas (hooks.renderCanvas(), the same view-state-only idiom
// the T0207 cleanup preview uses) — nothing commits until release. `change` (fires once on
// release for a range input) commits ONE patchElement; a release back at the value the
// slider STARTED this gesture at is a deliberate no-op (no journal entry). Each row also
// gets a small "×" reset (mirrors the Rotation row's "↺": always rendered, disabled at the
// default so its position never shifts mid-interaction).

// filters is whole-object-replace (like `style`) — every commit resolves the FULL desired
// object from the element's CURRENT effective values + the one field being changed, so
// changing brightness never silently resets saturation/contrast/tint.
// T0274 "Apply": true when the element carries a filters object (non-default — see
// effectiveFilters below) OR a stored opacity != 1 — the "there is something to bake"
// gate for the Apply button's enabled state. Mirrors ops.hasBakeableFilters (the server
// re-validates regardless, so a stale read here is caught, never silently no-op'd).
function hasBakeableFilters(element) {
  if (!element) return false;
  if (element.filters && typeof element.filters === "object" && Object.keys(element.filters).length) return true;
  const opacity = element.opacity;
  return opacity !== undefined && opacity !== null && Number(opacity) !== 1;
}

function effectiveFilters(element) {
  const stored = element.filters || {};
  const tint = stored.tint || {};
  return {
    brightness: stored.brightness ?? 1,
    saturation: stored.saturation ?? 1,
    contrast: stored.contrast ?? 1,
    tintColor: tint.color || "#000000",
    tintStrength: tint.strength || 0,
  };
}

// View-state-only: mutate the SAME element object the page already renders from and
// repaint immediately — no store write, no journal entry (mirrors setCleanupPreview).
function liveSetFilter(element, patch) {
  const next = { ...effectiveFilters(element), ...patch };
  element.filters = {
    brightness: next.brightness,
    saturation: next.saturation,
    contrast: next.contrast,
    tint: { color: next.tintColor, strength: next.tintStrength },
  };
  hooks.renderCanvas();
}

// ONE patchElement committing the full resolved filters object (the op layer validates +
// drops any field back at its default, so callers never have to pre-normalize).
function commitFilters(element, patch) {
  const next = { ...effectiveFilters(element), ...patch };
  patchElementBox(element.id, {
    filters: {
      brightness: next.brightness,
      saturation: next.saturation,
      contrast: next.contrast,
      tint: { color: next.tintColor, strength: next.tintStrength },
    },
  });
}

// A range row: live-preview via `onInput` on every `input` event, commits via `onCommit`
// on `change` — but ONLY when the released value differs from the value this gesture
// started at (a drag that settles back where it began is a deliberate no-op commit).
function filterSliderRow(label, value, { min, max, step, defaultValue, format, onInput, onCommit, onReset }) {
  const input = document.createElement("input");
  input.type = "range";
  input.min = String(min);
  input.max = String(max);
  input.step = String(step);
  input.value = String(value);
  input.className = "insp-range";

  const out = document.createElement("span");
  out.className = "insp-range-value";
  out.textContent = format(value);

  let committed = value;
  input.addEventListener("input", () => {
    const v = Number(input.value);
    out.textContent = format(v);
    onInput(v);
  });
  input.addEventListener("change", () => {
    const v = Number(input.value);
    if (v === committed) return; // released back where the gesture started — no commit
    committed = v;
    onCommit(v);
  });

  const reset = smallBtn("×", onReset); // ×
  reset.classList.add("insp-filter-reset");
  reset.title = `Reset ${label}`;
  reset.setAttribute("aria-label", `Reset ${label}`);
  reset.disabled = value === defaultValue;

  const row = field(label, input);
  row.append(out, reset);
  return row;
}

function renderFilters(element, root) {
  const body = collapsible(root, "filters", "Filters");

  const opacity = element.opacity === undefined || element.opacity === null ? 1 : Number(element.opacity);
  const pct = (v) => `${Math.round(v * 100)}%`;
  body.appendChild(
    filterSliderRow("Opacity", opacity, {
      min: 0,
      max: 1,
      step: 0.01,
      defaultValue: 1,
      format: pct,
      onInput: (v) => {
        element.opacity = v;
        hooks.renderCanvas();
      },
      onCommit: (v) => patchElementBox(element.id, { opacity: v }),
      onReset: () => patchElementBox(element.id, { opacity: 1 }),
    }),
  );

  const current = effectiveFilters(element);
  body.appendChild(
    filterSliderRow("Brightness", current.brightness, {
      min: 0,
      max: 2,
      step: 0.01,
      defaultValue: 1,
      format: pct,
      onInput: (v) => liveSetFilter(element, { brightness: v }),
      onCommit: (v) => commitFilters(element, { brightness: v }),
      onReset: () => commitFilters(element, { brightness: 1 }),
    }),
  );
  body.appendChild(
    filterSliderRow("Saturation", current.saturation, {
      min: 0,
      max: 2,
      step: 0.01,
      defaultValue: 1,
      format: pct,
      onInput: (v) => liveSetFilter(element, { saturation: v }),
      onCommit: (v) => commitFilters(element, { saturation: v }),
      onReset: () => commitFilters(element, { saturation: 1 }),
    }),
  );
  body.appendChild(
    filterSliderRow("Contrast", current.contrast, {
      min: 0,
      max: 2,
      step: 0.01,
      defaultValue: 1,
      format: pct,
      onInput: (v) => liveSetFilter(element, { contrast: v }),
      onCommit: (v) => commitFilters(element, { contrast: v }),
      onReset: () => commitFilters(element, { contrast: 1 }),
    }),
  );

  body.appendChild(field("Tint color", colorInput(current.tintColor, (next) => commitFilters(element, { tintColor: next }))));
  body.appendChild(
    filterSliderRow("Tint strength", current.tintStrength, {
      min: 0,
      max: 1,
      step: 0.01,
      defaultValue: 0,
      format: pct,
      onInput: (v) => liveSetFilter(element, { tintStrength: v }),
      onCommit: (v) => commitFilters(element, { tintStrength: v }),
      onReset: () => commitFilters(element, { tintStrength: 0 }),
    }),
  );

  // Lead ask (verbatim, mute/dim a background behind a UI mockup): one click to opacity
  // 70% + brightness 70% + saturation 60% — ONE patchElement, one undo. "Reset all" clears
  // both filters and opacity back to defaults, also one patchElement/one undo.
  const presetRow = document.createElement("div");
  presetRow.className = "insp-alpha-row";
  const dimBtn = smallBtn("Приглушить", () =>
    patchElementBox(element.id, { opacity: 0.7, filters: { brightness: 0.7, saturation: 0.6 } }),
  );
  dimBtn.title = "Dim: opacity 70%, brightness 70%, saturation 60%";
  const resetAllBtn = smallBtn("Reset all", () => patchElementBox(element.id, { opacity: 1, filters: null }));
  presetRow.append(dimBtn, resetAllBtn);
  body.appendChild(presetRow);

  // T0274 "Apply" (Photoshop-rasterize semantics, lead: "принял -> получил новый арт ->
  // ползунки снова в 0"): burns the CURRENT filters+opacity into a NEW source file as ONE
  // journaled op, then clears both — the section re-renders with every slider back at its
  // default (the op already cleared the fields; applyMutation's re-render just reflects
  // it). Disabled when there is nothing to bake (mirrors the Regions/Alpha section's
  // disabled-at-nothing-to-do stance).
  const applyRow = document.createElement("div");
  applyRow.className = "insp-alpha-row";
  const applyBtn = document.createElement("button");
  applyBtn.type = "button";
  applyBtn.className = "primary insp-btn";
  applyBtn.textContent = "Apply";
  applyBtn.title = "Bake the current filters + opacity into a new source file, then reset the sliders";
  applyBtn.disabled = !hasBakeableFilters(element);
  applyBtn.addEventListener("click", () => bakeFiltersFor(element.id, applyBtn));
  applyRow.appendChild(applyBtn);
  body.appendChild(applyRow);
}

// Multi-select (T0273): when EVERY selected element is an image, offer the SAME
// "Приглушить" dim preset batched into ONE elements-set call (one undo restores every
// element) — mirrors renderMultiAlpha's "Apply to N images" shape just above it.
function renderMultiFilters(selected, root) {
  const body = collapsible(root, "multifilters", "Filters");
  const dimBtn = document.createElement("button");
  dimBtn.type = "button";
  dimBtn.className = "insp-btn insp-alpha-btn";
  dimBtn.textContent = `Приглушить: apply to ${selected.length} images`;
  dimBtn.title = "Dim: opacity 70%, brightness 70%, saturation 60%";
  dimBtn.addEventListener("click", () => {
    patchElementsBatch(
      selected.map((element) => element.id),
      { opacity: 0.7, filters: { brightness: 0.7, saturation: 0.6 } },
      `Dimmed ${selected.length} image(s).`,
    );
  });
  body.appendChild(dimBtn);

  // T0274 "Apply" batch: only enabled when EVERY selected image actually has something to
  // bake (the batch op validates atomically — one element with nothing to bake would
  // refuse the WHOLE batch — so the button front-loads that same gate for a useful
  // disabled state instead of a click that always errors).
  const applyBtn = document.createElement("button");
  applyBtn.type = "button";
  applyBtn.className = "insp-btn insp-alpha-btn";
  applyBtn.textContent = `Apply filters: apply to ${selected.length} images`;
  applyBtn.title = "Bake each image's current filters + opacity into a new source file, then reset its sliders";
  applyBtn.disabled = !selected.every((element) => hasBakeableFilters(element));
  applyBtn.addEventListener("click", () => {
    bakeFiltersBatchFor(selected.map((element) => element.id), applyBtn);
  });
  body.appendChild(applyBtn);
}

// ---- Slice-9 (T0233 Packet 2) --------------------------------------------------
//
// Image-only inspector section (renderElement is only reached for a non-text
// element — text goes through renderTextElement instead — so there is no extra
// type gate here, same convention renderCleanup/renderAlpha already follow).
// Placed after Cleanup, before Extracted prompts (renderElement below).

const SLICE9_DEFAULT_CAP = 24;

// A sensible starting inset for the "Enable" draft: 1/4 of the smaller source
// dimension, FLOORED (never rounded up) and capped at 24px, so the default itself
// is always a valid setSlice9 call no matter how small the source image is —
// 2*floor(min/4) <= min/2 < min, so left+right/top+bottom always stay under the
// source dimension, even for a tiny source.
function defaultSlice9Inset(sourceW, sourceH) {
  const min = Math.max(0, Math.min(Number(sourceW) || 0, Number(sourceH) || 0));
  return Math.max(0, Math.min(SLICE9_DEFAULT_CAP, Math.floor(min / 4)));
}

// Absent element.slice9: four DRAFT number inputs (prefilled with
// defaultSlice9Inset, NOT yet committed) + a Scale draft (default 1) + an Enable
// button that commits all five as ONE setSlice9 op (one journal entry). Present:
// every field is LIVE-bound via the shared numberInput commit-on-change/Enter
// pattern — each edit is its own setSlice9 op (a settings tweak, not a drag
// gesture, so a per-field journal entry is fine, same stance boxGrid's X/Y/W/H
// takes) — plus a Clear button (insets: null). The op's own loud validation
// (a corner pair that would consume the source axis, an out-of-range scale, a
// non-image element) surfaces as an error toast via setStatus (setSlice9Action),
// same as every other action here — no client-side re-validation beyond the
// basic number parsing numberInput/raw inputs already do.
function renderSlice9(element, root) {
  const body = collapsible(root, "slice9", "Slice-9");
  const sourceW = element.source_w || element.w;
  const sourceH = element.source_h || element.h;
  body.appendChild(readOnly("Source", `${sourceW} x ${sourceH}`));

  const slice9 = element.slice9;
  if (!slice9) {
    const def = defaultSlice9Inset(sourceW, sourceH);
    const draftInput = (value, step) => {
      const input = document.createElement("input");
      input.type = "number";
      input.className = "insp-input num";
      input.value = value;
      if (step) input.step = String(step);
      return input;
    };
    const left = draftInput(def);
    const top = draftInput(def);
    const right = draftInput(def);
    const bottom = draftInput(def);
    const scale = draftInput(1, 0.1);

    const grid = document.createElement("div");
    grid.className = "insp-grid";
    grid.appendChild(field("Left", left));
    grid.appendChild(field("Top", top));
    grid.appendChild(field("Right", right));
    grid.appendChild(field("Bottom", bottom));
    body.appendChild(grid);
    body.appendChild(field("Scale", scale));

    const enableBtn = document.createElement("button");
    enableBtn.type = "button";
    enableBtn.className = "primary insp-btn";
    enableBtn.textContent = "Enable";
    enableBtn.addEventListener("click", () => {
      const insets = {
        left: Number(left.value) || 0,
        top: Number(top.value) || 0,
        right: Number(right.value) || 0,
        bottom: Number(bottom.value) || 0,
      };
      const scaleValue = Number(scale.value);
      if (Number.isFinite(scaleValue) && scaleValue !== 1) insets.scale = scaleValue;
      setSlice9Action(element.id, insets);
    });
    body.appendChild(enableBtn);
  } else {
    const commitField = (key, value) => setSlice9Action(element.id, { ...slice9, [key]: value });
    const grid = document.createElement("div");
    grid.className = "insp-grid";
    grid.appendChild(field("Left", numberInput(slice9.left, (v) => commitField("left", v))));
    grid.appendChild(field("Top", numberInput(slice9.top, (v) => commitField("top", v))));
    grid.appendChild(field("Right", numberInput(slice9.right, (v) => commitField("right", v))));
    grid.appendChild(field("Bottom", numberInput(slice9.bottom, (v) => commitField("bottom", v))));
    body.appendChild(grid);
    body.appendChild(field("Scale", numberInput(slice9.scale != null ? slice9.scale : 1, (v) => commitField("scale", v), { step: 0.1 })));
    body.appendChild(smallBtn("Clear slice-9", () => setSlice9Action(element.id, null)));
  }

  const hint = document.createElement("div");
  hint.className = "insp-region-hint";
  hint.textContent = "Corners stay fixed (× scale), edges stretch one axis, center stretches both.";
  body.appendChild(hint);
}

// ---- Animation (T0260 increment 2) ---------------------------------------------
//
// Procedural animation (element.animation — the ai_studio.canvas.animation.v1 spec) on an
// IMAGE or TEXT element, with an on-canvas PREVIEW (workspace.js samples the SHARED
// animation.mjs each rAF; pure view-state, never journaled). Chat/codex is the intended spec
// editor ("wings slower" = one number patch); this section is the manual fallback — seed a
// sample, Play/Stop the preview, hand-edit the JSON, or clear it. Play/Stop is per-element
// VIEW-STATE: switching selection does NOT stop playback, so the lead can start several
// elements and watch them together. Each of Add/Save/Clear = exactly ONE setElementAnimation op.

// The starter "Add sample animation" writes: a ~8px vertical bob every 1.2s (one osc channel
// on off_y) — instantly visible, and a valid spec the sampler/op both accept as-is.
const ANIMATION_SAMPLE_SPEC = { v: 1, channels: [{ prop: "off_y", kind: "osc", amplitude: 8, period_ms: 1200 }] };

// T0264: the text->animation bridge — a one-line description + [Animate] that runs the codex op
// (animateElementFromTextAction). Shown ABOVE the sample/Play/Edit row and in BOTH states: with
// no spec it authors one from scratch, with an existing spec it minimally patches it ("медленнее"
// = just the period). Enter submits too; the button disables while codex runs (~5-30s) via
// runLongOp's `control`; a failure surfaces as an error toast and the typed text survives (no
// applyMutation re-renders the input). On success the action auto-plays the preview.
function renderAnimateFromText(element, body) {
  const row = document.createElement("div");
  row.className = "insp-alpha-row";
  const input = document.createElement("input");
  input.type = "text";
  input.className = "insp-input";
  input.placeholder = "describe the motion…";
  input.title = "e.g. \"gently bobs up and down\" — or, with an animation set, \"make it slower\"";
  const btn = document.createElement("button");
  btn.type = "button";
  btn.className = "insp-btn-small";
  btn.style.flex = "0 0 auto"; // hug the label so the description input keeps the row width
  btn.textContent = "Animate";
  const submit = () => {
    const text = input.value.trim();
    if (!text) return;
    animateElementFromTextAction(element.id, text, btn);
  };
  btn.addEventListener("click", submit);
  input.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      submit();
    }
  });
  row.append(input, btn);
  body.appendChild(row);
}

function renderAnimation(element, root) {
  const body = collapsible(root, "animation", "Animation");
  renderAnimateFromText(element, body);
  const animation = element.animation;
  const hasSpec = animation && Array.isArray(animation.channels) && animation.channels.length > 0;

  if (!hasSpec) {
    const empty = document.createElement("div");
    empty.className = "insp-region-hint";
    empty.textContent = "No animation. Add a sample, then edit it here or ask chat.";
    body.appendChild(empty);

    const addBtn = document.createElement("button");
    addBtn.type = "button";
    addBtn.className = "insp-btn";
    addBtn.textContent = "Add sample animation";
    addBtn.title = "Seed a gentle vertical bob (off_y osc) you can preview and edit";
    addBtn.addEventListener("click", () => setElementAnimationAction(element.id, ANIMATION_SAMPLE_SPEC));
    body.appendChild(addBtn);
    return;
  }

  // Summary: "N channels: prop, prop" — the props this animation drives.
  const props = animation.channels.map((channel) => channel.prop);
  const summary = document.createElement("div");
  summary.className = "insp-region-hint";
  summary.textContent = `${props.length} ${props.length === 1 ? "channel" : "channels"}: ${props.join(", ")}`;
  body.appendChild(summary);

  const actions = document.createElement("div");
  actions.className = "insp-alpha-row";

  // Play/Stop is pure view-state (workspace.js), so a toggle repaints the CANVAS but NOT this
  // panel — re-label the button in place instead of forcing an inspector rebuild.
  const playing = isAnimationPreviewing(element.id);
  const playBtn = document.createElement("button");
  playBtn.type = "button";
  playBtn.className = playing ? "primary insp-btn insp-alpha-btn" : "insp-btn insp-alpha-btn";
  playBtn.textContent = playing ? "Stop" : "Play";
  playBtn.addEventListener("click", () => {
    toggleAnimationPreview(element.id);
    const now = isAnimationPreviewing(element.id);
    playBtn.textContent = now ? "Stop" : "Play";
    playBtn.classList.toggle("primary", now);
  });
  actions.appendChild(playBtn);

  const editBtn = document.createElement("button");
  editBtn.type = "button";
  editBtn.className = "insp-btn insp-alpha-btn";
  editBtn.textContent = "Edit JSON";
  editBtn.title = "Hand-edit the raw animation spec";
  editBtn.addEventListener("click", () => {
    openPromptModal(`${element.name || "Animation"} — JSON`, JSON.stringify(animation, null, 2), (next) => {
      // Throwing keeps the modal open + toasts (openPromptModal's save wrapper). Parse first,
      // then run the SAME validateAnimation the op re-validates with — a loud client-side gate
      // so a typo never round-trips.
      let spec;
      try {
        spec = JSON.parse(next);
      } catch (error) {
        throw new Error(`Invalid JSON: ${error.message}`);
      }
      validateAnimation(spec);
      setElementAnimationAction(element.id, spec);
    });
  });
  actions.appendChild(editBtn);
  body.appendChild(actions);

  // Clear = two-step confirm (home.js's in-place pattern): the first click ARMS (red label),
  // the second within the window clears; it disarms on blur or a 3s timeout so a stray single
  // click never wipes the spec. Stop any live preview first so it can't paint a vanished spec.
  const clearBtn = document.createElement("button");
  clearBtn.type = "button";
  clearBtn.className = "insp-btn-small insp-animation-clear";
  clearBtn.textContent = "Clear animation";
  let armed = false;
  let disarmTimer = null;
  const disarm = () => {
    armed = false;
    clearTimeout(disarmTimer);
    clearBtn.classList.remove("armed");
    clearBtn.textContent = "Clear animation";
  };
  clearBtn.addEventListener("click", () => {
    if (!armed) {
      armed = true;
      clearBtn.classList.add("armed");
      clearBtn.textContent = "Clear — confirm?";
      clearTimeout(disarmTimer);
      disarmTimer = setTimeout(disarm, 3000);
      return;
    }
    disarm();
    if (isAnimationPreviewing(element.id)) toggleAnimationPreview(element.id);
    setElementAnimationAction(element.id, null);
  });
  clearBtn.addEventListener("blur", disarm);
  body.appendChild(clearBtn);
}

// Flipbook section (T0265 increment 1): additive, shown only when the element carries a
// `flipbook` blob (design §1.2 — the per-frame video-anim result). Increment 1 is a
// read-only VIEWER: a frame/fps/mode counter + Play/Stop. Play/Stop is pure page-only
// preview (NOT journaled), reusing the SAME shared rAF as the procedural preview via
// toggleAnimationPreview — so a toggle repaints the canvas but not this panel (re-label in
// place, like renderAnimation's Play). Frame trim/delete, fps + play_mode editing, and sheet
// export are increment 2 (the animation mode) — deliberately absent here.
function renderFlipbook(element, root) {
  const fb = element.flipbook;
  if (!fb || typeof fb !== "object") return;
  const body = collapsible(root, "flipbook", "Flipbook");

  const frames = Array.isArray(fb.frames) ? fb.frames : [];
  const kept = frames.filter((frame) => frame && frame.kept !== false && frame.src);
  const fps = Number(fb.fps) > 0 ? Number(fb.fps) : 12;
  const mode = fb.play_mode || "loop";

  const summary = document.createElement("div");
  summary.className = "insp-region-hint";
  summary.textContent = `${kept.length} ${kept.length === 1 ? "frame" : "frames"} · ${fps} fps · ${mode}`;
  body.appendChild(summary);

  const playing = isAnimationPreviewing(element.id);
  const playBtn = document.createElement("button");
  playBtn.type = "button";
  playBtn.className = playing ? "primary insp-btn insp-alpha-btn" : "insp-btn insp-alpha-btn";
  playBtn.textContent = playing ? "Stop" : "Play";
  playBtn.disabled = kept.length === 0;
  playBtn.title = kept.length === 0 ? "No frames to play" : "Play the flipbook on the canvas (page-only preview)";
  playBtn.addEventListener("click", () => {
    toggleAnimationPreview(element.id);
    const now = isAnimationPreviewing(element.id);
    playBtn.textContent = now ? "Stop" : "Play";
    playBtn.classList.toggle("primary", now);
  });
  body.appendChild(playBtn);
}

// Generation section (T0250): additive, shown only when the element carries a frozen
// meta.recipe run snapshot ({cardId, engine, at, prompt_snapshot, refs_snapshot,
// params_snapshot} — minted by generateFromRecipeAction) — same "presence of the
// additive field" pattern as Provenance/the raw Meta dump below. Lead: "в мете я вижу
// названия и путь картинки хотелось бы и увидеть картинку визуально" — References gets
// an actual thumbnail per ref (reuses the plate-row/thumb/role classes from the Alpha
// plates list), not just the raw src string the Meta section already shows.
function renderGeneration(element, root) {
  const recipe = element.meta && element.meta.recipe;
  if (!recipe || typeof recipe !== "object") return;
  const body = collapsible(root, "generation", "Generation");

  body.appendChild(readOnly("Engine", recipe.engine || "—"));
  body.appendChild(readOnly("At", recipe.at ? new Date(recipe.at).toLocaleString() : "—"));

  const promptText = String(recipe.prompt_snapshot || "");
  const promptPreview = promptText.length > 60 ? `${promptText.slice(0, 60)}…` : promptText || "—"; // …

  const promptRow = document.createElement("div");
  promptRow.className = "insp-field insp-generation-prompt-row";
  const promptLabel = document.createElement("span");
  promptLabel.className = "insp-label";
  promptLabel.textContent = "Prompt";
  const promptPreviewEl = document.createElement("span");
  promptPreviewEl.className = "insp-generation-prompt-preview";
  promptPreviewEl.textContent = promptPreview;
  promptPreviewEl.title = promptText;
  const viewBtn = smallBtn("View", () => openPromptModal(element.name || "Prompt", promptText, null, { readOnly: true }));
  viewBtn.classList.add("insp-generation-view-btn");
  promptRow.append(promptLabel, promptPreviewEl, viewBtn);
  body.appendChild(promptRow);

  // Style row (T0239 increment 3): shown only when this run mixed in a style card
  // (meta.recipe.style_snapshot — additive, absent on a plain/no-style run). Read-only,
  // same modal seam as the Prompt row above.
  if (recipe.style_snapshot) {
    const stylePrompt = String(recipe.style_snapshot.prompt || "");
    const styleRow = document.createElement("div");
    styleRow.className = "insp-field insp-generation-prompt-row";
    const styleLabel = document.createElement("span");
    styleLabel.className = "insp-label";
    styleLabel.textContent = "Style";
    const styleNameEl = document.createElement("span");
    styleNameEl.className = "insp-generation-prompt-preview";
    styleNameEl.textContent = recipe.style_snapshot.name || "—";
    styleNameEl.title = stylePrompt;
    const styleViewBtn = smallBtn("View", () =>
      openPromptModal(recipe.style_snapshot.name || "Style prompt", stylePrompt, null, { readOnly: true }),
    );
    styleViewBtn.classList.add("insp-generation-view-btn");
    styleRow.append(styleLabel, styleNameEl, styleViewBtn);
    body.appendChild(styleRow);
  }

  const refsTitle = document.createElement("div");
  refsTitle.className = "insp-align-caption";
  refsTitle.textContent = "References";
  body.appendChild(refsTitle);

  const refs = Array.isArray(recipe.refs_snapshot) ? recipe.refs_snapshot : [];
  if (!refs.length) {
    const empty = document.createElement("div");
    empty.className = "insp-region-hint";
    empty.textContent = "No reference images were sent.";
    body.appendChild(empty);
  } else {
    const wrap = document.createElement("div");
    wrap.className = "insp-alpha-plates"; // reuse: same stacked thumb-row layout as the Alpha plates list
    refs.forEach((src) => {
      const row = document.createElement("div");
      row.className = "insp-plate-row";
      const shortName = String(src).split("/").pop();
      const img = document.createElement("img");
      img.className = "insp-plate-thumb";
      img.src = fileUrl({ src });
      img.alt = shortName;
      img.title = shortName;
      const label = document.createElement("span");
      label.className = "insp-plate-role";
      label.textContent = shortName;
      row.append(img, label);
      wrap.appendChild(row);
    });
    body.appendChild(wrap);
  }
}

// Pack meta (T0332 v2 phase C, build-spec §5): a SHEET element (meta.pack.cells is an
// ARRAY — the full per-cell manifest, see generatePackSheets) gets its own axes/time/prompt-
// modal (the SAME openPromptModal seam renderGeneration's Prompt row uses) plus a Regenerate
// button that force-regens exactly this sheet (--sheet, by this element's OWN `.name` — that
// IS the expander's job.name verbatim, the exact string generateFromRecipe's sheetSlug match
// requires; see commitPackSheetOutcome/storeAddImage — no extra field needed) into its own
// `.groupId` (the run group). A CUT (packSlice's own minimal per-cut meta.pack =
// {cardId, sheet_element_id, cell, axes} — no `cells` array) instead gets its own cell/axes +
// a link back to the parent sheet, where the full manifest/prompt actually live (packSlice's
// own doc: duplicating them per cut would balloon a 21-cut pack to 100+KB of repeated JSON).
function renderPackMeta(element, root) {
  const pack = element.meta && element.meta.pack;
  if (!pack || typeof pack !== "object") return;

  if (Array.isArray(pack.cells)) {
    const body = collapsible(root, "pack-sheet", "Pack sheet");
    body.appendChild(readOnly("Axes", JSON.stringify(pack.sheet_axes || {})));
    body.appendChild(readOnly("At", pack.at ? new Date(pack.at).toLocaleString() : "—"));

    const promptText = String(pack.prompt_snapshot || "");
    const promptPreview = promptText.length > 60 ? `${promptText.slice(0, 60)}…` : promptText || "—"; // …
    const promptRow = document.createElement("div");
    promptRow.className = "insp-field insp-generation-prompt-row";
    const promptLabel = document.createElement("span");
    promptLabel.className = "insp-label";
    promptLabel.textContent = "Prompt";
    const promptPreviewEl = document.createElement("span");
    promptPreviewEl.className = "insp-generation-prompt-preview";
    promptPreviewEl.textContent = promptPreview;
    promptPreviewEl.title = promptText;
    const viewBtn = smallBtn("View", () => openPromptModal(element.name || "Sheet prompt", promptText, null, { readOnly: true }));
    viewBtn.classList.add("insp-generation-view-btn");
    promptRow.append(promptLabel, promptPreviewEl, viewBtn);
    body.appendChild(promptRow);

    // Regenerate is grayed out once the card has left pack mode (recipe.pack cleared) — the
    // op's single-image branch simply IGNORES sheetSlug/runGroupId (proven by
    // recipe.test.mjs), so clicking it then would silently mint a normal single image instead
    // of regenerating this sheet, a confusing footgun worth refusing up front.
    const card = groupById(pack.cardId);
    const cardStillPack = !!(card && card.recipe && card.recipe.pack);
    const regenBtn = document.createElement("button");
    regenBtn.type = "button";
    regenBtn.className = "insp-btn";
    regenBtn.textContent = "Regenerate";
    regenBtn.disabled = !cardStillPack;
    regenBtn.title = cardStillPack
      ? "Force-regenerate exactly this sheet (codex, ~30-60s) into the same run group"
      : "The card is no longer in pack mode — Generate would mint a single image instead";
    regenBtn.addEventListener("click", () =>
      generateFromRecipeAction(pack.cardId, regenBtn, {
        runGroupId: element.groupId,
        sheetSlug: element.name,
        busyLabel: "Regenerating sheet… (codex)",
      }),
    );
    body.appendChild(regenBtn);
    return;
  }

  // A cut: own cell/axes + a link back to its sheet (selects it — the sheet's OWN Pack
  // sheet/Generation/Meta sections show the rest).
  const body = collapsible(root, "pack-cut", "Pack cut");
  body.appendChild(readOnly("Cell", JSON.stringify(pack.cell || [])));
  body.appendChild(readOnly("Axes", JSON.stringify(pack.axes || {})));
  const sheetElement = pack.sheet_element_id ? elementById(pack.sheet_element_id) : null;
  const linkBtn = smallBtn(sheetElement ? `View sheet: ${sheetElement.name}` : "Sheet not found", () => {
    if (sheetElement) selectOnly(sheetElement.id);
  });
  linkBtn.disabled = !sheetElement;
  body.appendChild(linkBtn);
}

// "Extracted" section (T0239 increment 4, final shape): ONE codex vision call
// (extractElementAction) writes element.meta.extracted — a complete standalone prompt, a
// subject-only prompt, and a style breakdown, plus a one-line description. No card is
// minted by the vision call itself; minting a card is a SEPARATE, cheap, non-codex
// "promotion" gesture (promoteRecipeAction / promoteStyleAction) that just re-slices the
// ALREADY-STORED blob, so the lead can extract once and mint as many cards as he likes at
// zero extra codex cost. "Re-extract" re-runs the vision call and overwrites (the
// regenerate ability). Absent meta.extracted, the section is just the "Extract" button.
function renderExtracted(element, root) {
  const extracted = element.meta && element.meta.extracted;
  // "Extracted prompts", not bare "Extracted" (lead: "extracted не очевидное название").
  const body = collapsible(root, "extracted", "Extracted prompts");

  if (!extracted || typeof extracted !== "object") {
    const extractBtn = document.createElement("button");
    extractBtn.type = "button";
    extractBtn.className = "insp-btn";
    extractBtn.textContent = "Extract";
    extractBtn.title = "Analyze this image with codex vision (~1 min): a standalone prompt, a subject-only prompt, and a style description";
    extractBtn.addEventListener("click", () => extractElementAction(element.id, extractBtn));
    body.appendChild(extractBtn);
    return;
  }

  body.appendChild(readOnly("Description", extracted.description || "—"));

  const copyText = async (text) => {
    try {
      await navigator.clipboard.writeText(text);
      setStatus("Copied.");
    } catch {
      setStatus("Clipboard write blocked by the browser.", true);
    }
  };

  const addPromptRow = (label, text, viewTitle) => {
    const value = String(text || "");
    const preview = value.length > 60 ? `${value.slice(0, 60)}…` : value || "—"; // …
    const row = document.createElement("div");
    row.className = "insp-field insp-generation-prompt-row";
    const labelEl = document.createElement("span");
    labelEl.className = "insp-label";
    labelEl.textContent = label;
    const previewEl = document.createElement("span");
    previewEl.className = "insp-generation-prompt-preview";
    previewEl.textContent = preview;
    previewEl.title = value;
    const viewBtn = smallBtn("View", () => openPromptModal(viewTitle, value, null, { readOnly: true }));
    viewBtn.classList.add("insp-generation-view-btn");
    const copyBtn = smallBtn("Copy", () => copyText(value));
    row.append(labelEl, previewEl, viewBtn, copyBtn);
    body.appendChild(row);
  };

  const elementLabel = element.name || "Element";
  addPromptRow("Full prompt", extracted.prompt_full, `${elementLabel} — full prompt`);
  addPromptRow("Subject", extracted.prompt_subject, `${elementLabel} — subject`);
  const style = extracted.style || {};
  const styleText = style.constraints_block ? `${style.style_block || ""}\n\n${style.constraints_block}` : style.style_block || "";
  addPromptRow("Style", styleText, `${elementLabel} — style`);

  const promoteRow = document.createElement("div");
  promoteRow.className = "insp-alpha-row";
  promoteRow.append(
    smallBtn("→ Recipe card", () => promoteRecipeAction(element.id)),
    smallBtn("→ Style card", () => promoteStyleAction(element.id)),
  );
  body.appendChild(promoteRow);

  const reExtractBtn = smallBtn("Re-extract", () => extractElementAction(element.id, reExtractBtn));
  reExtractBtn.title = "Re-run the vision call and overwrite the extracted data (~1 min, codex)";
  body.appendChild(reExtractBtn);
}

function renderElement(element, root) {
  const name = field("Name", textInput(element.name, (next) => renameElement(element.id, next)));
  name.classList.add("insp-name");
  root.appendChild(name);

  const layout = collapsible(root, "layout", "Position & Size", transformBadge(element));
  layout.appendChild(boxGrid(element, (patch) => patchElementBox(element.id, patch)));
  layout.appendChild(readOnly("Source", `${element.source_w || element.w} x ${element.source_h || element.h}`));
  layout.appendChild(renderResetToSourceButton(element));
  layout.appendChild(renderTransformControls(element));

  // Single node inside a parent group (screen or widget): align-to-frame — the "center
  // this widget inside the screen" case (Figma-auto reference, T0232 increment 1).
  if (element.groupId) renderAlignSection([element.id], root);

  renderRegions(element, root);
  renderAlpha(element, root);
  renderCleanup(element, root);
  renderFilters(element, root);
  renderSlice9(element, root);
  renderFlipbook(element, root);
  // procedural track rejected 2026-07-05, code dormant (T0260/T0264); re-enable = restore this call
  // renderAnimation(element, root);
  renderExtracted(element, root);
  renderGeneration(element, root);
  renderPackMeta(element, root);

  if (element.meta && element.meta.parent) {
    const prov = collapsible(root, "provenance", "Provenance");
    const parent = element.meta.parent;
    const parentEl = parent.elementId ? elementById(parent.elementId) : null;
    prov.appendChild(readOnly("Parent sheet", parentEl ? parentEl.name : parent.sheetSrc || parent.elementId || "—"));
    prov.appendChild(readOnly("Region", String(parent.regionId || "—")));
  }

  // "pack" is rendered by renderPackMeta above (sheet axes/prompt/Regenerate, or a cut's
  // cell/axes/sheet-link) — excluded here so it is not ALSO dumped as raw JSON below.
  const metaKeys = Object.keys(element.meta || {}).filter((key) => key !== "parent" && key !== "pack");
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

  const layout = collapsible(root, "layout", "Position & Size", transformBadge(element));
  const grid = document.createElement("div");
  grid.className = "insp-grid";
  grid.appendChild(field("X", numberInput(element.x, (v) => patchElementBox(element.id, { x: v }))));
  grid.appendChild(field("Y", numberInput(element.y, (v) => patchElementBox(element.id, { y: v }))));
  layout.appendChild(grid);
  layout.appendChild(readOnly("Size", `${element.w} x ${element.h} (auto-width)`));
  layout.appendChild(renderRotationRow(element));

  // Single node inside a parent group: align-to-frame (same rule as an image element).
  if (element.groupId) renderAlignSection([element.id], root);

  const hint = document.createElement("div");
  hint.className = "insp-region-hint";
  hint.textContent = "Double-click the text on the canvas to edit its content.";
  root.appendChild(hint);

  renderTextStyle(element, style, root);
  // procedural track rejected 2026-07-05, code dormant (T0260/T0264); re-enable = restore this call
  // renderAnimation(element, root);
}

// ---- note element (T0268) ----------------------------------------------------

// The NOTE inspector: name, an EDITABLE Position & Size (both w AND h — the box is fully
// user-fixed, unlike text's read-only "(auto-width)"), a Background section (sticky presets
// + custom + None), and a Text section (the note font SUBSET — no stroke/shadow). Every
// change commits ONE journaled patchElement, never per keystroke.
function renderNoteElement(element, root) {
  const style = element.style || {};
  const name = field("Name", textInput(element.name, (next) => renameElement(element.id, next)));
  name.classList.add("insp-name");
  root.appendChild(name);

  const layout = collapsible(root, "layout", "Position & Size");
  // Both w and h are editable (fully-fixed box) — reuse boxGrid, unlike text's read-only size.
  layout.appendChild(boxGrid(element, (patch) => patchElementBox(element.id, patch)));

  // Single node inside a parent group: align-to-frame (same rule as text/image).
  if (element.groupId) renderAlignSection([element.id], root);

  renderNoteBackground(element, root);
  renderNoteStyle(element, style, root);

  const hint = document.createElement("div");
  hint.className = "insp-region-hint";
  hint.textContent = "Double-click the note on the canvas to edit its text (it wraps and clips to the box).";
  root.appendChild(hint);
}

// The note's TEXT section: font family + weight, size, line height, align, fill color — the
// note font SUBSET only (no outline/shadow — those are text-only). Each change commits ONE
// patchNoteStyle (the op shallow-merges + validates against fonts.json).
function renderNoteStyle(element, style, root) {
  const body = collapsible(root, "text", "Text");
  const commit = (stylePatch) => patchNoteStyle(element.id, stylePatch);

  const families = fontFamilies();
  const familySel = selectInput(style.fontFamily, families.length ? families : [style.fontFamily], (family) => {
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
  body.appendChild(field("Fill", colorInput(style.color, (v) => commit({ color: v }))));
}

// The note's BACKGROUND section: a preset swatch row (yellow/green/pink/blue/gray) plus a
// mode None/Solid + custom color input (mirrors renderGroupBackground, with sticky presets).
// A change persists via setNoteBackground (patchElement -> applyMutation); the op validates
// it and refuses a background on a non-note element.
function renderNoteBackground(element, root) {
  const body = collapsible(root, "note-background", "Background");
  const current = element.background && element.background.type === "color" ? element.background : null;

  // Preset swatch row: one click sets the fill.
  const swatches = document.createElement("div");
  swatches.className = "note-swatches";
  for (const preset of NOTE_BACKGROUND_PRESETS) {
    const swatch = document.createElement("button");
    swatch.type = "button";
    swatch.className = "note-swatch";
    swatch.style.background = preset.color;
    swatch.title = preset.name;
    swatch.setAttribute("aria-label", preset.name);
    if (current && current.color.toLowerCase() === preset.color.toLowerCase()) swatch.classList.add("selected");
    swatch.addEventListener("click", () => setNoteBackground(element.id, { type: "color", color: preset.color }));
    swatches.appendChild(swatch);
  }
  body.appendChild(swatches);

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
  color.value = current ? current.color : "#fff9b1";
  color.className = "insp-color";
  color.disabled = !current;

  mode.addEventListener("change", () => {
    if (mode.value === "color") {
      color.disabled = false;
      setNoteBackground(element.id, { type: "color", color: color.value });
    } else {
      color.disabled = true;
      setNoteBackground(element.id, null);
    }
  });
  color.addEventListener("change", () => {
    if (mode.value === "color") setNoteBackground(element.id, { type: "color", color: color.value });
  });

  const row = field("Custom", mode);
  row.appendChild(color);
  body.appendChild(row);
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

// ---- pack mode (T0332 v2 phase C: build_spec_pack_card_2026-07-07.md) ---------------------
//
// Smart quotes (typed by a phone keyboard / autocorrect / pasted from a doc) are the single
// most common way a hand-typed axes JSON silently fails to parse — JSON.parse has no notion
// of «»/""/'' curly quotes. Straightened to ASCII BEFORE parsing (build-spec: "нормализация
// «умных кавычек» перед parse"). Pure/testable in isolation from the DOM textarea that calls
// it (tests/pack_ui.test.mjs).
export function normalizeSmartQuotes(text) {
  return String(text ?? "")
    .replace(/[“”„«»]/g, '"')
    .replace(/[‘’‚]/g, "'");
}

// A skeleton EXAMPLE prefilled into an empty axes textarea (build-spec: "префилл валидным
// скелетом-примером") — a starting point to edit, never auto-committed on its own; the lead's
// own blur still has to accept (or edit) it before patchRecipeAction ever sends anything.
export const PACK_AXES_SKELETON = `{
  "material": ["stone", "wood"],
  "grade": ["rusty", "plain", "gilded"]
}`;

// JSON.parse with a LINE/COLUMN pointer on failure (build-spec: "ошибка парсера с ПОЗИЦИЕЙ
// (строка/столбец)"). Modern V8 often ALREADY names a "(line X column Y)" pair right in the
// SyntaxError message — reused verbatim when present (most accurate); when it names only a
// 0-based character offset ("at position N") instead, this walks the text once to translate
// that into a line/column; some V8 error shapes (e.g. a trailing comma inside an array) give
// neither, and the raw message is shown as-is rather than a fabricated position. Throws a
// plain Error (never the raw SyntaxError) so every caller shows ONE consistent message shape.
// Also checks the coarse SHAPE (a plain object, not an array/primitive) — the per-axis
// semantic rules (non-empty string arrays, etc.) stay server-side (ops.normalizeRecipePack is
// the authority; a bad shape there still 400s, surfaced by patchRecipeAction's error toast).
export function parseAxesJson(rawText) {
  const text = normalizeSmartQuotes(rawText);
  let value;
  try {
    value = JSON.parse(text);
  } catch (error) {
    const lineCol = /line (\d+) column (\d+)/i.exec(error.message);
    if (lineCol) throw new Error(`Invalid JSON at line ${lineCol[1]}, column ${lineCol[2]}: ${error.message}`);
    const positionOnly = /position (\d+)/.exec(error.message);
    if (positionOnly) {
      const offset = Number(positionOnly[1]);
      let line = 1;
      let col = 1;
      for (let i = 0; i < offset && i < text.length; i += 1) {
        if (text[i] === "\n") {
          line += 1;
          col = 1;
        } else {
          col += 1;
        }
      }
      throw new Error(`Invalid JSON at line ${line}, column ${col}: ${error.message}`);
    }
    throw new Error(`Invalid JSON: ${error.message}`);
  }
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error(`Axes must be a JSON object of axisName -> array of values, e.g.:\n${PACK_AXES_SKELETON}`);
  }
  return value;
}

// A client-side ESTIMATE of the pack's sheet count (mirrors expand_jobs.py's own
// `itertools.product` over every axis EXCEPT `vary` — the "big" axes) — purely for the
// Generate/Preview busy-label/title text, computed before the real expander ever runs; the
// REAL count (and any axes/vary validation error) comes from Preview pack / Generate
// themselves. `vary` not matching any axis key is not an error here (that is the expander's
// own loud refusal at preview/generate time) — it just means no axis is excluded.
export function estimatePackSheetCount(pack) {
  if (!pack || typeof pack !== "object") return 0;
  const axes = pack.axes && typeof pack.axes === "object" ? pack.axes : {};
  let count = 1;
  for (const [name, values] of Object.entries(axes)) {
    if (name === pack.vary) continue;
    count *= Array.isArray(values) ? values.length : 0;
  }
  return count;
}

// Fresh pack-mode default: a WORKING config, not an empty draft (lead walked a chain of
// incomplete-config errors on 2026-07-07: empty vary -> pick vary -> other axis lacks a
// {slot}). ONE axis with vary preselected has no big axes at all, so the prompt needs no
// {slot} and Preview passes right after the toggle; errors only appear once the lead ADDS
// axes — at which point the Vary hint below has already stated the slot rule.
const DEFAULT_PACK_TEMPLATE = () => ({
  axes: { grade: ["rusty", "plain", "gilded"] },
  vary: "grade",
  grid: [3, 3],
  max_jobs: 12,
});

// `pack` REPLACES wholesale on every patch (ops.mjs's own doc: "patch ЗАМЕНЯЕТ pack
// целиком") — every pack-field commit below sends the FULL `{...recipe.pack, ...fieldPatch}`
// object, never a bare `{vary: next}`, mirroring cli.mjs's own recipe-set read-modify-write.
function commitPackPatch(group, recipe, fieldPatch) {
  patchRecipeAction(group.id, { pack: { ...recipe.pack, ...fieldPatch } });
}

const PACK_BG_KEYS = new Set(["#ff00ff", "#00ff00"]); // mirrors ops.mjs's BG_KEY_BACKGROUND keys

// Axes JSON textarea: blur-only validation (build-spec: "валидация на blur"), a skeleton
// prefill when axes is empty, smart-quote normalization, and a line/column-pointing error
// shown INLINE (never just a toast) so the lead can see exactly where the JSON broke.
function renderPackAxesField(group, recipe, body) {
  const textarea = document.createElement("textarea");
  textarea.className = "insp-input insp-pack-axes";
  textarea.rows = 6;
  const hasAxes = recipe.pack.axes && Object.keys(recipe.pack.axes).length;
  textarea.value = hasAxes ? JSON.stringify(recipe.pack.axes, null, 2) : PACK_AXES_SKELETON;
  body.appendChild(field("Axes (JSON)", textarea));

  const errorEl = document.createElement("div");
  errorEl.className = "insp-pack-error";
  errorEl.style.display = "none";
  body.appendChild(errorEl);

  textarea.addEventListener("blur", () => {
    const normalized = normalizeSmartQuotes(textarea.value);
    if (normalized !== textarea.value) textarea.value = normalized;
    let parsed;
    try {
      parsed = parseAxesJson(normalized);
    } catch (error) {
      errorEl.textContent = error.message;
      errorEl.style.display = "";
      return;
    }
    errorEl.style.display = "none";
    errorEl.textContent = "";
    if (JSON.stringify(parsed) === JSON.stringify(recipe.pack.axes || {})) return; // unchanged: no commit
    // Never leave vary dangling after an axes edit (lead's stumble 2026-07-07): if the
    // current vary is no longer a key of the new axes, re-point it at the first key in the
    // SAME commit — one journal entry, and Preview can't hit "vary '' is not a key".
    const patch = { axes: parsed };
    const keys = Object.keys(parsed);
    if (!keys.includes(recipe.pack.vary)) patch.vary = keys[0] || "";
    commitPackPatch(group, recipe, patch);
  });
}

// Vary select: options come from the axes keys (build-spec: "vary select (from axes keys)")
// — the CURRENT value is always included even if it is not (yet) an axes key, so the select
// never silently jumps to a different displayed value out from under an in-progress edit.
function renderPackVaryField(group, recipe, body) {
  const options = Object.keys(recipe.pack.axes || {});
  if (recipe.pack.vary && !options.includes(recipe.pack.vary)) options.push(recipe.pack.vary);
  if (!options.length) options.push("");
  body.appendChild(field("Vary", selectInput(recipe.pack.vary || "", options, (next) => commitPackPatch(group, recipe, { vary: next }))));
  // The one rule the error chain was teaching one bounce at a time (2026-07-07) — state it
  // upfront instead: vary spreads across the sheet cells, every OTHER axis must be a prompt
  // slot (the expander refuses an axis that affects nothing).
  const varyHint = document.createElement("div");
  varyHint.className = "insp-region-hint";
  varyHint.textContent = "Cells vary by this axis. Every OTHER axis must appear in the prompt as {axis} — one sheet per combination.";
  body.appendChild(varyHint);
}

// Grid select: 2x2 / 3x3 (build-spec: "grid select (2x2|3x3)") — a stored grid outside that
// pair (e.g. set via the CLI/an agent) is still shown, appended as its own option, same
// never-silently-jump rule as Vary above.
function renderPackGridField(group, recipe, body) {
  const options = ["2x2", "3x3"];
  const current = `${Number(recipe.pack.grid[0])}x${Number(recipe.pack.grid[1])}`;
  if (!options.includes(current)) options.push(current);
  const select = selectInput(current, options, (next) => {
    const match = /^(\d+)x(\d+)$/i.exec(next);
    if (!match) return;
    commitPackPatch(group, recipe, { grid: [Number(match[1]), Number(match[2])] });
  });
  body.appendChild(field("Grid", select));
}

function renderPackMaxJobsField(group, recipe, body) {
  body.appendChild(
    field(
      "Max jobs",
      numberInput(recipe.pack.max_jobs, (next) => commitPackPatch(group, recipe, { max_jobs: Math.max(1, Math.round(next)) })),
    ),
  );
}

// bg_key/n_candidates (T0332 v2: `params` unfrozen for exactly these two fields, plus
// size/quality — see ops.normalizeRecipePatch) live here, INSIDE the pack sub-block, since
// this phase adds no other params UI. bg_key's mode-aware hint + pair-validation is
// build-spec-mandated ("на blur, а не только на generate"): patch-time only checks generic
// hex format (any color is a legal single-image bg_key), so an off-pair value still commits
// here — the warning is ADVISORY, refusal itself stays where the build-spec puts it
// (packPreview/generateFromRecipe's pack branch). A successful blur-commit re-renders the
// whole inspector (applyMutation -> refresh), which recomputes this same warning from the
// freshly-stored value — no separate on-blur DOM patch needed.
function renderPackParamsFields(group, recipe, body) {
  // Defensive guard (recipe.params SHOULD always be an object per defaultRecipe, but a
  // hand-edited/legacy project.json could still omit it) — render sensible defaults instead
  // of throwing on a missing blob.
  const params = recipe.params || {};
  const bgKey = params.bg_key;
  const nCandidates = params.n_candidates != null ? params.n_candidates : 1;

  const bgKeyInput = document.createElement("input");
  bgKeyInput.type = "text";
  bgKeyInput.className = "insp-input";
  bgKeyInput.value = bgKey || "";
  bgKeyInput.addEventListener("blur", () => {
    const next = bgKeyInput.value.trim();
    if (!next || next === bgKey) return;
    patchRecipeAction(group.id, { params: { bg_key: next } });
  });
  body.appendChild(field("BG key", bgKeyInput));

  const bgKeyHint = document.createElement("div");
  bgKeyHint.className = "insp-region-hint";
  bgKeyHint.textContent = "Pack mode: only #ff00ff / #00ff00 — the key color gets baked into the sheet.";
  body.appendChild(bgKeyHint);

  if (!PACK_BG_KEYS.has(String(bgKey || "").toLowerCase())) {
    const warn = document.createElement("div");
    warn.className = "insp-pack-error";
    warn.textContent = `Current bg_key ${bgKey || "(none)"} is not #ff00ff/#00ff00 — Preview pack/Generate will refuse until this matches.`;
    body.appendChild(warn);
  }

  body.appendChild(
    field(
      "Candidates",
      numberInput(nCandidates, (next) => patchRecipeAction(group.id, { params: { n_candidates: Math.max(1, Math.round(next)) } })),
    ),
  );
}

// Recipe card surface (T0239 increment 1): additive, shown only when the selected group
// carries a `recipe` blob (same "presence of the additive field" pattern as
// renderGroupBackground/renderAlphaPlates — a plain group renders no Recipe section at
// all). Prompt + Engine are live-editable through patchRecipeAction (one journal entry
// per commit, mirrors every other inspector field). Generate runs the T0239-2 flow via
// generateFromRecipeAction (long-op queue, codex/agy = minutes; disabled on an empty
// prompt — the op would refuse loudly anyway, the disable just says WHY up front). The
// Style dropdown (T0239 increment 3) lists every style-card group of THIS project by name;
// picking one commits recipe.style_ref through the SAME patchRecipeAction the other fields
// use — style cards mix their prompt + ref image into the next Generate (ops.mjs).
function renderRecipe(group, root) {
  const recipe = group.recipe;
  if (!recipe || typeof recipe !== "object") return;
  const body = collapsible(root, "recipe", "Recipe");

  const promptField = field("Prompt", textareaInput(recipe.prompt, (next) => patchRecipeAction(group.id, { prompt: next })));
  body.appendChild(promptField);

  // T0250 (lead: "промпт тяжело вот так читать" — the 3-row textarea reads a real prompt
  // badly). Opens the SAME prompt in a large centered modal; Save commits through the
  // identical patchRecipeAction the inline textarea uses (one journal entry either way).
  const editPromptBtn = smallBtn("Edit", () =>
    openPromptModal(group.name || "Prompt", recipe.prompt, (next) => patchRecipeAction(group.id, { prompt: next })),
  );
  editPromptBtn.classList.add("insp-prompt-edit-btn");
  editPromptBtn.title = "Open the prompt in a large editor";
  body.appendChild(editPromptBtn);

  const engineField = field(
    "Engine",
    selectInput(recipe.engine || "codex", ["codex", "gemini", "both"], (next) => patchRecipeAction(group.id, { engine: next })),
  );
  body.appendChild(engineField);
  // T0332 v2 UX finding: pack mode is codex-only in v1 — the select is disabled (advisory;
  // the real gate is packPreview/generateFromRecipe's pack branch), WITH a visible caption,
  // not just a hover title (build-spec: "задизейблен С ПОДПИСЬЮ").
  if (recipe.pack) {
    engineField.querySelector("select").disabled = true;
    engineField.querySelector("select").title = "Packs are codex-only in v1";
    const engineHint = document.createElement("div");
    engineHint.className = "insp-region-hint";
    engineHint.textContent = "Packs are codex-only in v1";
    body.appendChild(engineHint);
  }

  const styleSelect = document.createElement("select");
  styleSelect.className = "insp-input";
  const noneOption = document.createElement("option");
  noneOption.value = "";
  noneOption.textContent = "None";
  styleSelect.appendChild(noneOption);
  const styleCards = (state.project ? state.project.groups || [] : []).filter(
    (candidate) => candidate.style && typeof candidate.style === "object",
  );
  for (const styleCard of styleCards) {
    const option = document.createElement("option");
    option.value = styleCard.id;
    option.textContent = styleCard.name || styleCard.id;
    styleSelect.appendChild(option);
  }
  styleSelect.value = recipe.style_ref || "";
  styleSelect.addEventListener("change", () => patchRecipeAction(group.id, { style_ref: styleSelect.value || null }));
  body.appendChild(field("Style", styleSelect));

  // ---- Pack mode (T0332 v2 phase C) -----------------------------------------------
  //
  // "Слить": pack is a MODE of the recipe card, not a third card type — this toggle just
  // sets/clears `recipe.pack` (a full default template on / null off); every other pack
  // control below only renders once it is set. The Generate button further down is the SAME
  // button either way — it branches on recipe.pack server-side (ops.generateFromRecipe).
  const packToggleRow = document.createElement("label");
  packToggleRow.className = "insp-check";
  const packToggleCheck = document.createElement("input");
  packToggleCheck.type = "checkbox";
  packToggleCheck.checked = !!recipe.pack;
  packToggleCheck.addEventListener("change", () => {
    patchRecipeAction(group.id, { pack: packToggleCheck.checked ? DEFAULT_PACK_TEMPLATE() : null });
  });
  const packToggleLabel = document.createElement("span");
  packToggleLabel.textContent = "Pack mode";
  packToggleRow.append(packToggleCheck, packToggleLabel);
  body.appendChild(packToggleRow);

  if (recipe.pack) {
    renderPackAxesField(group, recipe, body);
    renderPackVaryField(group, recipe, body);
    renderPackGridField(group, recipe, body);
    renderPackMaxJobsField(group, recipe, body);
    renderPackParamsFields(group, recipe, body);
  }

  // UX finding: recipe.expanded non-empty AND pack set — pack mode ignores it entirely (it
  // always sends recipe.prompt VERBATIM, never resolveRecipePromptText/expanded), so without
  // this the disappearing Expand-prompt button + a silently-ignored Expanded block would read
  // as a bug, not a mode.
  if (recipe.pack && recipe.expanded) {
    const banner = document.createElement("div");
    banner.className = "insp-pack-banner";
    banner.textContent = "Pack generates from the base prompt — expanded is not used. Move any needed detail into the prompt or the style card.";
    body.appendChild(banner);
  }

  // "last pack: <ts>, N sheets, M failed" (build-spec UX finding) — only for a PACK-shaped
  // last_run ({at, verdict, run_group_id, failed}); the single-image branch's own last_run
  // shape ({at, result_element_id, verdict}) has no `failed` array, so this line never shows
  // for a plain single-image run. N sheets is derived from the run group's OWN sheet elements
  // (nothing else persists a "sheets in this run" count) — a sheet is any element in
  // last_run.run_group_id carrying this card's meta.pack.cells (the full-manifest marker).
  if (recipe.last_run && Array.isArray(recipe.last_run.failed)) {
    const runElements = recipe.last_run.run_group_id
      ? (state.project ? state.project.elements || [] : []).filter(
          (el) => el.groupId === recipe.last_run.run_group_id && el.meta && el.meta.pack && Array.isArray(el.meta.pack.cells) && el.meta.pack.cardId === group.id,
        )
      : [];
    const ts = recipe.last_run.at ? new Date(recipe.last_run.at).toLocaleString() : "—";
    body.appendChild(readOnly("Last pack", `${ts}, ${runElements.length} sheet(s), ${recipe.last_run.failed.length} failed`));
  }

  if (recipe.pack) {
    const previewBtn = smallBtn("Preview pack", () => packPreviewAction(group.id, previewBtn));
    previewBtn.title = "Ephemeral: shows sheet count + per-sheet prompts — the ONLY honest per-cell preview (single Generate assembles a different prompt)";
    body.appendChild(previewBtn);

    if (state.packPreview && state.packPreview.cardId === group.id) {
      const preview = state.packPreview;
      const summary = document.createElement("div");
      summary.className = "insp-align-caption";
      summary.textContent = `Preview: ${preview.sheets} sheet(s)${preview.style_ref_image ? " · style ref image included" : ""}`;
      body.appendChild(summary);
      const previewWrap = document.createElement("div");
      previewWrap.className = "insp-alpha-plates"; // reuse: same stacked thumb/row layout as Generation's References list
      for (const job of preview.jobs || []) {
        const row = document.createElement("div");
        row.className = "insp-plate-row";
        const label = document.createElement("span");
        label.className = "insp-plate-role";
        label.textContent = job.name;
        const viewBtn = smallBtn("View", () => openPromptModal(job.name, job.prompt, null, { readOnly: true }));
        row.append(label, viewBtn);
        previewWrap.appendChild(row);
      }
      body.appendChild(previewWrap);
    }
  }

  const generateBtn = document.createElement("button");
  generateBtn.type = "button";
  generateBtn.className = "primary insp-btn";
  generateBtn.textContent = "Generate";
  const emptyPrompt = !String(recipe.prompt || "").trim();
  generateBtn.disabled = emptyPrompt;
  const packSheetEstimate = recipe.pack ? estimatePackSheetCount(recipe.pack) : 0;
  const packBusyLabel = `Generating pack… (~${packSheetEstimate} sheet(s), codex, ~30-60s each)`; // …
  generateBtn.title = emptyPrompt
    ? "Write a prompt first"
    : recipe.pack
      ? `Generate the pack (~${packSheetEstimate} sheet(s) — see Preview pack for the exact count/prompts)`
      : "Generate (codex/agy — takes minutes)";
  generateBtn.addEventListener("click", () =>
    generateFromRecipeAction(group.id, generateBtn, recipe.pack ? { busyLabel: packBusyLabel } : undefined),
  );
  body.appendChild(generateBtn);

  if (recipe.pack) {
    const canSlice = !!(recipe.last_run && recipe.last_run.run_group_id);
    const sliceBtn = document.createElement("button");
    sliceBtn.type = "button";
    sliceBtn.className = "insp-btn";
    sliceBtn.textContent = "Slice pack";
    sliceBtn.disabled = !canSlice;
    sliceBtn.title = canSlice ? "Detect + slice every sheet of the last pack run" : "Generate a pack run first";
    sliceBtn.addEventListener("click", () => packSliceAction(group.id, sliceBtn));
    body.appendChild(sliceBtn);
  }

  // ---- Expand-prompt (T0239 increment 4) ------------------------------------------
  // Hidden in pack mode (UX finding): pack always sends recipe.prompt verbatim, so Expand
  // has nothing to feed — showing it would invite generating a text nothing ever reads.
  if (!recipe.pack) {
    const expandBtn = document.createElement("button");
    expandBtn.type = "button";
    expandBtn.className = "insp-btn";
    expandBtn.textContent = "Expand prompt";
    expandBtn.disabled = emptyPrompt;
    expandBtn.title = emptyPrompt ? "Write a prompt first" : "Expand into a labeled generation-prompt template (codex, ~1 min)";
    expandBtn.addEventListener("click", () => expandRecipePromptAction(group.id, expandBtn));
    body.appendChild(expandBtn);
  }

  // The Expanded block only renders once recipe.expanded exists: an editable textarea + a
  // large-editor Edit modal (both commit through patchRecipeAction({expanded}), same
  // pattern as the Prompt field above) + the "Send expanded" checkbox (defaults true,
  // patchRecipeAction({use_expanded})) + Discard (patches {expanded: null} — "remove the
  // stale expansion", one journal entry). The muted hint states which text Generate will
  // ACTUALLY send right now, mirroring resolveRecipePromptText's own rule exactly
  // (`use_expanded && expanded ? expanded : prompt`) — EXCEPT in pack mode, where it never
  // reads expanded/use_expanded at all (T0332 v2), so the hint says that instead.
  if (recipe.expanded != null) {
    const expandedField = field(
      "Expanded",
      textareaInput(recipe.expanded, (next) => patchRecipeAction(group.id, { expanded: next })),
    );
    body.appendChild(expandedField);

    const editExpandedBtn = smallBtn("Edit", () =>
      openPromptModal(`${group.name || "Recipe"} — expanded`, recipe.expanded, (next) => patchRecipeAction(group.id, { expanded: next })),
    );
    editExpandedBtn.classList.add("insp-prompt-edit-btn");
    editExpandedBtn.title = "Open the expanded prompt in a large editor";
    body.appendChild(editExpandedBtn);

    const sendRow = document.createElement("label");
    sendRow.className = "insp-check";
    const sendCheck = document.createElement("input");
    sendCheck.type = "checkbox";
    sendCheck.checked = recipe.use_expanded !== false;
    sendCheck.addEventListener("change", () => patchRecipeAction(group.id, { use_expanded: sendCheck.checked }));
    const sendLabel = document.createElement("span");
    sendLabel.textContent = "Send expanded";
    sendRow.append(sendCheck, sendLabel);
    body.appendChild(sendRow);

    const discardBtn = smallBtn("Discard", () => patchRecipeAction(group.id, { expanded: null }));
    discardBtn.title = "Remove the expanded text — Generate falls back to the short prompt";
    body.appendChild(discardBtn);

    const willSendExpanded = !recipe.pack && recipe.use_expanded !== false && recipe.expanded;
    const hint = document.createElement("div");
    hint.className = "insp-region-hint";
    hint.textContent = recipe.pack
      ? "Pack mode ignores the expanded text — it always generates from the short prompt."
      : willSendExpanded
        ? "Generate sends the expanded text."
        : "Generate sends the short prompt (expanded text kept, not sent).";
    body.appendChild(hint);
  }
}

// ---- style card (T0239 increment 3) --------------------------------------------

// Style card surface: additive, shown only when the selected group carries a `style` blob
// (same pattern as renderRecipe). Prompt is live-editable through patchStyleAction (+ the
// SAME openPromptModal seam the Recipe prompt uses, reused verbatim for the large editor).
// Members lists every IMAGE member (reuses the Generation section's plate-thumb row shape,
// design R1): the current ref shows a "ref" badge, every other image gets a "Make ref"
// button. Non-image members (text) never show here — a style card's ref/examples are images
// only (design R1).
function renderStyle(group, root) {
  const style = group.style;
  if (!style || typeof style !== "object") return;
  const body = collapsible(root, "style", "Style");

  const promptField = field("Prompt", textareaInput(style.prompt, (next) => patchStyleAction(group.id, { prompt: next })));
  body.appendChild(promptField);

  const editPromptBtn = smallBtn("Edit", () =>
    openPromptModal(group.name || "Style prompt", style.prompt, (next) => patchStyleAction(group.id, { prompt: next })),
  );
  editPromptBtn.classList.add("insp-prompt-edit-btn");
  editPromptBtn.title = "Open the style prompt in a large editor";
  body.appendChild(editPromptBtn);

  const membersTitle = document.createElement("div");
  membersTitle.className = "insp-align-caption";
  membersTitle.textContent = "Members";
  body.appendChild(membersTitle);

  const images = memberElements(group.id).filter((element) => element.type === "image");
  if (!images.length) {
    const empty = document.createElement("div");
    empty.className = "insp-region-hint";
    empty.textContent = "Drag images into this card — the first one auto-becomes the ref.";
    body.appendChild(empty);
  } else {
    const wrap = document.createElement("div");
    wrap.className = "insp-alpha-plates"; // reuse: same stacked thumb-row layout as the plate/reference lists
    images.forEach((image) => {
      const row = document.createElement("div");
      row.className = "insp-plate-row";
      const img = document.createElement("img");
      img.className = "insp-plate-thumb";
      img.src = fileUrl(image);
      img.alt = image.name || "";
      img.title = image.name || image.id;
      const label = document.createElement("span");
      label.className = "insp-plate-role";
      label.textContent = image.name || image.id;
      row.append(img, label);
      if (style.ref === image.id) {
        const badge = document.createElement("span");
        badge.className = "insp-style-ref-badge";
        badge.textContent = "ref";
        row.appendChild(badge);
      } else {
        row.appendChild(smallBtn("Make ref", () => patchStyleAction(group.id, { ref: image.id })));
      }
      wrap.appendChild(row);
    });
    body.appendChild(wrap);
  }
}

// ---- animation card (T0265 increment 1, video route) ---------------------------

// Animation card surface: additive, shown only when the selected group carries an `anim`
// blob (design §1.1 — same "presence of the additive field" pattern as renderRecipe/
// renderStyle). Motion + Profile + Matte + Seed + Loop are live-editable through
// patchAnimAction (one journal entry per commit, mirrors every other inspector field).
// Generate runs the video route via generateAnimFromCardAction (long-op queue, minutes;
// disabled on an empty motion — the op refuses loudly anyway, the disable just says WHY up
// front). Increment 1 covers the generation inputs only; the frame-editing animation mode
// (timeline/trim/fps/play_mode/takes/export) is increment 2. Mirrors renderRecipe.
function renderAnim(group, root) {
  const anim = group.anim;
  if (!anim || typeof anim !== "object") return;
  const body = collapsible(root, "anim", "Animation card");

  const motionField = field("Motion", textareaInput(anim.motion, (next) => patchAnimAction(group.id, { motion: next })));
  body.appendChild(motionField);

  const editMotionBtn = smallBtn("Edit", () =>
    openPromptModal(group.name || "Motion", anim.motion, (next) => patchAnimAction(group.id, { motion: next })),
  );
  editMotionBtn.classList.add("insp-prompt-edit-btn");
  editMotionBtn.title = "Open the motion description in a large editor";
  body.appendChild(editMotionBtn);

  body.appendChild(
    field(
      "Profile",
      selectInput(anim.profile || "draft", ["draft", "final"], (next) => patchAnimAction(group.id, { profile: next })),
    ),
  );

  body.appendChild(
    field(
      "Matte",
      selectInput(anim.matte || "corridorkey", ["corridorkey", "key_matte"], (next) => patchAnimAction(group.id, { matte: next })),
    ),
  );

  // Seed: blank = null (random on each Generate); a number pins it. A text input keeps
  // "clear = random" one clean gesture (a number input can't tell empty from 0). The op
  // re-validates (number|null) loudly.
  const seedInput = document.createElement("input");
  seedInput.type = "text";
  seedInput.className = "insp-input";
  seedInput.placeholder = "random";
  seedInput.value = anim.seed == null ? "" : String(anim.seed);
  seedInput.title = "Blank = random on each Generate; a number pins the seed";
  const commitSeed = () => {
    const raw = seedInput.value.trim();
    if (raw === "") {
      if (anim.seed != null) patchAnimAction(group.id, { seed: null });
      return;
    }
    const n = Number(raw);
    if (!Number.isFinite(n)) {
      // F5: invalid input is LOUD — say why and restore the field to the committed value
      // (empty = random) instead of silently leaving unsaved bad text in the box.
      setStatus("Seed must be a number (or empty for random).", true);
      seedInput.value = anim.seed == null ? "" : String(anim.seed);
      return;
    }
    if (n !== anim.seed) patchAnimAction(group.id, { seed: n });
  };
  seedInput.addEventListener("change", commitSeed);
  seedInput.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      seedInput.blur();
      focusStage();
    }
  });
  body.appendChild(field("Seed", seedInput));

  // Loop hint (design §1.1): seamless-loop hint for generation (a single keyframe becomes a
  // same-image FLF). NOT the playback loop — that lives on the result (flipbook.play_mode).
  const loopRow = document.createElement("label");
  loopRow.className = "insp-check";
  const loopCheck = document.createElement("input");
  loopCheck.type = "checkbox";
  loopCheck.checked = anim.loop !== false;
  loopCheck.addEventListener("change", () => patchAnimAction(group.id, { loop: loopCheck.checked }));
  const loopLabel = document.createElement("span");
  loopLabel.textContent = "Loop";
  loopRow.append(loopCheck, loopLabel);
  body.appendChild(loopRow);

  const generateBtn = document.createElement("button");
  generateBtn.type = "button";
  generateBtn.className = "primary insp-btn";
  generateBtn.textContent = "Generate";
  const emptyMotion = !String(anim.motion || "").trim();
  generateBtn.disabled = emptyMotion;
  generateBtn.title = emptyMotion ? "Describe the motion first" : "Generate the animation (video route — takes minutes)";
  generateBtn.addEventListener("click", () => generateAnimFromCardAction(group.id, generateBtn));
  body.appendChild(generateBtn);
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

  // Screen (T0332 B1 — export flipped to opt-in): group.screen === true is the ONLY thing
  // that makes a top-level visible group count as an exportable "screen" (exportProject/
  // visibleScreenCount both gate on it — a recipe/style/anim/pack-run card simply never
  // carries it, no special-case skip needed anywhere).
  const screenRow = document.createElement("label");
  screenRow.className = "insp-check";
  const screenCheck = document.createElement("input");
  screenCheck.type = "checkbox";
  screenCheck.checked = group.screen === true;
  screenCheck.addEventListener("change", () => setGroupScreen(group.id, screenCheck.checked));
  const screenLabel = document.createElement("span");
  screenLabel.textContent = "Screen";
  screenRow.append(screenCheck, screenLabel);
  layout.appendChild(screenRow);

  // Nudge (build-spec UX finding): a top-level, VISIBLE, unflagged group otherwise sits
  // outside Export project with no clue why — this only fires for exactly that case (a
  // nested widget frame, or an already-hidden group, is not "missing" from the export).
  if (group.parentId == null && group.visible !== false && group.screen !== true) {
    const screenHint = document.createElement("div");
    screenHint.className = "insp-region-hint";
    screenHint.textContent = "Check Screen to include this group in Export project.";
    layout.appendChild(screenHint);
  }

  layout.appendChild(readOnly("Members", String(memberElements(group.id).length)));

  // A nested group (a widget frame inside a screen) aligns to ITS parent group's frame —
  // same Figma-auto rule as a single element inside a group.
  if (group.parentId) renderAlignSection([group.id], root);

  renderGroupBackground(group, root);
  renderRecipe(group, root);
  renderStyle(group, root);
  // Video-anim generation frozen 2026-07-06 (VIDEO_ANIM_FROZEN, app.js) — hide the anim-card
  // Generate section; renderFlipbook playback above is unaffected.
  if (!VIDEO_ANIM_FROZEN) renderAnim(group, root);

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
    renderMultiFilters(selected, root);
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

// Top-level VISIBLE, SCREEN-FLAGGED groups — exactly what exportProject renders (every
// parentId-less visible group with the explicit `screen === true` opt-in flag, T0332 B1:
// "ЭКСПОРТ — ИНВЕРСИЯ НА OPT-IN"). Computed via the shared tree helper
// (childrenOf(root).groups), not a hand-rolled scan, so the button label never counts
// nested component groups (T0224 item 9: "Export project (N screens)" must match
// exportProject, which is top-level only) — and, since the flip, never counts an unflagged
// group either, recipe/style/anim/pack_run cards included (they simply never carry `screen`).
function visibleScreenCount() {
  if (!state.project) return 0;
  return childrenOf(state.project, null).groups.filter((group) => group.visible !== false && group.screen === true).length;
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
    // recipe/style ride in the signature so a prompt/engine/ref commit (or a CLI edit)
    // rebuilds the Recipe/Style section — the Generate button's empty-prompt disable and
    // the Style member/ref rows both depend on it.
    return `g:${group.id}|${group.name}|${group.x},${group.y},${group.w},${group.h}|${group.visible !== false}|${group.clip === true}|${memberElements(group.id).length}|${JSON.stringify(group.background || null)}|${group.parentId || ""}|${JSON.stringify(group.recipe || null)}|${JSON.stringify(group.style || null)}|${JSON.stringify(group.anim || null)}`;
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
      // T0249: rotation is part of the structure too (the Position & Size header badge
      // + the rotation row's Reset button both depend on it) — a rotation-only change
      // (e.g. the canvas rotate handle) must rebuild the section, not just skip.
      return `t:${e.id}|${e.name}|${e.x},${e.y},${e.w},${e.h}|${e.content}|${JSON.stringify(e.style || {})}|${e.groupId || ""}|${e.rotation || 0}|${JSON.stringify(e.animation || null)}`;
    }
    // A note's structure is its box + content + style + background (T0268) — any change must
    // rebuild the Note sections so the inputs (size, Text, Background swatches) reflect it.
    if (e.type === "note") {
      return `n:${e.id}|${e.name}|${e.x},${e.y},${e.w},${e.h}|${e.content}|${JSON.stringify(e.style || {})}|${JSON.stringify(e.background || null)}|${e.groupId || ""}`;
    }
    const regions = (e.regions || [])
      .map((r) => `${r.id}~${r.name || ""}~${(r.rect || r.content_bbox || []).join(",")}`)
      .join("|");
    // element.export is part of the structure: a row add/remove/edit must rebuild.
    // rotation/flipH/flipV (T0249): the badge + Reset button + flip active-state all
    // depend on these, so they must rebuild the section too. element.slice9 (T0233
    // Packet 2): the Slice-9 section's Enable-vs-live-fields branch + every field's
    // displayed value depend on it, so an Enable/Clear/field commit must rebuild too.
    return `e:${e.id}|${e.name}|${e.x},${e.y},${e.w},${e.h}|${e.source_w},${e.source_h}|${regions}|${JSON.stringify(e.export || [])}|${JSON.stringify(e.slice9 || null)}|${JSON.stringify(e.meta || {})}|${e.groupId || ""}|${e.rotation || 0},${e.flipH ? 1 : 0},${e.flipV ? 1 : 0}|${JSON.stringify(e.animation || null)}|${JSON.stringify(e.flipbook || null)}`;
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

  // Cleanup preview (T0207) is pure view-state scoped to exactly one selected IMAGE
  // element (workspace.js paints it instead of the source while active). We only get here
  // when the selection/element identity actually changed (the sig check above already
  // skipped a no-op rerender), so drop any preview that no longer belongs to what's about
  // to render — otherwise it would silently resurrect if the user navigates back to the
  // same element later, which the spec explicitly rules out ("never leave a stale preview
  // painted on the wrong element").
  const cleanupOwnerId = selected.length === 1 && selected[0].type === "image" ? selected[0].id : null;
  const activeCleanupPreview = getCleanupPreview();
  if (activeCleanupPreview && activeCleanupPreview.elementId !== cleanupOwnerId) clearCleanupPreview();
  // The floating cleanup dialog must never outlive its element either (T0207 redesign).
  syncCleanupDialog(cleanupOwnerId);

  if (group) {
    renderGroupInspector(group, root);
  } else if (multiGroup) {
    renderMultiGroup(groupIds, root);
  } else if (selected.length === 1) {
    if (selected[0].type === "text") renderTextElement(selected[0], root);
    else if (selected[0].type === "note") renderNoteElement(selected[0], root);
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
