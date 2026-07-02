// Canvas operation layer — the ONE surface both clients call.
//
// Product rule (tool parity): every canvas capability is exactly one operation
// here. The HTTP API adapter (api.mjs) and the agent CLI (cli.mjs) are thin
// clients that only marshal input/output; they hold no logic of their own. Tests
// exercise these functions directly, so the browser page and an agent always go
// through identical code.
//
// Most mutating ops are thin store wrappers that also append one journal entry so
// they are undoable. detectRegions and sliceRegions are the bridged pipeline ops:
// they reuse the existing raster2d tool functions unmodified.
//
// Journal / undo-redo design (single linear history over an append-only log):
//   - Each mutating op appends a line {seq, at, op, args_summary, undo_patch,
//     state, parent}. undo_patch is the {elements, tool_runs} snapshot BEFORE the
//     op (restore target for undo); state is the snapshot AFTER (re-apply target
//     for redo); parent is the history head that was current when the op ran, so
//     the journal forms a linked chain. Files are immutable, so a metadata-only
//     snapshot always fully restores project.json.
//   - project.json carries one pointer, history_seq (the applied head; 0 = base).
//   - undo restores the head entry's undo_patch, moves the head to entry.parent,
//     and appends a {op:"undo", target_seq} marker.
//   - redo picks the greatest-seq mutation whose parent == the current head,
//     restores its state, advances the head, and appends a {op:"redo"} marker.
//   - A new mutation after an undo appends with parent == the current head, so it
//     becomes the newest child of that head; redo (greatest-seq child) then never
//     picks the stale branch — the redo tail is invalidated automatically.
import { execFile } from "node:child_process";
import { randomUUID } from "node:crypto";
import { existsSync, readFileSync } from "node:fs";
import { basename, extname, join } from "node:path";
import {
  detectRaster2dRegions,
  exportRaster2dRegions,
  uploadRaster2dSource,
} from "../tools/raster2d/api.mjs";
import {
  addImage as storeAddImage,
  appendJournal,
  createProject,
  deleteProject as storeDeleteProject,
  getProject,
  imageSize,
  listProjects,
  patchElement as storePatchElement,
  readElementBytes,
  readJournal,
  removeElement as storeRemoveElement,
  resolveProjectFile,
  resolveProjectPath,
  updateProject,
  writeProjectBytes,
} from "./store.mjs";

export {
  createProject,
  getProject,
  listProjects,
  resolveProjectFile,
  resolveProjectPath,
  updateProject,
};

function mimeForExt(fileName) {
  const ext = String(fileName || "").toLowerCase().split(".").pop();
  return (
    { png: "image/png", jpg: "image/jpeg", jpeg: "image/jpeg", gif: "image/gif", webp: "image/webp" }[ext] ||
    "image/png"
  );
}

function slug(value) {
  const cleaned = String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .slice(0, 60);
  return cleaned || "element";
}

// ---- journal core ------------------------------------------------------------

function snapshotOf(project) {
  return {
    // The project title is metadata like elements/groups, so carrying it in the
    // before/after snapshot makes a rename (patchProject) fully undoable too.
    title: project.title,
    elements: JSON.parse(JSON.stringify(project.elements || [])),
    // Groups are metadata like elements, so the same before/after snapshot makes
    // every group + visibility mutation fully undoable with no file changes.
    groups: JSON.parse(JSON.stringify(project.groups || [])),
    tool_runs: JSON.parse(JSON.stringify(project.tool_runs || [])),
  };
}

// Append one mutation entry and advance the project's history head. Returns the
// saved project (with the new history_seq). If the op changed nothing, no entry
// is written and the current project state is returned unchanged.
function commitMutation(root, projectId, { op, args_summary, before, after }) {
  const undoPatch = snapshotOf(before);
  const state = snapshotOf(after);
  if (JSON.stringify(undoPatch) === JSON.stringify(state)) return after;
  const entry = appendJournal(root, projectId, {
    op,
    args_summary: args_summary || {},
    undo_patch: undoPatch,
    state,
    parent: Number(before.history_seq) || 0,
  });
  return updateProject(root, projectId, { history_seq: entry.seq });
}

// ---- journaled store wrappers ------------------------------------------------

