// Canvas actions: the page's single place where UI intents become one HTTP API
// call to the shared ops layer. Panels and menus call these; none of them talk to
// the API directly. Nothing here holds business logic — each function marshals a
// request and reloads the project from disk (the source of truth).
import {
  api,
  clearSelection,
  elements,
  enterRegionEdit,
  regionEditElement,
  reloadProject,
  selectOnly,
  setStatus,
  setStatusLinks,
  state,
} from "./app.js";
import { newRegionId } from "./regions.js";
import { screenToImagePoint } from "../../viewer/asset_tools_viewport.mjs";

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

// Add a default centered region to an element and enter region-edit mode with it
// selected, so "+ Add region" is discoverable without knowing the draw gesture.
export async function addCenteredRegion(elementId) {
  const element = elements().find((item) => item.id === elementId);
  if (!element) return;
  const sw = element.source_w || element.w;
  const sh = element.source_h || element.h;
  const w = Math.max(4, Math.round(sw / 3));
  const h = Math.max(4, Math.round(sh / 3));
  const x = Math.round((sw - w) / 2);
  const y = Math.round((sh - h) / 2);
  const id = newRegionId();
  enterRegionEdit(elementId);
  state.selectedRegionIds = new Set([id]);
  try {
    await api("PUT", `/projects/${pid()}/elements/${elementId}/regions`, {
      regions: [...(element.regions || []), { id, rect: [x, y, w, h] }],
    });
    await reloadProject("Added region.");
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

export async function exportElementIds(ids) {
  if (!ids.length) return;
  try {
    setStatus("Exporting...");
    const result = await api("POST", `/projects/${pid()}/export`, { elementIds: ids });
    setStatusLinks(`Exported ${result.items.length} element(s):`, exportLinks(result.folder, result.items.map((item) => item.file)));
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- group (screen) ops ------------------------------------------------------

export async function createGroupFromSelection(name) {
  const ids = [...state.selectedIds];
  if (ids.length < 2) return;
  try {
    const result = await api("POST", `/projects/${pid()}/groups`, { name: name || "New screen", fromElements: ids });
    clearSelection();
    state.selectedGroupId = result.group.id;
    await reloadProject(`Grouped ${ids.length} element(s) into "${result.group.name}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

const DEFAULT_GROUP_SIZE = { w: 960, h: 540 };

// "+ Screen" (layers panel header): group the current selection into a screen
// exactly like Ctrl/Cmd+G, or — when nothing is selected — create an empty
// default-size screen centered on the current viewport via the same createGroup
// op the CLI's group-create (no --elements) already uses.
export async function createGroupOrDefault(name) {
  if (state.selectedIds.size > 0) {
    await createGroupFromSelection(name);
    return;
  }
  try {
    const center = screenToImagePoint({ x: state.cssWidth / 2, y: state.cssHeight / 2 }, state.viewport);
    const body = {
      name: name || "New screen",
      x: Math.round(center.x - DEFAULT_GROUP_SIZE.w / 2),
      y: Math.round(center.y - DEFAULT_GROUP_SIZE.h / 2),
      w: DEFAULT_GROUP_SIZE.w,
      h: DEFAULT_GROUP_SIZE.h,
    };
    const result = await api("POST", `/projects/${pid()}/groups`, body);
    state.selectedGroupId = result.group.id;
    await reloadProject(`Created screen "${result.group.name}".`);
  } catch (error) {
    setStatus(error.message, true);
  }
}

// Move elements into a screen (groupId) or out to top level (null). Used by the
// "Move to screen" context submenu and the layers-panel row drag.
export async function assignElementsToGroup(elementIds, groupId) {
  const ids = [...elementIds];
  if (!ids.length) return;
  try {
    await api("POST", `/projects/${pid()}/assign-group`, { elementIds: ids, groupId: groupId ?? null });
    await reloadProject(groupId ? "Moved to screen." : "Moved out of screen.");
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
    setStatus("Rendering screen...");
    const body = { scale };
    if (background) body.background = background;
    const result = await api("POST", `/projects/${pid()}/groups/${groupId}/render`, body);
    setStatusLinks(
      `Rendered ${result.manifest.width}x${result.manifest.height} screen:`,
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
    await reloadProject("Ungrouped screen.");
  } catch (error) {
    setStatus(error.message, true);
  }
}

export async function deleteGroupAction(groupId) {
  try {
    await api("DELETE", `/projects/${pid()}/groups/${groupId}`);
    state.selectedGroupId = null;
    await reloadProject("Deleted screen (elements kept).");
  } catch (error) {
    setStatus(error.message, true);
  }
}

// ---- history + project -------------------------------------------------------

export async function undo() {
  if (!state.project || !state.history.canUndo) return;
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
