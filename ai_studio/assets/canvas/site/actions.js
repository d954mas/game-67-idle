// Canvas actions: the page's single place where UI intents become one HTTP API
// call to the shared ops layer. Panels and menus call these; none of them talk to
// the API directly. Nothing here holds business logic — each function marshals a
// request and reloads the project from disk (the source of truth).
import {
  api,
  clearSelection,
  elements,
  regionEditElement,
  reloadProject,
  selectOnly,
  setStatus,
  setStatusLinks,
  state,
} from "./app.js";
import { screenToImagePoint } from "./viewport.mjs";
import { downloadFiles, pickDestination, supportsFsa, writeFilesToDir } from "./export_dest.mjs";

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
// are placed there, each offset by +20px so multiples don't stack exactly.
export async function addImageFiles(files, worldPoint) {
  const list = [...files].filter((file) => file && file.type && file.type.startsWith("image/"));
  if (!list.length || !state.project) return;
  let lastId = null;
  try {
    for (let i = 0; i < list.length; i += 1) {
      const file = list[i];
      setStatus(`Uploading ${file.name}...`);
      const bytes_base64 = await fileToBase64(file);
      const body = { name: file.name, bytes_base64 };
      if (worldPoint) {
        body.x = Math.round(worldPoint.x + i * 20);
        body.y = Math.round(worldPoint.y + i * 20);
      }
      const result = await api("POST", `/projects/${pid()}/images`, body);
      lastId = result.element.id;
    }
    if (lastId) selectOnly(lastId);
    await reloadProject(`Added ${list.length} image(s).`);
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
    await api("PATCH", `/projects/${pid()}/elements/${id}`, patch);
    await reloadProject();
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
    for (const id of ids) await api("DELETE", `/projects/${pid()}/elements/${id}`);
    clearSelection();
    await reloadProject(`Removed ${ids.length} element(s) (image files kept on disk).`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

export function deleteSelectedElements() {
  return deleteElements([...state.selectedIds]);
}

// ---- z-order (element ordering) ----------------------------------------------

// Sibling elements of `element` (same group scope) in paint/z-order. The op works
// on an absolute sibling index; these page helpers only translate a Figma-style
// intent (forward/backward/front/back) into that index and call the one reorder op.
function siblingsOf(element) {
  const scope = element.groupId || null;
  return elements().filter((item) => (item.groupId || null) === scope);
}

// Move one element to a target sibling index (0 = back / painted first). One
// journaled op; undo restores the exact previous order.
export async function reorderElementTo(id, index) {
  try {
    await api("POST", `/projects/${pid()}/elements/${id}/reorder`, { index });
    await reloadProject();
  } catch (error) {
    setStatus(error.message, true);
  }
}

function nudgeZ(id, delta) {
  const element = elements().find((item) => item.id === id);
  if (!element) return undefined;
  const siblings = siblingsOf(element);
  const from = siblings.findIndex((item) => item.id === id);
  const target = from + delta;
  if (target < 0 || target >= siblings.length) return undefined; // already at the edge
  return reorderElementTo(id, target);
}

function edgeZ(id, edge) {
  const element = elements().find((item) => item.id === id);
  if (!element) return undefined;
  const siblings = siblingsOf(element);
  const from = siblings.findIndex((item) => item.id === id);
  const target = edge === "front" ? siblings.length - 1 : 0;
  if (from === target) return undefined;
  return reorderElementTo(id, target);
}

export function bringElementForward(id) {
  return nudgeZ(id, +1);
}
export function sendElementBackward(id) {
  return nudgeZ(id, -1);
}
export function bringElementToFront(id) {
  return edgeZ(id, "front");
}
export function sendElementToBack(id) {
  return edgeZ(id, "back");
}

export async function detectRegionsFor(id) {
  try {
    setStatus("Detecting regions...");
    const result = await api("POST", `/projects/${pid()}/detect-regions`, { elementId: id });
    await reloadProject(`Detected ${result.regions.length} region(s).`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Slice all of an element's regions, or only `regionIds` when given (per-region
// slice from the region menu / inspector "Slice selected region(s)"). The op crops
// the STORED region rects verbatim, so edited and hand-drawn regions crop exactly.
export async function sliceRegionsFor(id, regionIds) {
  try {
    setStatus("Slicing regions...");
    const body = { elementId: id };
    if (Array.isArray(regionIds) && regionIds.length) body.regionIds = regionIds;
    const result = await api("POST", `/projects/${pid()}/slice`, body);
    state.selectedGroupId = null;
    state.selectedRegionIds = new Set();
    state.selectedIds = new Set(result.created.map((element) => element.id));
    await reloadProject(`Sliced ${result.created.length} region(s) into new elements.`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- regions -----------------------------------------------------------------

// Replace an element's regions in one journaled setRegions op. Region edits
// (move/resize/rubber-band) accumulate locally during the gesture and commit here
// exactly once, so undo steps back a whole gesture rather than every mouse move.
export async function setRegionsFor(elementId, regions, message) {
  try {
    await api("PUT", `/projects/${pid()}/elements/${elementId}/regions`, { regions });
    await reloadProject(message);
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
    await api("PUT", `/projects/${pid()}/elements/${elementId}/regions`, { regions });
    await reloadProject();
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
    await api("PUT", `/projects/${pid()}/elements/${element.id}/regions`, { regions: remaining });
    state.selectedRegionIds = new Set();
    await reloadProject(`Removed ${ids.size} region(s).`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function deleteRegion(elementId, regionId) {
  const element = elements().find((item) => item.id === elementId);
  if (!element) return;
  const remaining = (element.regions || []).filter((region) => region.id !== regionId);
  try {
    await api("PUT", `/projects/${pid()}/elements/${elementId}/regions`, { regions: remaining });
    state.selectedRegionIds.delete(regionId);
    await reloadProject("Removed region.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- export ------------------------------------------------------------------

// Replace an element's Figma-style export rows via the journaled setExportSettings
// op (the inspector Export section commits one entry per edit; undoable).
export async function setExportRows(elementId, rows) {
  try {
    await api("PUT", `/projects/${pid()}/elements/${elementId}/export`, { rows });
    await reloadProject();
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Deliver a finished export to disk. Figma-style destination: always open the
// directory picker (starting at the last-used folder); cancel aborts the export
// with a visible message; only a browser without the File System Access API falls
// back to per-file downloads. Never silently drops files.
async function deliverExport(result, label) {
  const files = (result.items || result.screens || []).map((entry) => entry.file);
  const stamp = baseName(result.folder);
  if (!files.length) {
    setStatus("Nothing to export.");
    return;
  }
  const links = exportLinks(result.folder, files);
  if (!supportsFsa()) {
    downloadFiles(pid(), stamp, files);
    setStatusLinks(`${label} — ${files.length} file(s) downloaded:`, links);
    return;
  }
  let dir;
  try {
    dir = await pickDestination(pid());
  } catch (error) {
    if (error && error.name === "AbortError") {
      setStatus("Export canceled — no folder chosen.");
      return;
    }
    setStatus(`Could not open a destination folder: ${error.message}`, true);
    return;
  }
  try {
    await writeFilesToDir(dir, pid(), stamp, files);
    setStatusLinks(`${label} — ${files.length} file(s) saved to “${dir.name}”:`, links);
  } catch (error) {
    setStatus(`Export failed writing to “${dir.name}”: ${error.message}`, true);
  }
}

// Export the given elements, each honoring its own persisted export rows (or the
// implicit 1x-png default), then deliver to the chosen destination.
export async function exportElementIds(ids) {
  if (!ids.length) return;
  try {
    setStatus("Exporting...");
    const result = await api("POST", `/projects/${pid()}/export`, { elementIds: ids });
    await deliverExport(result, `Exported ${ids.length} element(s)`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// No selection -> project export: render every visible screen at 1x png.
export async function exportProjectAction() {
  try {
    setStatus("Exporting project...");
    const result = await api("POST", `/projects/${pid()}/export`, { project: true });
    await deliverExport(result, `Exported ${result.screens.length} screen(s)`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- group ops ------------------------------------------------------

export async function createGroupFromSelection(name) {
  const ids = [...state.selectedIds];
  if (ids.length < 2) return;
  try {
    const result = await api("POST", `/projects/${pid()}/groups`, { name: name || "New group", fromElements: ids });
    clearSelection();
    state.selectedGroupId = result.group.id;
    await reloadProject(`Grouped ${ids.length} element(s) into "${result.group.name}".`);
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
    await reloadProject(`Created group "${result.group.name}".`);
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
    await api("POST", `/projects/${pid()}/assign-group`, { elementIds: ids, groupId: groupId ?? null });
    await reloadProject(groupId ? "Moved to group." : "Moved out of group.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function patchGroupBox(groupId, patch) {
  try {
    await api("PATCH", `/projects/${pid()}/groups/${groupId}`, patch);
    await reloadProject();
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

export async function renderScreen(groupId, { scale = 1, background } = {}) {
  try {
    setStatus("Rendering group...");
    const body = { scale };
    if (background) body.background = background;
    const result = await api("POST", `/projects/${pid()}/groups/${groupId}/render`, body);
    setStatusLinks(
      `Rendered ${result.manifest.width}x${result.manifest.height} group:`,
      exportLinks(result.folder, [result.file]),
    );
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Ungroup: clear the group from every member, then delete the (now empty) group.
export async function ungroup(groupId) {
  try {
    const memberIds = elements().filter((element) => element.groupId === groupId).map((element) => element.id);
    if (memberIds.length) await api("POST", `/projects/${pid()}/assign-group`, { elementIds: memberIds, groupId: null });
    await api("DELETE", `/projects/${pid()}/groups/${groupId}`);
    state.selectedGroupId = null;
    await reloadProject("Ungrouped.");
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
    await reloadProject(count ? `Deleted group + ${count} element(s). Undo restores both.` : "Deleted empty group.");
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
    await api("POST", `/projects/${pid()}/undo`);
    await reloadProject("Undid last change.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function redo() {
  if (!state.project || !state.history.canRedo) return;
  try {
    await api("POST", `/projects/${pid()}/redo`);
    await reloadProject("Redid change.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function renameProject(title) {
  try {
    await api("PATCH", `/projects/${pid()}`, { title });
    await reloadProject("Renamed project.");
  } catch (error) {
    setStatus(error.message, true);
  }
}