export function addImage(root, projectId, args = {}) {
  const before = getProject(root, projectId);
  const result = storeAddImage(root, projectId, args);
  const project = commitMutation(root, projectId, {
    op: "addImage",
    args_summary: { name: result.element.name, elementId: result.element.id, w: result.element.w, h: result.element.h },
    before,
    after: result.project,
  });
  return { project, element: result.element };
}

export function patchElement(root, projectId, elementId, patch = {}) {
  const before = getProject(root, projectId);
  const result = storePatchElement(root, projectId, elementId, patch);
  const project = commitMutation(root, projectId, {
    op: "patchElement",
    args_summary: { elementId, patch },
    before,
    after: result.project,
  });
  return { project, element: result.element };
}

export function removeElement(root, projectId, elementId) {
  const before = getProject(root, projectId);
  const result = storeRemoveElement(root, projectId, elementId);
  const project = commitMutation(root, projectId, {
    op: "removeElement",
    args_summary: { elementId },
    before,
    after: result.project,
  });
  return { project, removed: result.removed };
}

// ---- project-level ops -------------------------------------------------------

// Rename a project. Journaled like any metadata mutation: the title lives in the
// snapshot, so undo/redo restore it together with elements/groups/tool_runs.
export function patchProject(root, { projectId, title } = {}) {
  if (!projectId) throw new Error("patchProject requires projectId");
  if (title === undefined) throw new Error("patchProject requires a title");
  const before = getProject(root, projectId);
  const cleanTitle = String(title).trim() || before.title;
  const after = updateProject(root, projectId, { title: cleanTitle });
  const project = commitMutation(root, projectId, {
    op: "patchProject",
    args_summary: { title: cleanTitle },
    before,
    after,
  });
  return { project };
}

// Move a project to the projects-root .trash (safety: recoverable, never rm'd).
// A project-level action, not journaled — the whole folder (journal included)
// moves as one.
export function deleteProject(root, { projectId } = {}) {
  if (!projectId) throw new Error("deleteProject requires projectId");
  return storeDeleteProject(root, projectId);
}

// ---- groups (screens) --------------------------------------------------------
//
// A group is a Figma-frame-like named screen region that owns member elements
// (element.groupId). All group + visibility mutations are journaled exactly like
// element ops via commitMutation, so the metadata-only snapshot restores groups,
// elements, and tool_runs together on undo.

function finite(value) {
  return value !== undefined && value !== null && Number.isFinite(Number(value));
}

function groupsOf(project) {
  return Array.isArray(project.groups) ? project.groups : [];
}

function findGroup(project, groupId) {
  const group = groupsOf(project).find((item) => item.id === groupId);
  if (!group) throw new Error(`group not found: ${groupId}`);
  return group;
}

function elementsBBox(elements) {
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const element of elements) {
    minX = Math.min(minX, Number(element.x) || 0);
    minY = Math.min(minY, Number(element.y) || 0);
    maxX = Math.max(maxX, (Number(element.x) || 0) + (Number(element.w) || 0));
    maxY = Math.max(maxY, (Number(element.y) || 0) + (Number(element.h) || 0));
  }
  return { minX, minY, maxX, maxY };
}

