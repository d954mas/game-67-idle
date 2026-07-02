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
  regionEditElement,
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

// Alpha-cutout the selected image element via the image-tools matte pipeline: swaps the
// element to a new content-addressed alpha PNG in ONE journaled op (undo restores the
// previous src). `method` is "auto" (route; refuses a dual-plate soft zone loudly) or
// "matte" (force key_matte). `regionIds`, when given, keys ONLY inside those stored
// regions (rest untouched). Long (python-backed) op — same limiter/spinner/disable
// treatment as slice; the triggering `control` is disabled while in flight.
export async function alphaCutoutFor(id, method, regionIds, control) {
  await runLongOp(
    "Alpha cutout…",
    async () => {
      const body = { elementId: id, method };
      if (Array.isArray(regionIds) && regionIds.length) body.regions = regionIds;
      const result = await api("POST", `/projects/${pid()}/alpha`, body);
      applyMutation(result);
      const scope = Array.isArray(regionIds) && regionIds.length ? `${regionIds.length} region(s)` : "element";
      return { kind: "success", message: `Alpha cutout applied to the ${scope} (${result.method}).` };
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
