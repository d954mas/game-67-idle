// Canvas actions: the page's single place where UI intents become one HTTP API
// call to the shared ops layer. Panels and menus call these; none of them talk to
// the API directly. Nothing here holds business logic — each function marshals a
// request and reloads the project from disk (the source of truth).
import {
  api,
  applyMutation,
  clearSelection,
  elementById,
  elements,
  groupById,
  refresh,
  regionEditElement,
  selectGroupOnly,
  selectOnly,
  setStatus,
  state,
} from "./app.js";
import { buildNodesSpec, orderedChildren, nodeScope } from "../tree.mjs";
import { mergeTextStyle } from "../fonts.mjs";
import { getFontManifest, measureTextBox } from "./fonts.js";
import { screenToImagePoint } from "./viewport.mjs";
import { saveBlobToFile } from "./export_dest.mjs";
import { runLongOp } from "./toasts.js";
// T0264: auto-play the preview the moment a text->animation spec lands (the lead's demo
// moment). workspace.js already imports from this module; both directions only use hoisted
// function exports at call time, so the cycle resolves cleanly.
import { startAnimationPreview } from "./workspace.js";

function pid() {
  return state.project.id;
}

function baseName(path) {
  return String(path || "").replace(/[\\/]+$/, "").split(/[\\/]/).at(-1);
}

// Clickable /export/<stamp>/<file> download links for the status line.
function exportLinks(folder, files) {
  const stamp = baseName(folder);
  return files.map((file) => ({
    href: `/api/canvas/projects/${pid()}/export/${encodeURIComponent(stamp)}/${encodeURIComponent(file)}`,
    label: file,
  }));
}

function fileToBase64(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error("could not read file"));
    reader.onload = () => resolve(String(reader.result).split(",")[1] || "");
    reader.readAsDataURL(file);
  });
}

// ---- images ------------------------------------------------------------------