// Create a screen group. Either explicit bounds (x/y/w/h) OR fromElements: an
// array of element ids whose bounding box (+24px padding) becomes the frame and
// which are assigned this group. One journal entry.
export function createGroup(root, { projectId, name, x, y, w, h, fromElements } = {}) {
  if (!projectId) throw new Error("createGroup requires projectId");
  const before = getProject(root, projectId);
  const groupId = `grp_${randomUUID().slice(0, 8)}`;
  const cleanName = String(name || "").trim() || "Screen";

  let bounds;
  let memberIds = [];
  if (Array.isArray(fromElements) && fromElements.length) {
    memberIds = fromElements.map(String);
    const members = memberIds.map((id) => {
      const element = (before.elements || []).find((item) => item.id === id);
      if (!element) throw new Error(`element not found: ${id}`);
      return element;
    });
    const pad = 24;
    const { minX, minY, maxX, maxY } = elementsBBox(members);
    bounds = { x: minX - pad, y: minY - pad, w: maxX - minX + pad * 2, h: maxY - minY + pad * 2 };
  } else {
    if (!finite(w) || !finite(h) || Number(w) <= 0 || Number(h) <= 0) {
      throw new Error("createGroup requires fromElements or positive w/h bounds");
    }
    bounds = { x: finite(x) ? Number(x) : 0, y: finite(y) ? Number(y) : 0, w: Number(w), h: Number(h) };
  }

  const group = { id: groupId, name: cleanName, x: bounds.x, y: bounds.y, w: bounds.w, h: bounds.h, visible: true };
  const memberSet = new Set(memberIds);
  const nextElements = (before.elements || []).map((element) =>
    memberSet.has(element.id) ? { ...element, groupId } : element,
  );
  const after = updateProject(root, projectId, {
    groups: [...groupsOf(before), group],
    elements: nextElements,
  });
  const project = commitMutation(root, projectId, {
    op: "createGroup",
    args_summary: { groupId, name: cleanName, members: memberIds, bounds },
    before,
    after,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Patch a group's name/bounds/visibility. When x or y change, translate ALL
// member elements by the same delta so the whole screen moves as one; resize
// (w/h) never moves members. One journal entry restores everything on undo.
export function patchGroup(root, { projectId, groupId, name, x, y, w, h, visible } = {}) {
  if (!projectId) throw new Error("patchGroup requires projectId");
  if (!groupId) throw new Error("patchGroup requires groupId");
  const before = getProject(root, projectId);
  const current = findGroup(before, groupId);

  const dx = finite(x) ? Number(x) - Number(current.x || 0) : 0;
  const dy = finite(y) ? Number(y) - Number(current.y || 0) : 0;

  const nextGroups = groupsOf(before).map((group) => {
    if (group.id !== groupId) return group;
    const patched = { ...group };
    if (name !== undefined) patched.name = String(name);
    if (finite(x)) patched.x = Number(x);
    if (finite(y)) patched.y = Number(y);
    if (finite(w)) patched.w = Number(w);
    if (finite(h)) patched.h = Number(h);
    if (visible !== undefined) patched.visible = !(visible === false || visible === "false");
    return patched;
  });

  const nextElements = (dx !== 0 || dy !== 0)
    ? (before.elements || []).map((element) =>
        element.groupId === groupId
          ? { ...element, x: (Number(element.x) || 0) + dx, y: (Number(element.y) || 0) + dy }
          : element,
      )
    : (before.elements || []);

  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "patchGroup",
    args_summary: { groupId, name, x, y, w, h, visible, dx, dy },
    before,
    after,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Assign elements to a group (groupId) or clear their group (groupId=null). One
// journal entry.
export function assignToGroup(root, { projectId, elementIds, groupId } = {}) {
  if (!projectId) throw new Error("assignToGroup requires projectId");
  const before = getProject(root, projectId);
  const ids = Array.isArray(elementIds) ? elementIds.map(String) : [];
  if (!ids.length) throw new Error("assignToGroup requires elementIds");
  const target = groupId == null || groupId === "" ? null : String(groupId);
  if (target) findGroup(before, target);
  const idSet = new Set(ids);
  for (const id of ids) {
    if (!(before.elements || []).some((item) => item.id === id)) throw new Error(`element not found: ${id}`);
  }
  const nextElements = (before.elements || []).map((element) =>
    idSet.has(element.id) ? { ...element, groupId: target } : element,
  );
  const after = updateProject(root, projectId, { elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "assignToGroup",
    args_summary: { elementIds: ids, groupId: target },
    before,
    after,
  });
  return { project, count: ids.length, groupId: target };
}

// Remove a group. Members keep their positions and have groupId cleared. One
// journal entry.
export function deleteGroup(root, { projectId, groupId } = {}) {
  if (!projectId) throw new Error("deleteGroup requires projectId");
  if (!groupId) throw new Error("deleteGroup requires groupId");
  const before = getProject(root, projectId);
  findGroup(before, groupId);
  const nextGroups = groupsOf(before).filter((group) => group.id !== groupId);
  const nextElements = (before.elements || []).map((element) =>
    element.groupId === groupId ? { ...element, groupId: null } : element,
  );
  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "deleteGroup",
    args_summary: { groupId },
    before,
    after,
  });
  return { project, removed: groupId };
}

// ---- undo / redo / history ---------------------------------------------------

export function undoOp(root, { projectId } = {}) {
  if (!projectId) throw new Error("undoOp requires projectId");
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
  if (!head) throw new Error("nothing to undo");
  const entry = readJournal(root, projectId).find((item) => Number(item.seq) === head && item.undo_patch);
  if (!entry) throw new Error(`no undoable journal entry for seq ${head}`);
  const restore = {
    elements: entry.undo_patch.elements || [],
    groups: entry.undo_patch.groups || [],
    tool_runs: entry.undo_patch.tool_runs || [],
    history_seq: Number(entry.parent) || 0,
  };
  // Older journals predate title-in-snapshot; only restore title when present so
  // updateProject never clobbers the live title with undefined.
  if (entry.undo_patch.title !== undefined) restore.title = entry.undo_patch.title;
  const saved = updateProject(root, projectId, restore);
  appendJournal(root, projectId, { op: "undo", target_seq: head });
  return { project: saved, undone_seq: head, history_seq: saved.history_seq };
}

export function redoOp(root, { projectId } = {}) {
  if (!projectId) throw new Error("redoOp requires projectId");
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
  const candidates = readJournal(root, projectId).filter((item) => item.state && (Number(item.parent) || 0) === head);
  if (!candidates.length) throw new Error("nothing to redo");
  const entry = candidates.reduce((best, item) => (Number(item.seq) > Number(best.seq) ? item : best));
  const restore = {
    elements: entry.state.elements || [],
    groups: entry.state.groups || [],
    tool_runs: entry.state.tool_runs || [],
    history_seq: Number(entry.seq),
  };
  if (entry.state.title !== undefined) restore.title = entry.state.title;
  const saved = updateProject(root, projectId, restore);
  appendJournal(root, projectId, { op: "redo", target_seq: entry.seq });
  return { project: saved, redone_seq: entry.seq, history_seq: saved.history_seq };
}

// Compact journal view for `history <id>` / GET .../history: seq, timestamp, op,
// and the small args_summary (or marker target) — never the big snapshots.
export function readHistory(root, { projectId } = {}) {
  if (!projectId) throw new Error("readHistory requires projectId");
  const project = getProject(root, projectId);
  const journal = readJournal(root, projectId);
  const head = Number(project.history_seq) || 0;
  const canUndo = head > 0 && journal.some((item) => Number(item.seq) === head && item.undo_patch);
  const canRedo = journal.some((item) => item.state && (Number(item.parent) || 0) === head);
  const entries = journal.map((item) => ({
    seq: item.seq,
    at: item.at,
    op: item.op,
    ...(item.args_summary ? { args_summary: item.args_summary } : {}),
    ...(item.target_seq !== undefined ? { target_seq: item.target_seq } : {}),
  }));
  return { history_seq: head, canUndo, canRedo, entries };
}

// ---- detectRegions (bridged) -------------------------------------------------

// Read the element's stored image, run it through the existing raster2d upload +
// detect pipeline (imported unmodified from ../tools/raster2d/api.mjs), then
// persist the detected regions on the element and record a tool_runs entry. One
// journal entry makes the detection undoable.
export async function detectRegions(root, { projectId, elementId, params = {} } = {}) {
  if (!projectId) throw new Error("detectRegions requires projectId");
  if (!elementId) throw new Error("detectRegions requires elementId");
  const { buffer, fileName } = readElementBytes(root, projectId, elementId);
  const dims = imageSize(buffer);

  const dataUrl = `data:${mimeForExt(fileName)};base64,${buffer.toString("base64")}`;
  const uploaded = await uploadRaster2dSource(root, { fileName, dataUrl });
  const detected = await detectRaster2dRegions(root, {
    sourcePath: uploaded.sourcePath,
    options: params || {},
  });
  const regions = Array.isArray(detected.regions && detected.regions.regions)
    ? detected.regions.regions
    : [];

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "detect_regions",
    elementId,
    at: new Date().toISOString(),
    params: params || {},
    result_summary: {
      region_count: regions.length,
      session_id: detected.sessionId,
      background_mode: (detected.regions && detected.regions.mode) || "",
    },
  };

  // Re-read to avoid clobbering concurrent edits, snapshot before, then persist
  // regions (and backfill source dimensions) + the tool_runs entry atomically.
  const before = getProject(root, projectId);
  if (!(before.elements || []).some((item) => item.id === elementId)) {
    throw new Error(`element not found: ${elementId}`);
  }
  const nextElements = (before.elements || []).map((item) =>
    item.id === elementId
      ? { ...item, source_w: item.source_w || dims.width, source_h: item.source_h || dims.height, regions }
      : item,
  );
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: [...(before.tool_runs || []), run],
  });
  const project = commitMutation(root, projectId, {
    op: "detectRegions",
    args_summary: { elementId, region_count: regions.length },
    before,
    after,
  });
  const element = (project.elements || []).find((item) => item.id === elementId);
  return { project, element, run, regions };
}

// ---- sliceRegions (bridged) --------------------------------------------------

// Slice an element's detected regions into new immutable image elements. Bridges
// to the raster2d pipeline exactly as the Asset Tools surface does: stage the
// element bytes as a session source, normalize + detect to get a clean keyed
// image, then export the SELECTED region rects in one slice call. Cropping PNGs
// in pure Node has no dependency-free path, so we reuse the Python slicer; no
// raster2d code is modified. Each crop becomes a content-addressed file + a new
// image element placed in a grid to the right of the parent, with provenance in
// meta.parent. The whole slice is one journal entry (undo removes every crop).
export async function sliceRegions(root, { projectId, elementId, regionIds } = {}) {
  if (!projectId) throw new Error("sliceRegions requires projectId");
  if (!elementId) throw new Error("sliceRegions requires elementId");
  const before = getProject(root, projectId);
  const parent = (before.elements || []).find((item) => item.id === elementId);
  if (!parent) throw new Error(`element not found: ${elementId}`);
  if (parent.type !== "image" || !parent.src) throw new Error(`element ${elementId} is not an image`);
  const allRegions = Array.isArray(parent.regions) ? parent.regions : [];
  if (!allRegions.length) {
    throw new Error(`element ${elementId} has no regions; run detectRegions first`);
  }

  let selected = allRegions;
  if (Array.isArray(regionIds) && regionIds.length) {
    const wanted = new Set(regionIds.map(String));
    selected = allRegions.filter((region) => wanted.has(String(region.id)));
    const found = new Set(selected.map((region) => String(region.id)));
    const missing = [...wanted].filter((id) => !found.has(id));
    if (missing.length) throw new Error(`unknown region id(s): ${missing.join(", ")}`);
  }
  if (!selected.length) throw new Error("no regions selected to slice");

  const { buffer, fileName } = readElementBytes(root, projectId, elementId);
  const dataUrl = `data:${mimeForExt(fileName)};base64,${buffer.toString("base64")}`;
  const uploaded = await uploadRaster2dSource(root, { fileName, dataUrl });
  const detected = await detectRaster2dRegions(root, { sourcePath: uploaded.sourcePath, options: {} });
  const prefix = slug(parent.name || "sheet");
  const exported = await exportRaster2dRegions(root, {
    imagePath: detected.normalizedPath,
    regions: selected,
    prefix,
    includeReviewSheet: false,
  });
  const slices = (exported.manifest && exported.manifest.slices) || [];
  if (!slices.length) throw new Error("raster2d produced no slices");

  // Place crops in a neat grid to the right of the parent (gap in source pixels).
  const gap = 16;
  const startX = parent.x + parent.w + gap;
  const columns = Math.max(1, Math.ceil(Math.sqrt(slices.length)));
  const created = [];
  let col = 0;
  let cursorX = startX;
  let rowY = parent.y;
  let rowMaxH = 0;
  for (const sliceItem of slices) {
    const bytes = readFileSync(join(root, sliceItem.path));
    const added = storeAddImage(root, projectId, {
      name: `${parent.name}#${sliceItem.id}`,
      bytes,
      x: cursorX,
      y: rowY,
      meta: { parent: { elementId, regionId: sliceItem.id, sheetSrc: parent.src } },
    });
    created.push(added.element);
    cursorX += added.element.w + gap;
    rowMaxH = Math.max(rowMaxH, added.element.h);
    col += 1;
    if (col >= columns) {
      col = 0;
      cursorX = startX;
      rowY += rowMaxH + gap;
      rowMaxH = 0;
    }
  }

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "slice_regions",
    elementId,
    at: new Date().toISOString(),
    params: { regionIds: selected.map((region) => String(region.id)) },
    result_summary: { slice_count: created.length, session_id: detected.sessionId },
  };
  const withRun = updateProject(root, projectId, {
    tool_runs: [...(getProject(root, projectId).tool_runs || []), run],
  });
  const project = commitMutation(root, projectId, {
    op: "slice",
    args_summary: {
      elementId,
      regionIds: selected.map((region) => String(region.id)),
      created: created.map((element) => element.id),
      count: created.length,
    },
    before,
    after: withRun,
  });
  return { project, created, run, regions: selected };
}