// Add one or more image files. When `worldPoint` is given (a drop point), files
// are placed there, each offset by +20px so multiples don't stack exactly. A single
// file goes through addImage (POST /images); MULTIPLE files go through the batched
// addImages op (POST /images-batch) so one drop/paste gesture = ONE journal entry / one
// undo (T0224 item 7 — the last gesture-audit fix from T0223's one-entry law).
export async function addImageFiles(files, worldPoint) {
  const list = [...files].filter((file) => file && file.type && file.type.startsWith("image/"));
  if (!list.length || !state.project) return;
  const placement = (i) => (worldPoint ? { x: Math.round(worldPoint.x + i * 20), y: Math.round(worldPoint.y + i * 20) } : {});
  try {
    if (list.length === 1) {
      setStatus(`Uploading ${list[0].name}...`);
      const bytes_base64 = await fileToBase64(list[0]);
      const result = await api("POST", `/projects/${pid()}/images`, { name: list[0].name, bytes_base64, ...placement(0) });
      selectOnly(result.element.id);
      applyMutation(result, "Added 1 image.");
      return;
    }
    setStatus(`Uploading ${list.length} images...`);
    const images = [];
    for (let i = 0; i < list.length; i += 1) {
      images.push({ name: list[i].name, bytes_base64: await fileToBase64(list[i]), ...placement(i) });
    }
    const result = await api("POST", `/projects/${pid()}/images-batch`, { images });
    // Select the LAST added image (parity with the old loop's trailing selectOnly).
    const last = result.elements && result.elements[result.elements.length - 1];
    if (last) selectOnly(last.id);
    applyMutation(result, `Added ${list.length} images.`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- text --------------------------------------------------------------------

// Add a text element at a world point (the T tool click). Returns the created
// element so the caller can immediately open its inline editor. One journaled
// addText op; the op validates style against fonts.json.
export async function addTextAt(worldPoint, { content, style } = {}) {
  if (!state.project) return null;
  try {
    const body = { x: Math.round(worldPoint.x), y: Math.round(worldPoint.y) };
    if (content !== undefined) body.content = content;
    if (style !== undefined) body.style = style;
    const result = await api("POST", `/projects/${pid()}/text`, body);
    if (result && result.element) selectOnly(result.element.id);
    applyMutation(result, "Added text.");
    return result ? result.element : null;
  } catch (error) {
    setStatus(error.message, true);
    return null;
  }
}

// Patch a text element's content and/or style in ONE journaled patchElement, always
// re-measuring the auto-width box with the RESULTING content+style so the stored w/h
// (selection/marquee bookkeeping) stays in sync from the same entry — no extra journal
// steps. `stylePatch` is a PARTIAL style; the op shallow-merges + validates it, and we
// merge the same way locally only to measure.
export async function patchTextElement(id, { content, style } = {}, message) {
  const element = elementById(id);
  if (!element || element.type !== "text") return;
  const nextContent = content !== undefined ? content : element.content;
  let nextStyle = element.style;
  if (style !== undefined) {
    try {
      nextStyle = mergeTextStyle(element.style, style, getFontManifest());
    } catch (error) {
      setStatus(error.message, true);
      return;
    }
  }
  const box = measureTextBox(nextContent, nextStyle);
  const patch = { w: box.w, h: box.h };
  if (content !== undefined) patch.content = content;
  if (style !== undefined) patch.style = style;
  try {
    applyMutation(await api("PATCH", `/projects/${pid()}/elements/${id}`, patch), message);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- notes (T0268) -----------------------------------------------------------

// Add a note card at a world point (the N tool click / context-menu "New note"). Returns
// the created element so the caller can immediately open its inline editor. One journaled
// addNote op; the op validates style + background loudly and applies the default sticky
// box + yellow fill. The note is a canvas annotation — excluded from renderGroup/exportProject.
export async function addNoteAt(worldPoint, { content, style, background, w, h } = {}) {
  if (!state.project) return null;
  try {
    const body = { x: Math.round(worldPoint.x), y: Math.round(worldPoint.y) };
    if (content !== undefined) body.content = content;
    if (style !== undefined) body.style = style;
    if (background !== undefined) body.background = background;
    if (w !== undefined) body.w = w;
    if (h !== undefined) body.h = h;
    const result = await api("POST", `/projects/${pid()}/note`, body);
    if (result && result.element) selectOnly(result.element.id);
    applyMutation(result, "Added note.");
    return result ? result.element : null;
  } catch (error) {
    setStatus(error.message, true);
    return null;
  }
}

// Patch a note's content ONLY (the inline-editor commit). The box is user-fixed, so unlike
// text there is NO re-measured box here — just the content string, in ONE journaled entry.
export async function patchNoteContent(id, content, message) {
  const element = elementById(id);
  if (!element || element.type !== "note") return;
  try {
    applyMutation(await api("PATCH", `/projects/${pid()}/elements/${id}`, { content }), message);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Patch a note's style (a partial note font-subset object) — the inspector's Text section.
// The op shallow-merges + validates it against fonts.json (loud on a bad family/weight/
// align/color/size); one journaled patchElement per commit, mirrors patchTextElement.
export async function patchNoteStyle(id, style, message) {
  const element = elementById(id);
  if (!element || element.type !== "note") return;
  try {
    applyMutation(await api("PATCH", `/projects/${pid()}/elements/${id}`, { style }), message);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Set/clear a note's background fill (one journaled patchElement): null = no fill,
// {type:"color", color:"#rrggbb"} = solid. The op validates the value and refuses it on a
// non-note element (background is note-only). Mirrors setGroupBackground exactly.
export async function setNoteBackground(id, background) {
  await patchElementBox(id, { background });
}

// Paste an image blob from the clipboard at the current viewport center.
export async function pasteImageBlob(blob) {
  if (!blob || !state.project) return;
  const center = screenToImagePoint({ x: state.cssWidth / 2, y: state.cssHeight / 2 }, state.viewport);
  await addImageFiles([new File([blob], `pasted-${Date.now()}.png`, { type: blob.type || "image/png" })], center);
}

// ---- element ops -------------------------------------------------------------

export async function patchElementBox(id, patch) {
  try {
    applyMutation(await api("PATCH", `/projects/${pid()}/elements/${id}`, patch));
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function renameElement(id, name) {
  await patchElementBox(id, { name });
}

// Patch SEVERAL elements with the SAME patch, in ONE journaled gesture (elements-set) —
// e.g. the multi-image "Приглушить N images" dim preset (T0273). Same per-field rules as
// patchElementBox, applied identically to every id; one undo restores every element.
export async function patchElementsBatch(ids, patch, message) {
  if (!ids || !ids.length) return;
  try {
    const patches = ids.map((elementId) => ({ elementId, ...patch }));
    applyMutation(await api("POST", `/projects/${pid()}/elements-set`, { patches }), message);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Toggle one flip axis on an IMAGE element (T0232 increment 3a) — an additive boolean
// flag (image-only; validated at the op layer). Both the inspector's Flip H/Flip V
// buttons and the element context menu's "Flip horizontal"/"Flip vertical" drive this
// one path, so they never drift out of sync with each other or with the CLI/agent
// (element-set --flip-h/--flip-v flows through the SAME patchElement fields).
export async function toggleElementFlip(id, axis) {
  const element = elementById(id);
  if (!element) return;
  const key = axis === "v" ? "flipV" : "flipH";
  await patchElementBox(id, { [key]: !element[key] });
}

export async function setElementVisible(id, visible) {
  await patchElementBox(id, { visible });
}

export async function deleteElements(ids) {
  if (!ids.length) return;
  try {
    // ONE batched op = one HTTP call + one journal entry (a single Ctrl+Z restores
    // the whole multi-delete), instead of the old N sequential DELETEs.
    const result = await api("POST", `/projects/${pid()}/elements-remove`, { elementIds: ids });
    clearSelection();
    applyMutation(result, `Removed ${ids.length} element(s) (image files kept on disk).`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

export function deleteSelectedElements() {
  return deleteElements([...state.selectedIds]);
}

// Batched mixed delete (elements + whole group subtrees) in ONE journaled deleteNodes op —
// the Delete key on a multi-group or mixed selection. One HTTP call, one undo deep-restores
// every group and element at its exact z-slot.
export async function deleteNodes(ids) {
  if (!ids.length) return;
  try {
    const result = await api("POST", `/projects/${pid()}/nodes-delete`, { nodeIds: ids });
    clearSelection();
    const g = (result.removedGroups || []).length;
    const e = (result.removedElements || []).length;
    applyMutation(result, `Deleted ${g} group(s) + ${e} element(s). Undo restores all.`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- clipboard: copy / paste / duplicate of nodes (T0227) --------------------
//
// Figma-like copy/paste/duplicate for canvas objects. The COPY BUFFER (state.clipboard) is
// PAGE VIEW-STATE (never journaled): Ctrl+C serializes the current selection's deep subtree
// via the shared pure tree.buildNodesSpec. The journaled gesture is the PASTE/DUPLICATE
// (pasteNodes/duplicateNodes op) — one HTTP call, one undo. Ctrl+V is owned by the window
// "paste" event (dnd.js) so the OS-image path and the node buffer never both fire.

const PASTE_OFFSET = 16;

// Ctrl+C: snapshot the selection (elements AND groups, mixed) into the page copy buffer.
// Image element specs reference their immutable content-addressed file, so the buffer stays
// valid even after the source nodes are deleted. Nothing is written to disk here.
export function copySelection() {
  if (!state.project) return;
  const ids = selectedNodeIds();
  if (!ids.length) return;
  try {
    const spec = buildNodesSpec(state.project, ids);
    state.clipboard = { spec, pastes: 0 };
    setStatus(`Copied ${spec.nodes.length} item(s). Ctrl+V to paste.`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Ctrl+V (from the dnd.js paste-event owner): instantiate the copy buffer as ONE journaled
// op into the CURRENT scope (enteredGroupId). Each repeat paste steps the offset again so
// stacked pastes don't overlap exactly; the pasted copy is selected (Figma behavior).
export async function pasteClipboard() {
  if (!state.project || !state.clipboard || !state.clipboard.spec) return;
  state.clipboard.pastes += 1;
  const step = PASTE_OFFSET * state.clipboard.pastes;
  try {
    const result = await api("POST", `/projects/${pid()}/nodes-paste`, {
      spec: state.clipboard.spec,
      dx: step,
      dy: step,
      scopeId: state.enteredGroupId ?? null,
    });
    selectPastedNodes(result);
    applyMutation(result, `Pasted ${result.count} item(s).`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Ctrl+D: duplicate the current selection in place (+offset) as ONE journaled op. Builds
// the spec server-side from the live node ids (duplicateNodes) and selects the copy.
export async function duplicateSelection() {
  if (!state.project) return;
  const ids = selectedNodeIds();
  if (!ids.length) return;
  try {
    const result = await api("POST", `/projects/${pid()}/nodes-duplicate`, {
      nodeIds: ids,
      dx: PASTE_OFFSET,
      dy: PASTE_OFFSET,
      scopeId: state.enteredGroupId ?? null,
    });
    selectPastedNodes(result);
    applyMutation(result, `Duplicated ${result.count} item(s).`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Select the pasted/duplicated ROOTS (top-level new nodes) so the copy is ready to move.
// Runs before applyMutation, whose ingestProject prunes to still-alive ids (the new ids
// survive). Elements and groups split into the two selection sets.
function selectPastedNodes(result) {
  state.regionEditId = null;
  state.selectedRegionIds = new Set();
  state.selectedIds = new Set(result.elementIds || []);
  state.selectedGroupIds = new Set(result.groupIds || []);
  state.selectedGroupId =
    state.selectedGroupIds.size === 1 && state.selectedIds.size === 0 ? [...state.selectedGroupIds][0] : null;
}

// ---- z-order (element + group ordering) --------------------------------------
//
// z-order acts on a NODE — an element OR a group. The op reorders it among its MERGED
// same-scope siblings (elements + groups); these page helpers only translate a
// Figma-style intent (forward/backward/front/back) into an absolute merged-sibling index
// via the shared tree math, then call the one reorderNode op. Elements and groups share
// one id namespace, so a single helper set serves both.

function nodeById(id) {
  return elementById(id) || groupById(id);
}

// Merged siblings (elements + groups) of a node in computed back → front order, or null
// when the id is unknown. The same list tree.mjs paints from, so the index math agrees
// with what the user sees.
function nodeSiblings(id) {
  const node = nodeById(id);
  if (!node) return null;
  return orderedChildren(state.project, nodeScope(state.project, node));
}

// Move any node (element or group) to a target merged-sibling index (0 = back / painted
// first). One journaled op; undo restores the exact previous order.
export async function reorderNodeTo(id, index) {
  try {
    applyMutation(await api("POST", `/projects/${pid()}/nodes/${id}/reorder`, { index }));
  } catch (error) {
    setStatus(error.message, true);
  }
}

function nudgeZ(id, delta) {
  const siblings = nodeSiblings(id);
  if (!siblings) return undefined;
  const from = siblings.findIndex((sibling) => sibling.id === id);
  const target = from + delta;
  if (target < 0 || target >= siblings.length) return undefined; // already at the edge
  return reorderNodeTo(id, target);
}

function edgeZ(id, edge) {
  const siblings = nodeSiblings(id);
  if (!siblings) return undefined;
  const from = siblings.findIndex((sibling) => sibling.id === id);
  const target = edge === "front" ? siblings.length - 1 : 0;
  if (from === target) return undefined;
  return reorderNodeTo(id, target);
}

export function bringNodeForward(id) {
  return nudgeZ(id, +1);
}
export function sendNodeBackward(id) {
  return nudgeZ(id, -1);
}
export function bringNodeToFront(id) {
  return edgeZ(id, "front");
}
export function sendNodeToBack(id) {
  return edgeZ(id, "back");
}

// The current multi-node selection as one id list (loose elements + whole groups) — the
// unit z-order and mixed-move gestures act on. Elements and groups share one id namespace.
export function selectedNodeIds() {
  return [...state.selectedIds, ...state.selectedGroupIds];
}

// Multi-selection z-order: move the selected same-scope siblings as ONE block (Figma
// semantics; relative order preserved), cross-scope applied per scope — ONE journaled
// reorderNodes op (one undo). `direction` is front|back|forward|backward. Backs the
// multi-select Ctrl+[/] shortcuts and the Order menu on a multi-selection.
export async function reorderNodesBy(direction) {
  const ids = selectedNodeIds();
  if (ids.length < 2) return;
  try {
    applyMutation(await api("POST", `/projects/${pid()}/nodes-reorder`, { nodeIds: ids, direction }));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Commit a mixed selection move (loose elements + one or more group frames) as ONE
// journaled moveNodes op: each group cascades its subtree, one HTTP call, one undo restores
// every position. `moves` is [{nodeId, x, y}] of absolute top-lefts. Backs the canvas
// marquee/multi-select drag commit (workspace.js).
export async function moveNodesTo(moves) {
  if (!moves.length) return;
  try {
    applyMutation(await api("POST", `/projects/${pid()}/nodes-move`, { moves }), "Moved selection.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- align / distribute (T0232 increment 1) ----------------------------------
//
// Align 2+ nodes (elements AND/OR groups, mixed OK) — or exactly 1 node that lives inside
// a parent group — to a shared reference frame in ONE journaled alignNodes op (one undo
// restores every moved node). `reference` is left to the op's "auto" default (Figma
// semantics: 2+ nodes -> the selection's union bbox; 1 node inside a group -> that group's
// frame), so the page never has to pick a mode itself. Backs the inspector's Align row.
export async function alignSelection(nodeIds, align) {
  if (!nodeIds || !nodeIds.length) return;
  try {
    applyMutation(await api("POST", `/projects/${pid()}/nodes-align`, { nodeIds, align }), "Aligned selection.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Distribute 3+ nodes (elements AND/OR groups, mixed OK) with equal gaps along an axis in
// ONE journaled distributeNodes op (one undo restores every moved node). Backs the
// inspector's Distribute buttons.
export async function distributeSelection(nodeIds, axis) {
  if (!nodeIds || nodeIds.length < 3) return;
  try {
    applyMutation(await api("POST", `/projects/${pid()}/nodes-distribute`, { nodeIds, axis }), "Distributed selection.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Detect is a long (python-backed) op: it runs through the limiter so at most N=2
// python spawns are ever in flight, its progress toast resolves into the result, and
// the triggering `control` (the inspector Detect button) is disabled while in flight.
// The canvas stays fully interactive throughout.
export async function detectRegionsFor(id, control) {
  await runLongOp(
    "Detecting regions…",
    async () => {
      const result = await api("POST", `/projects/${pid()}/detect-regions`, { elementId: id });
      applyMutation(result); // update the page; the result toast carries the confirmation
      return { kind: "success", message: `Detected ${result.regions.length} region(s).` };
    },
    { control },
  );
}

// Slice all of an element's regions, or only `regionIds` when given (per-region
// slice from the region menu / inspector "Slice selected region(s)"). The op crops
// the STORED region rects verbatim, so edited and hand-drawn regions crop exactly.
// Long (python-backed) op — same limiter/spinner/disable treatment as detect.
export async function sliceRegionsFor(id, regionIds, control) {
  await runLongOp(
    "Slicing regions…",
    async () => {
      const body = { elementId: id };
      if (Array.isArray(regionIds) && regionIds.length) body.regionIds = regionIds;
      const result = await api("POST", `/projects/${pid()}/slice`, body);
      state.selectedGroupId = null;
      state.selectedRegionIds = new Set();
      state.selectedIds = new Set(result.created.map((element) => element.id));
      applyMutation(result);
      return { kind: "success", message: `Sliced ${result.created.length} region(s) into new elements.` };
    },
    { control },
  );
}

// Alpha-cutout the selected image element via the image-tools matte pipeline: mints the
// cutout as a NEW element beside the source in ONE journaled op (T0336 — the original element
// and its pixels are never touched, so the lead can compare keyers side by side; undo removes
// the copy). The new element is selected on success (mirrors alphaDualPlateGenerateFor).
// `method` is "auto" (route; refuses a dual-plate soft zone loudly),
// "matte" (force key_matte), "corridorkey" (T0261 — the neural green-screen matte for soft
// glow art; green-only + whole-element, ~15s GPU), "vitmatte" (T0335 — neural thin-detail /
// 2nd-choice-glow matte on a green/magenta key, own GPU venv, ~1-3s, whole-element only), or
// "birefnet" (T0335 — SOD cutout for an arbitrary/unknown background with no key, CPU ~25s,
// whole-element only). `regionIds`, when given, keys ONLY inside those stored regions (rest
// untouched; never passed for corridorkey/vitmatte/birefnet — all whole-element only). Long
// (python-backed) op — same limiter/spinner/disable treatment as slice + the busy toast covers
// the neural wait (up to ~25s for birefnet); the triggering `control` is disabled while in flight.
export async function alphaCutoutFor(id, method, regionIds, control) {
  await runLongOp(
    "Alpha cutout…",
    async () => {
      const body = { elementId: id, method };
      if (Array.isArray(regionIds) && regionIds.length) body.regions = regionIds;
      const result = await api("POST", `/projects/${pid()}/alpha`, body);
      // The cutout is a NEW element beside the source — select it so the lead sees the
      // result immediately (mirrors alphaDualPlateGenerateFor); the source stays untouched.
      selectOnly(result.element.id);
      applyMutation(result);
      const scope = Array.isArray(regionIds) && regionIds.length ? `${regionIds.length} region(s)` : "element";
      return { kind: "success", message: `Alpha cutout copy "${result.element.name}" (${scope}, ${result.method}).` };
    },
    { control },
  );
}

// Alpha-cutout a MULTI-selection of image elements as ONE operation (T0230 — "Apply to N
// images"): every element keys its own CURRENT pixels through the same pipeline as the
// single-element Alpha row, but the whole batch mints N NEW copies beside their sources in
// ONE journaled op (T0336 — sources untouched; one Ctrl+Z removes every copy). No region
// scoping here — regions stay single-element (select one image and use its own Regions
// section). Atomic: if any element refuses (dual-plate guard, etc.) the whole batch is
// rejected and NOTHING changes. The first minted copy is selected on success (mirrors the
// batch-mint precedent in generateFakeShotsFor). Long (python-backed) op — same limiter/
// spinner/disable treatment as the single-element row.
export async function alphaCutoutBatchFor(ids, method, control) {
  await runLongOp(
    "Alpha cutout…",
    async () => {
      const result = await api("POST", `/projects/${pid()}/alpha`, { elementIds: ids, method });
      const minted = result.elements || [];
      if (minted[0]) selectOnly(minted[0].id);
      applyMutation(result);
      return { kind: "success", message: `Alpha cutout: ${minted.length} new copies (${result.method}).` };
    },
    { control },
  );
}

// Dual-plate alpha cutout (T0237): TWO selected image elements — the SAME art rendered on
// a white plate and a black plate, either order (the tool auto-detects roles by overall
// brightness) — key into ONE NEW cut element via ops.alphaDualPlate. Non-destructive: both
// plate elements stay on the canvas untouched (the lead deletes them himself once happy
// with the result); one Ctrl+Z removes the new element. The new element is selected so the
// lead sees the result immediately (mirrors addImageFiles' single-add selectOnly). Long
// (python-backed) op — same runLongOp limiter/spinner/disable treatment as the other alpha rows.
export async function alphaDualPlateFor(ids, control) {
  await runLongOp(
    "Dual-plate alpha…",
    async () => {
      const result = await api("POST", `/projects/${pid()}/alpha-dual`, { elementIds: ids });
      selectOnly(result.element.id);
      applyMutation(result);
      return { kind: "success", message: `Dual-plate alpha created "${result.element.name}".` };
    },
    { control },
  );
}

// AUTOMATIC dual-plate alpha from ONE element (T0238/T0247 — the inspector Alpha section's
// explicit "Dual-plate (generate)" method): the element's own pixels are the light plate
// (the op refuses loudly on a non-flat/non-light background BEFORE any codex spend), the
// dark plate is generated as a subject-locked codex edit, and the gated/aligned cut lands
// as ONE NEW element beside the source (plates + prompt + verdict in its meta.alpha).
// Codex generation runs minutes, not seconds — same runLongOp limiter/spinner/disable
// treatment, just a longer-lived toast. The new element is selected on success (mirrors
// alphaDualPlateFor).
export async function alphaDualPlateGenerateFor(id, control) {
  await runLongOp(
    "Dual-plate generate… (codex, ~2-4 min)",
    async () => {
      const result = await api("POST", `/projects/${pid()}/alpha-dual-generate`, { elementId: id });
      selectOnly(result.element.id);
      applyMutation(result);
      return { kind: "success", message: `Dual-plate alpha created "${result.element.name}".` };
    },
    { control },
  );
}

// Mint a normal element from a dual-plate-generate plate's STORED file (T0238) — the
// inspector's per-plate "Add to canvas" button. No re-upload/no re-fetch: the server reads
// the existing content-addressed file directly (POST /images-from-file). Fast metadata op
// (no python), so plain try/catch + applyMutation like patchElementBox, not runLongOp.
export async function addPlateFromFile(src, name, placement, control) {
  if (control) control.disabled = true;
  try {
    const result = await api("POST", `/projects/${pid()}/images-from-file`, { src, name, ...placement });
    selectOnly(result.element.id);
    applyMutation(result, `Added "${result.element.name}" to the canvas.`);
  } catch (error) {
    setStatus(error.message, true);
  } finally {
    if (control) control.disabled = false;
  }
}

// ---- cleanup: Quantize + Denoise (T0207) --------------------------------------
//
// Preview is a READ-ONLY probe against the element's CURRENT pixels — the server
// (ops.cleanupPreview) writes nothing (no files/ entry, no journal line), so this is
// safe to call on every debounced slider/segmented change from the inspector's Cleanup
// section. Deliberately NOT a runLongOp: a progress toast per keystroke would be noise —
// the inspector owns its own tiny spinner/disabled state instead. Throws on failure; the
// caller decides how to surface it (setStatus + drop the preview, per the section's spec).
export async function cleanupPreviewAction(elementId, tool, params) {
  return api("POST", `/projects/${pid()}/elements/${elementId}/cleanup-preview`, { tool, params });
}

// Apply the given tool+params as ONE journaled mutation (new content-addressed file +
// element.src swap + additive element.meta.cleanup) — same runLongOp limiter/spinner/
// disable treatment as Alpha cutout. quantize/denoise carry no randomness, so this
// reproduces byte-identical bytes to whatever the caller's last preview already showed.
export async function cleanupApplyAction(elementId, tool, params, control) {
  const label = tool === "denoise" ? "Denoise" : "Quantize";
  await runLongOp(
    `${label}…`,
    async () => {
      const result = await api("POST", `/projects/${pid()}/elements/${elementId}/cleanup`, { tool, params });
      applyMutation(result);
      return { kind: "success", message: `${label} applied.` };
    },
    { control },
  );
}

// ---- filters bake (T0274 "Apply": rasterize filters+opacity into pixels) -----
//
// Photoshop-rasterize semantics ("принял -> получил новый арт -> ползунки снова в 0"):
// burns the element's CURRENT non-destructive filters+opacity (T0273/T0260) into a NEW
// content-addressed source file as ONE journaled mutation, then clears both fields — the
// inspector's Filters section re-renders with every slider back at its default. Same
// runLongOp limiter/spinner/disable treatment as Alpha cutout/Cleanup (python-backed).
export async function bakeFiltersFor(elementId, control) {
  await runLongOp(
    "Apply filters…",
    async () => {
      const result = await api("POST", `/projects/${pid()}/elements/${elementId}/filters-bake`, {});
      applyMutation(result);
      return { kind: "success", message: "Filters applied — new art, sliders reset." };
    },
    { control },
  );
}

// Batch "Apply filters on N images" (T0274): every selected image bakes its own CURRENT
// filters+opacity in ONE journaled op (one Ctrl+Z restores every element). Atomic — mirrors
// alphaCutoutBatchFor.
export async function bakeFiltersBatchFor(ids, control) {
  await runLongOp(
    "Apply filters…",
    async () => {
      const result = await api("POST", `/projects/${pid()}/filters-bake`, { elementIds: ids });
      applyMutation(result);
      return { kind: "success", message: `Filters applied to ${ids.length} images.` };
    },
    { control },
  );
}

// ---- regions -----------------------------------------------------------------

// Replace an element's regions in one journaled setRegions op. Region edits
// (move/resize/rubber-band) accumulate locally during the gesture and commit here
// exactly once, so undo steps back a whole gesture rather than every mouse move.
export async function setRegionsFor(elementId, regions, message) {
  try {
    applyMutation(await api("PUT", `/projects/${pid()}/elements/${elementId}/regions`, { regions }), message);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Rename a region (inline edit in the layers tree / inspector). Journaled via
// setRegions; an empty name is dropped by the op.
export async function renameRegion(elementId, regionId, name) {
  const element = elements().find((item) => item.id === elementId);
  if (!element) return;
  const regions = (element.regions || []).map((region) =>
    region.id === regionId ? { ...region, name } : region,
  );
  try {
    applyMutation(await api("PUT", `/projects/${pid()}/elements/${elementId}/regions`, { regions }));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Delete the selected regions from the isolated (mode B) element (Delete key /
// inspector ×). One setRegions op with the survivors; stays in mode B.
export async function deleteSelectedRegions() {
  const element = regionEditElement();
  if (!element) return;
  const ids = new Set(state.selectedRegionIds);
  if (!ids.size) return;
  const remaining = (element.regions || []).filter((region) => !ids.has(region.id));
  try {
    const result = await api("PUT", `/projects/${pid()}/elements/${element.id}/regions`, { regions: remaining });
    state.selectedRegionIds = new Set();
    applyMutation(result, `Removed ${ids.size} region(s).`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function deleteRegion(elementId, regionId) {
  const element = elements().find((item) => item.id === elementId);
  if (!element) return;
  const remaining = (element.regions || []).filter((region) => region.id !== regionId);
  try {
    const result = await api("PUT", `/projects/${pid()}/elements/${elementId}/regions`, { regions: remaining });
    state.selectedRegionIds.delete(regionId);
    applyMutation(result, "Removed region.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- slice-9 (T0233 Packet 2) --------------------------------------------------

// Set or clear an image element's 9-slice insets in ONE journaled setSlice9 op — the
// inspector Slice-9 section's Enable/Clear buttons and its per-field live edits all
// commit through here (tool parity with the CLI's slice9-set). `insets` an object
// {left,top,right,bottom,scale?} -> validated + stored on element.slice9;
// `insets === null` clears the field. The op's loud validation (a corner pair that
// would consume the source axis, an out-of-range scale, a non-image element)
// surfaces as an error toast — mirrors setRegionsFor/setExportRows exactly.
export async function setSlice9Action(elementId, insets) {
  try {
    applyMutation(await api("PUT", `/projects/${pid()}/elements/${elementId}/slice9`, { insets }));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- animation (T0260) ---------------------------------------------------------

// Set or clear an element's procedural animation in ONE journaled setElementAnimation op —
// the inspector Animation section's "Add sample", Edit-JSON Save, and Clear all commit through
// here (tool parity with the CLI's animation-set and the agent API). `animationOrNull` is a
// validated {channels:[...]} spec, or null to clear the field (image + text). The op's own
// loud validation (a bad channel/prop/kind, a duplicate prop, a non-image/text element)
// surfaces as an error toast — mirrors setSlice9Action exactly, one route segment over.
export async function setElementAnimationAction(elementId, animationOrNull) {
  try {
    applyMutation(await api("PUT", `/projects/${pid()}/elements/${elementId}/animation`, { animation: animationOrNull }));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// T0264: the text->animation bridge. The Animation section's [Animate] input describes the
// motion ("крылья медленно машут"); ONE codex TEXT/VISION call authors a fresh spec or
// minimally patches the existing one. Same runLongOp treatment as extractElementAction (codex
// = real seconds/minutes; a failure surfaces as an error toast and the typed text survives
// because no applyMutation re-renders the input). ON SUCCESS: applyMutation, then AUTO-PLAY the
// preview so the result is immediately visible on the canvas — the lead's demo moment.
export async function animateElementFromTextAction(elementId, text, control) {
  await runLongOp(
    "Animating… (codex)",
    async () => {
      const result = await api("POST", `/projects/${pid()}/elements/${elementId}/animate`, { text });
      applyMutation(result); // rebuilds the inspector (Play button reads "not previewing" yet)
      if (result.element) {
        startAnimationPreview(result.element.id);
        refresh(); // re-render so the Animation section's Play button now reads "Stop" (it IS playing)
      }
      return { kind: "success", message: "Animation applied." };
    },
    { control },
  );
}

// ---- export ------------------------------------------------------------------

// Replace an element's Figma-style export rows via the journaled setExportSettings
// op (the inspector Export section commits one entry per edit; undoable).
export async function setExportRows(elementId, rows) {
  try {
    applyMutation(await api("PUT", `/projects/${pid()}/elements/${elementId}/export`, { rows }));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// A filesystem-friendly base for the save dialog's suggested name (the lead can still
// rename in the dialog). Drops a trailing image extension (an element named "sheet.png"
// should suggest "sheet.zip", not "sheet.png.zip") and strips characters Windows/macOS
// reject in a file name.
function suggestedBase(name) {
  const cleaned = String(name || "")
    .trim()
    .replace(/\.(png|jpe?g|webp|gif)$/i, "")
    .replace(/[\\/:*?"<>|]+/g, " ")
    .replace(/\s+/g, " ")
    .trim();
  return cleaned || "export";
}

// Deliver a finished export to disk and return the RESULT TOAST spec (the caller's
// runLongOp resolves the progress toast into it). Figma-style destination (T0229):
//   - ONE output  -> a save-FILE dialog seeded with the file's own name (editable);
//   - 2+ outputs  -> ONE STORE-mode .zip (built server-side over the export-zip route),
//     saved via the same dialog, suggested "<zipBaseName>.zip".
// A dialog CANCEL is a quiet info toast; any other failure throws (runLongOp turns it
// into an error toast — never swallowed); a browser without showSaveFilePicker downloads
// with the same suggested name. The pinned result names what was saved (no directory
// handle) and keeps the still-valid per-file server links.
async function deliverExport(result, { zipBaseName } = {}) {
  const files = (result.items || result.screens || []).map((entry) => entry.file).filter(Boolean);
  const stamp = baseName(result.folder);
  if (!files.length) return { kind: "info", message: "Nothing to export." };
  const links = exportLinks(result.folder, files);
  const canceled = { kind: "info", message: "Отмена в диалоге — экспорт отменён." };

  if (files.length === 1) {
    // Single output: fetch the one image and save it under its own (editable) name.
    const file = files[0];
    const url = `/api/canvas/projects/${pid()}/export/${encodeURIComponent(stamp)}/${encodeURIComponent(file)}`;
    const response = await fetch(url);
    if (!response.ok) throw new Error(`could not fetch ${file} (${response.status})`);
    const blob = await response.blob();
    const dot = file.lastIndexOf(".");
    const ext = dot >= 0 ? file.slice(dot) : "";
    const types = ext
      ? [{ description: `${ext.slice(1).toUpperCase()} file`, accept: { [blob.type || "application/octet-stream"]: [ext] } }]
      : [];
    const saved = await saveBlobToFile(blob, file, types);
    if (saved.canceled) return canceled;
    return { kind: "pinned", message: `Saved “${saved.name}”`, links };
  }

  // Several outputs: build ONE zip server-side, then save it via the same dialog.
  const zipUrl = `/api/canvas/projects/${pid()}/export-zip/${encodeURIComponent(stamp)}`;
  const response = await fetch(zipUrl);
  if (!response.ok) throw new Error(`could not build the zip archive (${response.status})`);
  const blob = await response.blob();
  const suggestedName = `${suggestedBase(zipBaseName)}.zip`;
  const types = [{ description: "ZIP archive", accept: { "application/zip": [".zip"] } }];
  const saved = await saveBlobToFile(blob, suggestedName, types);
  if (saved.canceled) return canceled;
  return { kind: "pinned", message: `Saved “${saved.name}” (${files.length} files)`, links };
}

// Export the given elements, each honoring its own persisted export rows (or the
// implicit 1x-png default), then deliver to the chosen destination. Long op (the export
// spawns Python for anything but the 1x-png copy fast path): limiter + spinner + the
// inspector Export button (`control`) disabled while in flight.
export async function exportElementIds(ids, control) {
  if (!ids.length) return;
  // The multi-output zip name: a single selected element's name, else the project title.
  const one = ids.length === 1 ? elementById(ids[0]) : null;
  const zipBaseName = (one && one.name) || (state.project && state.project.title) || "export";
  await runLongOp(
    "Exporting…",
    async () => {
      const result = await api("POST", `/projects/${pid()}/export`, { elementIds: ids });
      return deliverExport(result, { zipBaseName });
    },
    { control },
  );
}

// No selection -> project export: render every visible screen at 1x png. Long op.
export async function exportProjectAction(control) {
  const zipBaseName = (state.project && state.project.title) || "export";
  await runLongOp(
    "Exporting project…",
    async () => {
      const result = await api("POST", `/projects/${pid()}/export`, { project: true });
      return deliverExport(result, { zipBaseName });
    },
    { control },
  );
}

// ---- group ops ------------------------------------------------------

export async function createGroupFromSelection(name) {
  const ids = [...state.selectedIds];
  if (ids.length < 2) return;
  try {
    const result = await api("POST", `/projects/${pid()}/groups`, { name: name || "New group", fromElements: ids });
    clearSelection();
    state.selectedGroupId = result.group.id;
    applyMutation(result, `Grouped ${ids.length} element(s) into "${result.group.name}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

const DEFAULT_GROUP_SIZE = { w: 960, h: 540 };

// Group the current selection (Ctrl/Cmd+G, layers/canvas context menus), or —
// when nothing is selected — create an empty default-size group centered on
// the current viewport via the same createGroup
// op the CLI's group-create (no --elements) already uses.
export async function createGroupOrDefault(name) {
  if (state.selectedIds.size > 0) {
    await createGroupFromSelection(name);
    return;
  }
  try {
    const center = screenToImagePoint({ x: state.cssWidth / 2, y: state.cssHeight / 2 }, state.viewport);
    const body = {
      name: name || "New group",
      x: Math.round(center.x - DEFAULT_GROUP_SIZE.w / 2),
      y: Math.round(center.y - DEFAULT_GROUP_SIZE.h / 2),
      w: DEFAULT_GROUP_SIZE.w,
      h: DEFAULT_GROUP_SIZE.h,
    };
    const result = await api("POST", `/projects/${pid()}/groups`, body);
    state.selectedGroupId = result.group.id;
    applyMutation(result, `Created group "${result.group.name}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Create a new empty group NESTED inside the given parent group (context-menu
// "Create group inside"): a centered box at half the parent's size, so it lands
// visibly within the frame. Same journaled createGroup op, with parentId.
export async function createGroupInside(parentGroupId) {
  const parent = state.project?.groups?.find((g) => g.id === parentGroupId);
  if (!parent) return;
  try {
    const w = Math.max(80, Math.round(parent.w / 2));
    const h = Math.max(80, Math.round(parent.h / 2));
    const body = {
      name: "New group",
      x: Math.round(parent.x + (parent.w - w) / 2),
      y: Math.round(parent.y + (parent.h - h) / 2),
      w,
      h,
      parentId: parentGroupId,
    };
    const result = await api("POST", `/projects/${pid()}/groups`, body);
    state.selectedGroupId = result.group.id;
    applyMutation(result, `Created group "${result.group.name}" inside "${parent.name || "Group"}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Nest a group under another group (parentId), or move it to top level (null), at an
// optional merged-sibling index (default = front of the destination scope). One
// journaled reparentGroup op. Used by the layers-panel group-row drag-to-nest and the
// "Move to group" submenu on a group.
export async function reparentGroupTo(groupId, parentId, index) {
  try {
    const body = { parentId: parentId ?? null };
    if (index !== undefined && index !== null) body.index = index;
    applyMutation(
      await api("POST", `/projects/${pid()}/groups/${groupId}/reparent`, body),
      parentId ? "Nested group." : "Moved group to top level.",
    );
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Move elements into a group (groupId) or out to top level (null). Used by the
// "Move to group" context submenu and the layers-panel row drag.
export async function assignElementsToGroup(elementIds, groupId) {
  const ids = [...elementIds];
  if (!ids.length) return;
  try {
    const result = await api("POST", `/projects/${pid()}/assign-group`, { elementIds: ids, groupId: groupId ?? null });
    applyMutation(result, groupId ? "Moved to group." : "Moved out of group.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function patchGroupBox(groupId, patch) {
  try {
    applyMutation(await api("PATCH", `/projects/${pid()}/groups/${groupId}`, patch));
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function renameGroup(groupId, name) {
  await patchGroupBox(groupId, { name });
}

export async function setGroupVisible(groupId, visible) {
  await patchGroupBox(groupId, { visible });
}

// Set/clear a group's background fill (one journaled patchGroup): null = None,
// {type:"color", color:"#rrggbb"} = Solid. Canvas + render honor it (render-time bg
// arg still overrides). The op validates the value (no silent fallback).
export async function setGroupBackground(groupId, background) {
  await patchGroupBox(groupId, { background });
}

// Toggle a group's clip-to-bounds flag (one journaled patchGroup): true clips members to
// the frame on canvas AND in the subgroup render; false clears it (stored as absent). The
// op validates it is a boolean (no silent fallback).
export async function setGroupClip(groupId, clip) {
  await patchGroupBox(groupId, { clip });
}

// Toggle a group's `screen` (export opt-in) flag (T0332 B1 — export flipped to opt-in:
// group.screen === true is the ONLY thing that makes a top-level visible group count as an
// exportable screen; exportProject/visibleScreenCount both gate on it). Mirrors setGroupClip
// exactly — one journaled patchGroup, false clears to an absent field.
export async function setGroupScreen(groupId, screen) {
  await patchGroupBox(groupId, { screen });
}

// ---- recipe cards (T0239 increment 1) -----------------------------------------

// Create a new recipe card: a group carrying an additive `recipe` blob (prompt/engine/
// params/style_ref — see ops.createRecipeCard). `worldPoint`, when given (the canvas
// context menu's click point), places the card there like addTextAt; omitted, the op's
// own default (0,0) applies. Plain fast op — no long-op queue, no generation yet.
export async function createRecipeCardAction(worldPoint) {
  if (!state.project) return;
  try {
    const body = {};
    if (worldPoint) {
      body.x = Math.round(worldPoint.x);
      body.y = Math.round(worldPoint.y);
    }
    const result = await api("POST", `/projects/${pid()}/recipe-cards`, body);
    state.selectedGroupId = result.group.id;
    applyMutation(result, `Created recipe card "${result.group.name}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Partial update of a card's `recipe` blob (prompt/expanded/use_expanded/engine/style_ref)
// — the Recipe inspector section's live edits. One journaled patchRecipe op; loud on a
// group without `recipe` (the op layer's guard — a plain group is not a card). The
// Expanded textarea, its Edit-modal Save, the "Send expanded" checkbox, and the Discard
// button all go through this SAME action ({expanded}/{use_expanded} respectively).
export async function patchRecipeAction(groupId, patch) {
  try {
    applyMutation(await api("PATCH", `/projects/${pid()}/recipe-cards/${groupId}`, patch));
    // A pack preview is a pure derivative of the recipe blob — after ANY successful edit of
    // THIS card the stashed spoilers are stale (pre-edit prompts) and must not keep rendering
    // as if they previewed the card's current state (review finding 2026-07-07). Dropping the
    // stash also drops the `pp:` inspectorSig token, so the panel rebuilds without them; the
    // lead re-clicks Preview (free) when he wants fresh prompts.
    if (state.packPreview && state.packPreview.cardId === groupId) state.packPreview = null;
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- Expand-prompt (T0239 increment 4) -----------------------------------------

// Expand a recipe card's short prompt into a labeled generation-prompt template via the
// ONE codex TEXT seam (tools/prompt_assist.mjs). Same runLongOp treatment as
// generateFromRecipeAction (codex = real seconds/minutes); writes recipe.expanded, one
// journal entry. The lead edits/discards the result himself afterward (the Expanded
// textarea/Edit modal/Discard button) — this action only produces the first draft.
export async function expandRecipePromptAction(groupId, control) {
  await runLongOp(
    "Expanding prompt… (codex)",
    async () => {
      const result = await api("POST", `/projects/${pid()}/recipe-cards/${groupId}/expand`, {});
      applyMutation(result);
      return { kind: "success", message: "Prompt expanded." };
    },
    { control },
  );
}

// ---- recipe generation (T0239 increment 2) -------------------------------------

// Generate from a recipe card (the Recipe inspector's Generate button): mints 1 new RAW
// image element for engine codex/gemini, or 2 side-by-side for engine "both" (R3 compare
// mode — "<card> codex" / "<card> agy"), beside the card frame, in the card's PARENT scope
// (never inside the card — a result can never become a ref feeding a future run, decision
// 8). Codex/agy generation runs minutes, not seconds — same runLongOp limiter/spinner/
// disable treatment as alphaDualPlateGenerateFor, just a longer/engine-aware label. The
// first minted element is selected on success (mirrors alphaDualPlateGenerateFor).
// engine="both" allows PARTIAL success (one engine failing still lands the other; the op
// does NOT throw) — `result.failed` names what did not land, surfaced as a pinned (sticky)
// toast instead of the plain success toast so it does not auto-dismiss unnoticed. Only
// EVERY engine failing throws, which runLongOp already renders as its own error toast.
//
// T0332 v2 (build_spec_pack_card_2026-07-07.md phase C): the SAME button/action also drives
// the PACK branch now — ops.generateFromRecipe branches on recipe.pack itself, so this action
// only forwards the two pack-only resume/force-regen options (`opts.runGroupId`/
// `opts.sheetSlug` — both undefined for a plain single-image call OR a fresh pack run; the
// inspector's Pack "Generate" button omits them, its per-sheet "Regenerate" button sets
// sheetSlug to the sheet element's own `.name`, which IS the expander's job.name
// verbatim — see commitPackSheetOutcome/storeAddImage — and runGroupId to that element's
// `.groupId`; no extra field needed) and renders a DIFFERENT result toast for a pack run
// (`result.results`/`result.last_run` — no `.elements`) vs the single-image branch's minted
// elements. `opts.busyLabel` overrides the static busy label (the inspector computes a
// "~N sheets" ESTIMATE client-side from recipe.pack.axes/vary before the real expander ever
// runs — see estimatePackSheetCount below). No live per-sheet "лист k/N" progress: ops.mjs
// commits each sheet as it lands, so the count is technically readable mid-flight by polling
// GET /projects/<id>, but there is no polling precedent anywhere in this file/toasts.js
// (runLongOp resolves once, from one await) and a correct poller would have to special-case
// resume/skip/forced-regen dedup just to count "done so far" — real new engineering, not a
// mechanical wire-up, so this stays the build-spec's own explicitly-endorsed fallback: a
// static busy state + a full result refresh once the whole run settles.
export async function generateFromRecipeAction(groupId, control, opts = {}) {
  await runLongOp(
    opts.busyLabel || "Generating… (codex/agy, minutes)",
    async () => {
      const body = {};
      if (opts.runGroupId) body.runGroupId = opts.runGroupId;
      if (opts.sheetSlug) body.sheetSlug = opts.sheetSlug;
      const result = await api("POST", `/projects/${pid()}/recipe-cards/${groupId}/generate`, body);
      if (Array.isArray(result.results)) {
        // Pack branch: no `.elements` to select (many sheets, not one/two) — the layers
        // panel already shows the fresh/updated run group once applyMutation refreshes.
        applyMutation(result);
        const lastRun = result.last_run || {};
        const failedCount = Array.isArray(lastRun.failed) ? lastRun.failed.length : 0;
        const sheetWord = result.results.length === 1 ? "sheet" : "sheets";
        if (failedCount) {
          return {
            kind: "pinned",
            message: `Pack run: ${result.results.length} ${sheetWord}, ${failedCount} failed (see the card's last_run for details).`,
            links: [],
          };
        }
        return { kind: "success", message: `Pack run: ${result.results.length} ${sheetWord} generated.` };
      }
      const minted = result.elements || [];
      if (minted[0]) selectOnly(minted[0].id);
      applyMutation(result);
      const names = minted.map((element) => `"${element.name}"`).join(", ");
      if (result.failed && result.failed.length) {
        const reasons = result.failed.map((f) => `${f.engine}: ${f.error}`).join("; ");
        return {
          kind: "pinned",
          message: `Generated ${names} — ${result.failed.length} engine(s) failed (${reasons}).`,
          links: [],
        };
      }
      return { kind: "success", message: `Generated ${names}.` };
    },
    { control },
  );
}

// ---- pack mode (T0332 v2 phase C) ----------------------------------------------

// Preview pack (build-spec §2/Phase C): an EPHEMERAL, non-journaled call — packPreview never
// mutates recipe.pack, so there is nothing for applyMutation to fold in. The result (sheet
// count, style_ref_image flag, per-sheet {name,prompt,cells}) is stashed on `state.packPreview`
// (page-only view state, never persisted/journaled — mirrors the build-spec's own non-goal
// "Персист превью развёртки" — no freshness tracking needed either) so renderRecipe can show
// it inline; `refresh()` re-renders the inspector to pick it up.
export async function packPreviewAction(groupId, control) {
  await runLongOp(
    "Previewing pack… (offline expander, no codex)",
    async () => {
      const result = await api("POST", `/projects/${pid()}/recipe-cards/${groupId}/pack-preview`, {});
      // `at` is the inspector-sig token: the preview is ephemeral (no project change), so
      // without a changing marker in inspectorSig the panel never rebuilds and the result
      // stays invisible (lead hit exactly this, 2026-07-07).
      state.packPreview = { cardId: groupId, at: Date.now(), ...result };
      refresh();
      const refNote = result.style_ref_image ? " (style ref image included)" : "";
      return { kind: "success", message: `Preview: ${result.sheets} sheet(s)${refNote}.` };
    },
    { control },
  );
}

// Slice pack (build-spec §4): detect + hard-gate + slice every sheet of the card's last pack
// run, reparenting cuts into the run group. packSlice's own return has no `.project` —
// applyMutation's existing "no project -> reloadProject" fallback (app.js) resyncs the page,
// same as any other op that doesn't carry one.
export async function packSliceAction(groupId, control) {
  await runLongOp(
    "Slicing pack…",
    async () => {
      const result = await api("POST", `/projects/${pid()}/recipe-cards/${groupId}/pack-slice`, {});
      applyMutation(result);
      const contract = result.contract || [];
      const ok = contract.filter((c) => c.verdict === "OK").length;
      const rejected = contract.filter((c) => c.verdict === "REJECT").length;
      const missing = contract.filter((c) => c.verdict === "MISSING").length;
      const parts = [`${ok} OK`];
      if (rejected) parts.push(`${rejected} REJECT`);
      if (missing) parts.push(`${missing} MISSING`);
      const message = `Sliced pack: ${parts.join(", ")}.`;
      if (rejected || missing) return { kind: "pinned", message, links: [] };
      return { kind: "success", message };
    },
    { control },
  );
}

// ---- style cards (T0239 increment 3) -------------------------------------------

// Create a new style card: a group carrying an additive `style` blob (prompt/ref — see
// ops.createStyleCard). Mirrors createRecipeCardAction exactly, one route segment over:
// `worldPoint` (the canvas context menu's click point) places the card there; omitted,
// the op's own default (0,0) applies.
export async function createStyleCardAction(worldPoint) {
  if (!state.project) return;
  try {
    const body = {};
    if (worldPoint) {
      body.x = Math.round(worldPoint.x);
      body.y = Math.round(worldPoint.y);
    }
    const result = await api("POST", `/projects/${pid()}/style-cards`, body);
    state.selectedGroupId = result.group.id;
    applyMutation(result, `Created style card "${result.group.name}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Partial update of a card's `style` blob (prompt/ref) — the Style inspector section's live
// edits AND its "Make ref" buttons. One journaled patchStyle op; loud on a group without
// `style` (the op layer's guard — a plain group is not a card). Mirrors patchRecipeAction
// exactly, one route segment over.
export async function patchStyleAction(groupId, patch) {
  try {
    applyMutation(await api("PATCH", `/projects/${pid()}/style-cards/${groupId}`, patch));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- animation cards (T0265 increment 1, video route) --------------------------
//
// An animation card is a GROUP carrying an additive `anim` blob (design §1.1 —
// ai_studio.canvas.anim_card.v1), the SAME "group + additive blob" shape as a recipe/style
// card. These three actions mirror createRecipeCardAction / patchRecipeAction /
// generateFromRecipeAction one route segment over (design §2). The op layer + routes are
// built by the parallel server packet; the client is written to the design contract.

// Mint a new animation card. Two modes:
//   * from-scratch (empty-canvas context menu): `worldPoint` places the card there, else
//     the op's own (0,0) default — mirrors createRecipeCardAction exactly.
//   * PROMOTION ("Animate this image" on an image element): ONE POST with `memberId` (T0265
//     F6). The server's createAnimCard fits the box around the image (24px padding), moves it
//     in as the FIRST keyframe, and records the whole promotion as a SINGLE journal entry (one
//     Ctrl+Z undoes it). It refuses LOUDLY if the image is already a member of a
//     recipe/style/anim card or a claimed style-card ref — that refusal surfaces as-is via the
//     catch's toast. The new card is selected so the lead types motion + Generate.
export async function createAnimCardAction(worldPoint, image) {
  if (!state.project) return;
  try {
    if (image) {
      const result = await api("POST", `/projects/${pid()}/anim-cards`, { memberId: image.id });
      selectGroupOnly(result.group.id);
      applyMutation(result, `Animating "${image.name || image.id}" in new card "${result.group.name}".`);
      return;
    }
    const body = {};
    if (worldPoint) {
      body.x = Math.round(worldPoint.x);
      body.y = Math.round(worldPoint.y);
    }
    const result = await api("POST", `/projects/${pid()}/anim-cards`, body);
    state.selectedGroupId = result.group.id;
    applyMutation(result, `Created animation card "${result.group.name}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Partial update of a card's `anim` blob (motion/profile/seed/matte/gen_fps/loop/columns/
// trim/style_ref/accepted_ref — design §1.1). One journaled patchAnim op; loud on a group
// without `anim` (the op layer's guard). Mirrors patchRecipeAction exactly.
export async function patchAnimAction(groupId, patch) {
  try {
    applyMutation(await api("PATCH", `/projects/${pid()}/anim-cards/${groupId}`, patch));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Generate from an animation card (the Anim inspector's Generate button): runs the video
// route (generate -> frames -> matte, stops at matte) and imports the per-frame RGBA PNGs
// into a NEW flipbook element beside the card, in its PARENT scope (design §2
// generateAnimFromCard -> { project, element, group, run }). Generation takes MINUTES
// (GPU/ComfyUI) — same runLongOp limiter/spinner/disable + long-op queue (max 2) as
// generateFromRecipeAction. The minted flipbook element is selected on success.
export async function generateAnimFromCardAction(groupId, control) {
  await runLongOp(
    "Generating animation… (video route, minutes)",
    async () => {
      const result = await api("POST", `/projects/${pid()}/anim-cards/${groupId}/generate`, {});
      if (result.element) selectOnly(result.element.id);
      applyMutation(result);
      return { kind: "success", message: `Generated "${result.element ? result.element.name : "animation"}".` };
    },
    { control },
  );
}

// ---- Extract / promote (T0239 increment 4, final shape) ------------------------
//
// Extraction is ONE codex vision call that writes element.meta.extracted (no card minted
// here); minting a card from that ALREADY-STORED data is a SEPARATE, cheap, non-codex
// "promotion" gesture — so the lead can extract once and mint as many cards as he likes at
// zero extra codex cost. The two promotion actions are plain try/catch (fast, metadata-only
// mutations), not runLongOp — only extractElementAction spends real codex time.

// The Extracted section's "Extract"/"Re-extract" button: a codex VISION call (real
// seconds/minutes, same runLongOp treatment as generateFromRecipeAction). Re-running
// overwrites the prior extraction — applyMutation just re-renders the new meta.extracted
// blob, no special-case needed.
export async function extractElementAction(elementId, control) {
  await runLongOp(
    "Extracting… (codex vision)",
    async () => {
      const result = await api("POST", `/projects/${pid()}/elements/${elementId}/extract`, {});
      applyMutation(result);
      return { kind: "success", message: "Extracted." };
    },
    { control },
  );
}

// Mint a RECIPE card from an element's ALREADY-STORED meta.extracted — a plain, FAST
// mutation (no codex call; the op 400s naming "run Extract first" when meta.extracted is
// absent). Selects the new card so the lead sees it land immediately.
export async function promoteRecipeAction(elementId) {
  if (!state.project) return;
  try {
    const result = await api("POST", `/projects/${pid()}/elements/${elementId}/promote-recipe`, {});
    if (result.card) selectGroupOnly(result.card.id);
    applyMutation(result, `Created recipe card "${result.card ? result.card.name : "?"}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Mint a STYLE card from an element's ALREADY-STORED meta.extracted — mirrors
// promoteRecipeAction exactly, one route segment over.
export async function promoteStyleAction(elementId) {
  if (!state.project) return;
  try {
    const result = await api("POST", `/projects/${pid()}/elements/${elementId}/promote-style`, {});
    if (result.card) selectGroupOnly(result.card.id);
    applyMutation(result, `Created style card "${result.card ? result.card.name : "?"}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Set a shared field (visible/clip) on SEVERAL groups in ONE journaled patchGroups op —
// the multi-group inspector's shared toggles. One HTTP call, one undo restores every
// group. `patch` is {visible?} or {clip?}.
export async function setGroupsShared(groupIds, patch) {
  const ids = [...groupIds];
  if (!ids.length) return;
  try {
    applyMutation(await api("POST", `/projects/${pid()}/groups-set`, { groupIds: ids, ...patch }));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Resize a group's frame to fit its content (Figma "Resize to fit"): one journaled
// fitGroup op sets the frame to the union of the descendant closure + padding; children
// never move. An empty group is a loud op error that surfaces as an error toast (no
// client-side pre-check). Metadata op, so no spinner — the applyMutation flow updates
// the page from the response.
export async function fitGroupAction(groupId, padding) {
  try {
    const body = {};
    if (padding !== undefined && padding !== null) body.padding = padding;
    applyMutation(await api("POST", `/projects/${pid()}/groups/${groupId}/fit`, body), "Fit group to content.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Render a group to a composited screen PNG — long (python-backed) op. Resolves into a
// pinned-result toast with the download link; the triggering `control` is disabled while
// in flight (context-menu callers pass none — the menu already closed).
export async function renderScreen(groupId, { scale = 1, background } = {}, control) {
  await runLongOp(
    "Rendering group…",
    async () => {
      const body = { scale };
      if (background) body.background = background;
      const result = await api("POST", `/projects/${pid()}/groups/${groupId}/render`, body);
      return {
        kind: "pinned",
        message: `Rendered ${result.manifest.width}×${result.manifest.height} group`,
        links: exportLinks(result.folder, [result.file]),
      };
    },
    { control },
  );
}

// Ungroup = dissolve ONE level in ONE journaled ungroupGroup op: the group's direct child
// elements AND direct child subgroups move up to the group's OWN parent (nesting depth
// preserved), landing AT the group's former z-slot in their internal order, and the empty
// group is removed. One HTTP call, one undo restores the group exactly (the op owns the
// z-slot + reparent + delete atomically — no page-composed multi-call sequence).
export async function ungroup(groupId) {
  try {
    const result = await api("POST", `/projects/${pid()}/groups/${groupId}/ungroup`, {});
    state.selectedGroupId = null;
    state.selectedGroupIds = new Set();
    applyMutation(result, "Ungrouped.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Delete = the group AND its content go together (one journal entry, one undo);
// dissolving a group while keeping the elements is Ungroup's job.
export async function deleteGroupAction(groupId) {
  try {
    const result = await api("DELETE", `/projects/${pid()}/groups/${groupId}`);
    state.selectedGroupId = null;
    const count = (result.removedElements || []).length;
    applyMutation(result, count ? `Deleted group + ${count} element(s). Undo restores both.` : "Deleted empty group.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- history + project -------------------------------------------------------

export async function undo() {
  if (!state.project || !state.history.canUndo) return;
  // In region-edit isolation undo is clamped to the mode's own edits: never past
  // the journal seq captured at entry (lead's rule — Esc first for older ops).
  if (
    state.regionEditId &&
    state.regionEditBaseSeq != null &&
    state.history.seq != null &&
    state.history.seq <= state.regionEditBaseSeq
  ) {
    setStatus("Nothing to undo inside region editing — press Esc to exit first.");
    return;
  }
  try {
    // undo/redo responses carry the restored {project} + folded {history} flags too,
    // so the page reconciles from the response — no reload GET, no /history GET.
    // No confirmation toast: undo/redo are high-frequency and the change is already
    // visible (canvas updates + the enabled/disabled Undo/Redo buttons) — a toast per
    // Ctrl+Z would be pure noise. Failures still surface as an error toast.
    applyMutation(await api("POST", `/projects/${pid()}/undo`));
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function redo() {
  if (!state.project || !state.history.canRedo) return;
  try {
    applyMutation(await api("POST", `/projects/${pid()}/redo`));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Jump the project to any recorded history step (the history panel row click; Base = 0 =
// empty). ONE jumpHistory op — the same op the CLI's history-jump drives (tool parity).
// Behaves like N undos/redos and is itself undoable; quiet like undo/redo (the panel's
// live highlight is the feedback, no toast). applyMutation reconciles selection/region
// mode against the restored state, exactly like undo/redo.
export async function jumpToHistory(seq) {
  if (!state.project) return;
  try {
    applyMutation(await api("POST", `/projects/${pid()}/history-jump`, { seq }));
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function renameProject(title) {
  try {
    applyMutation(await api("PATCH", `/projects/${pid()}`, { title }), "Renamed project.");
  } catch (error) {
    setStatus(error.message, true);
  }
}