// ---- exportElements ----------------------------------------------------------

// Copy each selected element's current image file into
// <project>/export/<utc-stamp>/ under its (sanitized, collision-suffixed) name
// plus a manifest.json. Export creates no project mutation, so it is NOT
// journaled/undoable; it only records a tool_runs entry for provenance.
export function exportElements(root, { projectId, elementIds, format } = {}) {
  if (!projectId) throw new Error("exportElements requires projectId");
  const project = getProject(root, projectId);
  const ids = Array.isArray(elementIds) ? elementIds.map(String) : [];
  if (!ids.length) throw new Error("exportElements requires elementIds");

  const elements = [];
  for (const id of ids) {
    const element = (project.elements || []).find((item) => String(item.id) === id);
    if (!element) throw new Error(`element not found: ${id}`);
    if (element.type !== "image" || !element.src) throw new Error(`element ${id} is not an exportable image`);
    elements.push(element);
  }

  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const folder = resolveProjectPath(root, projectId, "export", stamp);
  const used = new Set();
  const items = [];
  for (const element of elements) {
    const srcAbs = resolveProjectFile(root, projectId, element.src);
    const bytes = readFileSync(srcAbs);
    const ext = extname(basename(srcAbs)) || ".png";
    const base = slug(element.name || element.id);
    let file = `${base}${ext}`;
    let counter = 2;
    while (used.has(file)) {
      file = `${base}_${String(counter).padStart(2, "0")}${ext}`;
      counter += 1;
    }
    used.add(file);
    writeProjectBytes(resolveProjectPath(root, projectId, "export", stamp, file), bytes);
    items.push({ elementId: element.id, name: element.name || element.id, file, src: element.src, meta: element.meta || {} });
  }

  const manifest = {
    schema: "ai_studio.canvas.export.v1",
    project: project.id,
    at: new Date().toISOString(),
    items,
  };
  writeProjectBytes(
    resolveProjectPath(root, projectId, "export", stamp, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
  );

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "export_elements",
    at: new Date().toISOString(),
    params: { elementIds: ids, format: format || "copy" },
    result_summary: { item_count: items.length, folder },
  };
  updateProject(root, projectId, { tool_runs: [...(project.tool_runs || []), run] });
  return { folder, items, manifest, run };
}

// ---- renderGroup (screen compositing) ----------------------------------------
//
// Composite a group's VISIBLE member elements (element.visible !== false), in
// element array order (z-order), clipped to the group bounds, into ONE PNG at
// the requested scale over a transparent (or solid) background. The pixel work
// is done by our own Python tool (tools/render_group.py, PIL) because there is
// no dependency-free pure-Node compositor. This tool is OURS, so ops spawns it
// directly with the same robust Python discovery the raster2d bridge uses; the
// full render spec is handed over as one JSON file. renderGroup makes no
// undoable geometry change, so like exportElements it is NOT journaled — it only
// records a render_group tool_runs entry.

function pythonCandidates() {
  const candidates = [];
  const add = (command, args = []) => {
    if (command && !candidates.some((candidate) => candidate.command === command)) {
      candidates.push({ command, args });
    }
  };
  for (const command of [process.env.AI_STUDIO_PYTHON, process.env.PYTHON]) add(command);
  const bundled = process.env.USERPROFILE
    ? join(process.env.USERPROFILE, ".cache", "codex-runtimes", "codex-primary-runtime", "dependencies", "python", "python.exe")
    : "";
  if (bundled && existsSync(bundled)) add(bundled);
  add("py", ["-3.12"]);
  for (const command of ["C:\\Python312\\python.exe", "C:\\Python314\\python.exe"]) {
    if (existsSync(command)) add(command);
  }
  add("python");
  return candidates;
}

// Try each Python candidate in turn (cwd = repo root, so the script resolves by
// its repo-relative path); reject with the last real error if all fail.
function runPython(root, args) {
  return new Promise((resolveRun, rejectRun) => {
    const candidates = pythonCandidates();
    const failures = [];
    const tryCandidate = (index) => {
      const candidate = candidates[index];
      execFile(candidate.command, [...candidate.args, ...args], { cwd: root, windowsHide: true }, (error, stdout, stderr) => {
        if (!error) {
          resolveRun(stdout);
          return;
        }
        failures.push((stderr || stdout || error.message).trim());
        if (index + 1 < candidates.length) {
          tryCandidate(index + 1);
          return;
        }
        rejectRun(new Error(failures.filter(Boolean).at(-1) || error.message));
      });
    };
    tryCandidate(0);
  });
}

function hexColor(value) {
  const text = String(value || "").trim();
  return /^#[0-9a-fA-F]{6}$/.test(text) ? text.toLowerCase() : null;
}

export async function renderGroup(root, { projectId, groupId, scale, background } = {}) {
  if (!projectId) throw new Error("renderGroup requires projectId");
  if (!groupId) throw new Error("renderGroup requires groupId");
  const project = getProject(root, projectId);
  const group = groupsOf(project).find((item) => item.id === groupId);
  if (!group) throw new Error(`group not found: ${groupId}`);

  const renderScale = finite(scale) && Number(scale) > 0 ? Number(scale) : 1;
  const bg = background === undefined || background === null || background === "" ? null : hexColor(background);
  if (background && bg === null) throw new Error(`background must be #rrggbb, got ${JSON.stringify(background)}`);

  // Visible member elements, in element array order (z-order).
  const members = (project.elements || []).filter(
    (element) => element.groupId === groupId && element.visible !== false && element.type === "image" && element.src,
  );

  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const fileName = `screen_${slug(group.name || group.id)}.png`;
  const outputAbs = resolveProjectPath(root, projectId, "export", stamp, fileName);
  const reportAbs = resolveProjectPath(root, projectId, "export", stamp, "render_report.json");
  const specAbs = resolveProjectPath(root, projectId, "export", stamp, "render_spec.json");
  const folder = resolveProjectPath(root, projectId, "export", stamp);

  const specElements = members.map((element) => ({
    id: element.id,
    src: resolveProjectFile(root, projectId, element.src),
    x: Number(element.x) || 0,
    y: Number(element.y) || 0,
    w: Number(element.w) || 0,
    h: Number(element.h) || 0,
  }));
  const spec = {
    schema: "ai_studio.canvas.render_group_spec.v1",
    scale: renderScale,
    background: bg,
    group: { x: Number(group.x) || 0, y: Number(group.y) || 0, w: Number(group.w) || 0, h: Number(group.h) || 0 },
    output: outputAbs,
    report: reportAbs,
    elements: specElements,
  };
  writeProjectBytes(specAbs, `${JSON.stringify(spec, null, 2)}\n`);

  await runPython(root, ["ai_studio/assets/canvas/tools/render_group.py", "--spec", specAbs]);

  let report = {};
  try {
    report = JSON.parse(readFileSync(reportAbs, "utf8"));
  } catch {
    // The PNG is the real product; a missing/foreign report is non-fatal.
  }

  const manifest = {
    schema: "ai_studio.canvas.export.v1",
    kind: "screen",
    project: project.id,
    at: new Date().toISOString(),
    group: { id: group.id, name: group.name || group.id },
    scale: renderScale,
    background: bg,
    file: fileName,
    width: report.width || Math.max(1, Math.round((Number(group.w) || 0) * renderScale)),
    height: report.height || Math.max(1, Math.round((Number(group.h) || 0) * renderScale)),
    items: members.map((element) => ({ elementId: element.id, name: element.name || element.id, src: element.src })),
  };
  writeProjectBytes(
    resolveProjectPath(root, projectId, "export", stamp, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
  );

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "render_group",
    at: new Date().toISOString(),
    params: { groupId, scale: renderScale, background: bg },
    result_summary: { file: fileName, folder, member_count: members.length, width: manifest.width, height: manifest.height },
  };
  updateProject(root, projectId, { tool_runs: [...(getProject(root, projectId).tool_runs || []), run] });
  return { folder, file: fileName, path: outputAbs, manifest, run, members: members.length };
}
