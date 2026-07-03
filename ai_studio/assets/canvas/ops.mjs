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
// they reuse the image tools functions (regions/sources bridges) unmodified.
//
// Journal / undo-redo design (single linear history over an append-only log):
//   - Each mutating op appends a THIN metadata line
//     {seq, at, op, args_summary, parent, duration_ms, has_snapshot:true}; the fat
//     before/after project snapshot lives in a sidecar <project>/snapshots/<seq>.json
//     ({undo_patch, state}). undo_patch is the {title, elements, groups, tool_runs}
//     snapshot BEFORE the op (restore target for undo); state is the snapshot AFTER
//     (re-apply target for redo); parent is the history head that was current when
//     the op ran, so the journal forms a linked chain. Files are immutable, so a
//     metadata-only snapshot always fully restores project.json.
//   - project.json carries one pointer, history_seq (the applied head; 0 = base).
//   - undo loads the head entry's snapshot.undo_patch, moves the head to
//     entry.parent, and appends a {op:"undo", target_seq} marker.
//   - redo picks the greatest-seq mutation whose parent == the current head,
//     restores its snapshot.state, advances the head, and appends a {op:"redo"}.
//   - A new mutation after an undo appends with parent == the current head, so it
//     becomes the newest child of that head; redo (greatest-seq child) then never
//     picks the stale branch — the redo tail is invalidated automatically.
//   - History is capped (canvasHistoryDepth, default 200): post-mutation compaction
//     drops entries past the Nth undo step, deletes their snapshots, archives their
//     thin lines to journal.archive.jsonl, and rebases the horizon entry's parent to
//     0 so undo stops cleanly ("nothing to undo") at the horizon.
//   - Legacy fat journals (inline undo_patch/state) are migrated transparently to
//     this sidecar layout on the first mutating open (store.ensureThinJournal), with
//     the original kept as journal.jsonl.bak.
//   - Observability: every journaled line carries duration_ms; failed ops append to
//     <project>/errors.jsonl (see recordOpFailure, wired from the API + CLI clients).
import { randomUUID } from "node:crypto";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { extname, join } from "node:path";
import { performance } from "node:perf_hooks";
import { canvasHistoryDepth } from "../../core_harness/tool_lib/studio_config.mjs";
// ALL canvas Python tools (export_images.py, crop_regions.py, render_group.py) run
// through the shared image-tools bridge (T0218 config-only interpreter from
// studio.config pythonPath) via its warm worker (T0202): one persistent Python process
// serves every spawn site, so the second and later detect/slice/render/export calls skip
// the interpreter-startup + numpy/PIL import floor. Interpreter/dependency problems are
// loud errors naming the one-shot setup command; there is no cold-spawn fallback.
import { runPython as runToolPython } from "../tools/image/_bridge/bridge.mjs";
import { detectImageRegions } from "../tools/image/regions/api.mjs";
import { uploadImageSource } from "../tools/image/sources/api.mjs";
// Scene-tree math (shared, pure): computed per-scope z-order + the front-order hook.
// Imported here so paint/composite order and the reorder op go through ONE
// implementation the site also loads — ordering itself obeys tool parity.
import { ancestorsOf, blockReorder, buildNodesSpec, descendantsOf, frontOrder, isNodeHidden, nodeScope, orderedChildren, wouldCycle } from "./tree.mjs";
// Shared, pure text/font contract (imported by the site too — see fonts.mjs). ops.mjs
// owns the node-only disk read of the manifest; all validation/merge/resolution logic
// lives in fonts.mjs so the browser and the agent normalize a style identically.
import {
  FONTS_DIR_REPO_PATH,
  FONTS_MANIFEST_REPO_PATH,
  defaultTextStyle,
  firstTextLine,
  mergeTextStyle,
  nominalTextBox,
  resolveFontEntry,
  splitTextLines,
} from "./fonts.mjs";
import {
  addFile as storeAddFile,
  addImage as storeAddImage,
  addText as storeAddText,
  appendArchive,
  appendError,
  appendJournal,
  appendJournalLine,
  capToolRuns,
  createProject as storeCreateProject,
  deleteProject as storeDeleteProject,
  deleteSnapshot,
  ensureThinJournal,
  getProject,
  imageSize,
  listProjects,
  nextJournalSeq,
  patchElement as storePatchElement,
  patchElements as storePatchElements,
  projectExists,
  readElementBytes,
  readErrors,
  readJournal,
  readSnapshot,
  removeElement as storeRemoveElement,
  removeElements as storeRemoveElements,
  resolveProjectFile,
  resolveProjectPath,
  rewriteJournal,
  updateProject,
  writeProjectBytes,
  writeSnapshot,
} from "./store.mjs";
// Minimal STORE-mode zip writer (node built-ins only) for the page's "several outputs
// -> one .zip" save-dialog flow and the CLI --zip flag. Pure; ops.zipExport gathers the
// run's files from the confined export folder and hands them here.
import { zipStore } from "./zip.mjs";

export {
  getProject,
  listProjects,
  resolveProjectFile,
  resolveProjectPath,
  updateProject,
};

// Round a millisecond duration to 3 decimals for compact, stable journal/error rows.
function ms(value) {
  return Math.round(value * 1000) / 1000;
}

// Round to int and clamp to [0, bound] (far edge inclusive) — the polygon vertex rule
// (mirrors the Python slicer's max(0, min(image_dim, x))).
function clampRound(value, bound) {
  return Math.max(0, Math.min(bound, Math.round(value)));
}

// Axis-aligned bounding box [x, y, w, h] of an integer polygon, kept inside source
// bounds (ports the legacy rectFromPolygon: floor min / ceil max, min dimension 1). A
// polygonal region stores this as its rect so shape and bbox never diverge.
function polygonBBox(points, boundsW, boundsH) {
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  for (const [px, py] of points) {
    if (px < minX) minX = px;
    if (py < minY) minY = py;
    if (px > maxX) maxX = px;
    if (py > maxY) maxY = py;
  }
  let x = Math.max(0, Math.floor(minX));
  let y = Math.max(0, Math.floor(minY));
  const w = Math.max(1, Math.ceil(maxX) - x);
  const h = Math.max(1, Math.ceil(maxY) - y);
  if (x + w > boundsW) x = Math.max(0, boundsW - w);
  if (y + h > boundsH) y = Math.max(0, boundsH - h);
  return [x, y, w, h];
}

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

// ---- text fonts (node side of the shared fonts.mjs contract) -----------------

// Read the bundled fonts.json manifest from disk (node-only; the page fetches the
// same file over HTTP). A loud error names the manifest path when it is missing or
// corrupt — text can't be validated without it.
function readFontsManifest(root) {
  const path = join(root, FONTS_MANIFEST_REPO_PATH);
  if (!existsSync(path)) throw new Error(`canvas fonts manifest not found: ${FONTS_MANIFEST_REPO_PATH}`);
  try {
    return JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, ""));
  } catch (error) {
    throw new Error(`canvas fonts manifest is not valid JSON (${FONTS_MANIFEST_REPO_PATH}): ${error.message}`);
  }
}

// Absolute path to a manifest font entry's .ttf, for render_group.py / PIL. entry.file
// is a trusted repo-relative path from our own manifest.
function resolveFontFileAbs(root, entry) {
  return join(root, FONTS_DIR_REPO_PATH, entry.file);
}

// Sanitize a patch that may carry text `content`/`style` for a TEXT element: validate
// + normalize the style against the manifest (loud on unknown family/weight), coerce
// content to a string, and pass every other field (x/y/w/h/name/visible) through
// untouched. A no-op fast path when the patch has neither, so image patches never load
// the manifest. Throws if content/style target a non-text element.
function sanitizeTextPatch(root, project, elementId, patch = {}) {
  if (patch.style === undefined && patch.content === undefined) return patch;
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "text") {
    throw new Error(`element ${elementId} is not a text element (content/style only apply to type:"text")`);
  }
  const clean = { ...patch };
  if (patch.content !== undefined) clean.content = String(patch.content);
  if (patch.style !== undefined) {
    const manifest = readFontsManifest(root);
    clean.style = mergeTextStyle(element.style || defaultTextStyle(), patch.style, manifest);
  }
  return clean;
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

// A mutation line (has a sidecar snapshot); tolerates a legacy inline fat line too,
// so undo/redo/history stay correct even if migration has not run yet.
function isMutation(line) {
  return !!line && (line.has_snapshot === true || line.undo_patch !== undefined || line.state !== undefined);
}

// The {undo_patch, state} snapshot for one entry: inline (legacy fat line) or the
// sidecar file (thin line). Returns {} if neither is present.
function snapshotForEntry(root, projectId, entry) {
  if (entry && (entry.undo_patch !== undefined || entry.state !== undefined)) {
    return { undo_patch: entry.undo_patch, state: entry.state };
  }
  return readSnapshot(root, projectId, entry.seq) || {};
}

// Append one mutation entry and advance the project's history head. Returns the
// saved project (with the new history_seq). If the op changed nothing, no entry is
// written and the current project state is returned unchanged. Writes the fat
// snapshot to a sidecar and keeps the journal line thin (op metadata + duration_ms),
// then compacts history past the depth cap. `startedAt` is a performance.now() taken
// at op entry, so the recorded duration_ms covers the whole op (incl. any Python).
// Actor attribution (T0228): which kind of client drives the ops — "user" (the page
// over HTTP; also the default for direct imports/tests) or "agent" (the CLI). Set once
// per process at the transport seam (cli.mjs boot); no per-op signature churn. Recorded
// on every mutation entry; readers treat an absent field (pre-T0228 entries) as "user".
let opsActor = "user";
export function setOpsActor(actor) {
  if (actor !== "user" && actor !== "agent") throw new Error(`unknown ops actor: ${JSON.stringify(actor)}`);
  opsActor = actor;
}

function commitMutation(root, projectId, { op, args_summary, before, after, startedAt }) {
  ensureThinJournal(root, projectId); // one-time migration of a legacy fat journal
  const undoPatch = snapshotOf(before);
  const state = snapshotOf(after);
  if (JSON.stringify(undoPatch) === JSON.stringify(state)) return after; // no-op: no entry
  const seq = nextJournalSeq(root, projectId);
  writeSnapshot(root, projectId, seq, { undo_patch: undoPatch, state });
  appendJournalLine(root, projectId, {
    seq,
    at: new Date().toISOString(),
    op,
    actor: opsActor,
    args_summary: args_summary || {},
    parent: Number(before.history_seq) || 0,
    duration_ms: startedAt === undefined ? undefined : ms(performance.now() - startedAt),
    has_snapshot: true,
  });
  const saved = updateProject(root, projectId, { history_seq: seq });
  compactJournal(root, projectId);
  return saved;
}

// Bound retained undo depth to canvasHistoryDepth (default 200; <= 0 disables). Walk
// the undo chain from the current head; if it exceeds the cap, keep every line with
// seq >= the horizon (the cap-th step from the tip), archive + drop the rest, delete
// their snapshots, and rebase the horizon entry's parent to 0 so undo bottoms out
// cleanly there. Redo children of kept entries always have a larger seq, so the
// redo/undo tree for the retained window is fully preserved. Runs post-mutation only
// (undo/redo never grow depth), and since it physically shrinks the journal to ~cap
// lines the per-op scan stays bounded rather than O(session).
function compactJournal(root, projectId) {
  const cap = canvasHistoryDepth(root);
  if (!(cap > 0)) return; // unlimited: compaction disabled
  const head = Number(getProject(root, projectId).history_seq) || 0;
  if (!head) return;
  const journal = readJournal(root, projectId);
  const mutationsBySeq = new Map();
  for (const line of journal) if (isMutation(line)) mutationsBySeq.set(Number(line.seq), line);

  const chain = [];
  const guard = new Set();
  let cursor = head;
  while (cursor && mutationsBySeq.has(cursor) && !guard.has(cursor) && chain.length < cap + 1) {
    guard.add(cursor);
    const entry = mutationsBySeq.get(cursor);
    chain.push(entry);
    cursor = Number(entry.parent) || 0;
  }
  if (chain.length <= cap) return; // within the cap: nothing to drop

  const horizonSeq = Number(chain[cap - 1].seq);
  const kept = [];
  const dropped = [];
  for (const line of journal) {
    if (Number(line.seq) >= horizonSeq) {
      // Rebase the horizon entry so the last retained undo lands on base (0).
      if (Number(line.seq) === horizonSeq && isMutation(line)) line.parent = 0;
      kept.push(line);
    } else {
      dropped.push(line);
    }
  }
  appendArchive(root, projectId, dropped);
  for (const line of dropped) if (isMutation(line)) deleteSnapshot(root, projectId, Number(line.seq));
  rewriteJournal(root, projectId, kept);
}

// Append an errors.jsonl row for a failed op (project-resolvable failures only; a
// missing/unsafe project id can't be logged). Wired from the API and CLI clients so
// every surfaced failure leaves a trail without masking the caller's error.
export function recordOpFailure(root, projectId, { op, args_summary, error, duration_ms } = {}) {
  if (!projectId) return false;
  return appendError(root, projectId, {
    op: op || "",
    args_summary: args_summary || {},
    error: error && error.message ? error.message : String(error),
    duration_ms: duration_ms === undefined ? undefined : ms(duration_ms),
  });
}

// ---- journaled store wrappers ------------------------------------------------

export function addImage(root, projectId, args = {}) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const result = storeAddImage(root, projectId, args);
  // Keep an explicitly-ordered destination scope explicit (scopes never go
  // half-explicit): a fresh image lands at the FRONT of its scope. Computed from
  // `before` (the scope's siblings prior to the add); a no-op on any scope that
  // has never been reordered (frontOrder === null).
  let after = result.project;
  const fo = frontOrder(before, result.element.groupId == null ? null : result.element.groupId);
  if (fo !== null) {
    after = updateProject(root, projectId, {
      elements: (result.project.elements || []).map((element) =>
        element.id === result.element.id ? { ...element, order: fo } : element,
      ),
    });
  }
  const project = commitMutation(root, projectId, {
    op: "addImage",
    args_summary: { name: result.element.name, elementId: result.element.id, w: result.element.w, h: result.element.h },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === result.element.id) || result.element;
  return { project, element };
}

// Add SEVERAL images in ONE journaled gesture — the page's multi-file drop/paste. Each
// image is {name, bytes, x?, y?}; they append in array order and, when the destination
// (root) scope is already explicitly ordered, land stacked at the FRONT in add order (so
// a reordered scope stays explicit — the same front hook addImage uses, extended to a
// batch). Loud + atomic: EVERY image's bytes are validated up front (non-empty + a
// parseable header), so a bad image in the batch throws before any element is appended;
// an empty list is a loud error (the page only calls this with real files). The
// content-addressed file writes are harmless even on a later throw (immutable storage
// keeps orphan files; only project.json state matters, and it stays at `before`). One
// commitMutation, so a single undo removes every added image. Single adds stay on addImage.
export function addImages(root, projectId, { images } = {}) {
  if (!projectId) throw new Error("addImages requires projectId");
  if (!Array.isArray(images) || !images.length) throw new Error("addImages requires a non-empty images array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  // Validate every image FIRST (atomic like patchElements): coerce bytes to a non-empty
  // Buffer and parse the header (imageSize throws on a corrupt/unsupported image).
  const prepared = images.map((image, index) => {
    if (!image || typeof image !== "object") throw new Error(`image ${index} is not an object`);
    const bytes = Buffer.isBuffer(image.bytes) ? image.bytes : Buffer.from(image.bytes || []);
    if (!bytes.length) throw new Error(`image ${index} (${image.name || "unnamed"}) has empty bytes`);
    imageSize(bytes); // throws on an unsupported/corrupt header
    return { name: image.name, bytes, x: image.x, y: image.y };
  });
  const added = [];
  for (const image of prepared) added.push(storeAddImage(root, projectId, image).element.id);
  // Front-order hook (addImage's, extended to a batch): stack the fresh images at the
  // front of the root scope in add order when that scope is already explicit; a no-op on
  // a never-reordered scope.
  let after = getProject(root, projectId);
  let fo = frontOrder(before, null);
  if (fo !== null) {
    const orderById = new Map(added.map((id) => [id, fo++]));
    after = updateProject(root, projectId, {
      elements: (after.elements || []).map((element) =>
        orderById.has(element.id) ? { ...element, order: orderById.get(element.id) } : element,
      ),
    });
  }
  const project = commitMutation(root, projectId, {
    op: "addImages",
    args_summary: { count: added.length, elementIds: added },
    before,
    after,
    startedAt,
  });
  const idSet = new Set(added);
  return { project, elements: (project.elements || []).filter((item) => idSet.has(item.id)), count: added.length };
}

// Add a TEXT element (Figma text node). Mirrors addImage: builds the element via the
// store, gives it a FRONT order when its destination scope is already explicit (so a
// reordered scope stays explicit), and journals ONE undoable entry. `style` is merged
// over the defaults and validated LOUDLY against the fonts manifest (an unknown
// family/weight, bad align/color, or non-finite size throws before any write). Optional
// `groupId` drops the text straight into a group (validated). The stored w/h is a
// NOMINAL box (the page re-measures on open, the renderer re-measures at export).
export function addText(root, projectId, args = {}) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const manifest = readFontsManifest(root);
  const style = mergeTextStyle(defaultTextStyle(), args.style || {}, manifest);
  const content = args.content == null ? "Text" : String(args.content);
  const groupId = args.groupId == null || args.groupId === "" ? undefined : String(args.groupId);
  if (groupId && !groupsOf(before).some((group) => group.id === groupId)) {
    throw new Error(`group not found: ${groupId}`);
  }
  const box = nominalTextBox(content, style);
  const name = firstTextLine(content) || "Text";
  const result = storeAddText(root, projectId, {
    x: args.x,
    y: args.y,
    w: box.w,
    h: box.h,
    content,
    style,
    name,
    groupId,
  });
  // Front-order hook (identical to addImage): a fresh text lands at the FRONT of its
  // scope when that scope is already explicitly ordered; a no-op otherwise.
  let after = result.project;
  const fo = frontOrder(before, result.element.groupId == null ? null : result.element.groupId);
  if (fo !== null) {
    after = updateProject(root, projectId, {
      elements: (result.project.elements || []).map((element) =>
        element.id === result.element.id ? { ...element, order: fo } : element,
      ),
    });
  }
  const project = commitMutation(root, projectId, {
    op: "addText",
    args_summary: {
      elementId: result.element.id,
      content: content.slice(0, 40),
      fontFamily: style.fontFamily,
      fontSize: style.fontSize,
    },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === result.element.id) || result.element;
  return { project, element };
}

export function patchElement(root, projectId, elementId, patch = {}) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const clean = sanitizeTextPatch(root, before, elementId, patch);
  const result = storePatchElement(root, projectId, elementId, clean);
  const project = commitMutation(root, projectId, {
    op: "patchElement",
    args_summary: { elementId, patch: clean },
    before,
    after: result.project,
    startedAt,
  });
  return { project, element: result.element };
}

export function removeElement(root, projectId, elementId) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const result = storeRemoveElement(root, projectId, elementId);
  const project = commitMutation(root, projectId, {
    op: "removeElement",
    args_summary: { elementId },
    before,
    after: result.project,
    startedAt,
  });
  return { project, removed: result.removed };
}

// Patch several elements in ONE journaled gesture — the marquee/multi-select
// move commit and any agent multi-move. Each patch is {elementId, x?, y?, w?, h?,
// name?, visible?} with the SAME per-field rules as patchElement. A bad/missing
// elementId throws before any write (atomic — no partial batch), and the whole batch
// is ONE commitMutation, so a single undo restores the entire gesture (not N steps).
// An empty batch is a no-op (no journal entry), like the other no-op-guarded ops.
export function patchElements(root, { projectId, patches } = {}) {
  if (!projectId) throw new Error("patchElements requires projectId");
  if (!Array.isArray(patches)) throw new Error("patchElements requires a patches array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const clean = patches.map((patch, index) => {
    if (!patch || typeof patch !== "object") throw new Error(`patch ${index} is not an object`);
    const elementId = String(patch.elementId == null ? "" : patch.elementId).trim();
    if (!elementId) throw new Error(`patch ${index} is missing an elementId`);
    // Text content/style patches are validated + normalized against the manifest here
    // (same as patchElement); image geometry patches skip the manifest entirely.
    return { ...sanitizeTextPatch(root, before, elementId, patch), elementId };
  });
  const result = storePatchElements(root, projectId, clean);
  const project = commitMutation(root, projectId, {
    op: "patchElements",
    args_summary: { count: clean.length, elementIds: clean.map((patch) => patch.elementId) },
    before,
    after: result.project,
    startedAt,
  });
  const ids = new Set(clean.map((patch) => patch.elementId));
  return { project, elements: (project.elements || []).filter((item) => ids.has(item.id)), count: clean.length };
}

// Remove several elements in ONE journaled gesture — the multi-select delete. All
// ids must exist (throws before any write — atomic; no partial delete), duplicates
// are de-duplicated, and the whole batch is ONE commitMutation, so a single undo
// restores every removed element. Backing files stay on disk (immutable storage).
export function removeElements(root, { projectId, elementIds } = {}) {
  if (!projectId) throw new Error("removeElements requires projectId");
  if (!Array.isArray(elementIds)) throw new Error("removeElements requires an elementIds array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const result = storeRemoveElements(root, projectId, elementIds.map((value) => String(value)));
  const project = commitMutation(root, projectId, {
    op: "removeElements",
    args_summary: { elementIds: result.removed, count: result.removed.length },
    before,
    after: result.project,
    startedAt,
  });
  return { project, removed: result.removed };
}

// Locate a node (element OR group) by id and resolve its parent scope (the group it
// lives in, or null for root). Throws loudly on an unknown id. Element and group ids
// are disjoint namespaces, so one id resolves to at most one node.
function findNode(project, nodeId) {
  const element = (project.elements || []).find((item) => item.id === nodeId);
  if (element) return { kind: "element", ref: element, scopeId: nodeScope(project, element) };
  const group = groupsOf(project).find((item) => item.id === nodeId);
  if (group) return { kind: "group", ref: group, scopeId: nodeScope(project, group) };
  throw new Error(`node not found: ${nodeId}`);
}

// Move a node (ELEMENT or GROUP) to a target `index` among its MERGED same-scope
// siblings — the elements AND groups sharing its parent scope, in the computed
// back → front order tree.mjs.orderedChildren yields (0 = back / painted first,
// N-1 = front / painted last). The move assigns explicit contiguous `order` values
// (0..N-1) to EVERY sibling of that scope reflecting the new arrangement — the design's
// lazy per-scope normalization: the first reorder on a scope makes it explicit, and it
// never goes half-explicit afterwards (see the frontOrder hook on add/assign). Only this
// scope's siblings are touched; every other scope is left exactly as it was. One journal
// entry; undo restores the whole scope's previous `order` fields for free (snapshots).
// An unknown node or an out-of-range index is a loud error (no silent clamp — the thin
// reorderElement delegate keeps the historical clamping contract for element callers).
export function reorderNode(root, { projectId, nodeId, index } = {}) {
  if (!projectId) throw new Error("reorderNode requires projectId");
  if (!nodeId) throw new Error("reorderNode requires nodeId");
  if (!Number.isFinite(Number(index))) throw new Error("reorderNode requires a numeric index");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const node = findNode(before, nodeId);
  const target = Math.round(Number(index));

  // Merged siblings of the scope in current visual (back → front) order.
  const siblings = orderedChildren(before, node.scopeId);
  const count = siblings.length;
  if (target < 0 || target > count - 1) {
    throw new Error(`reorderNode index ${target} is out of range 0..${count - 1} for scope ${node.scopeId == null ? "(root)" : node.scopeId}`);
  }
  const from = siblings.findIndex((sibling) => sibling.id === nodeId);
  if (from === target) return { project: before, node: node.ref, kind: node.kind, index: target }; // no-op

  // New arrangement; assign contiguous order 0..N-1 to every sibling.
  const arranged = siblings.slice();
  const [moved] = arranged.splice(from, 1);
  arranged.splice(target, 0, moved);
  const orderByNodeId = new Map(arranged.map((sibling, order) => [sibling.id, order]));

  const nextElements = (before.elements || []).map((element) =>
    orderByNodeId.has(element.id) ? { ...element, order: orderByNodeId.get(element.id) } : element,
  );
  const nextGroups = groupsOf(before).map((group) =>
    orderByNodeId.has(group.id) ? { ...group, order: orderByNodeId.get(group.id) } : group,
  );

  const after = updateProject(root, projectId, { elements: nextElements, groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "reorderNode",
    args_summary: { nodeId, kind: node.kind, index: target, scope: node.scopeId },
    before,
    after,
    startedAt,
  });
  return { project, node: findNode(project, nodeId).ref, kind: node.kind, index: target };
}

// Move an ELEMENT to a target sibling index — a thin delegate to reorderNode (element
// ids ARE node ids). The index is over the MERGED same-scope siblings (elements + groups
// of the element's scope), matching the computed paint order. Kept as its own op + route
// + CLI for back-compat; it preserves the historical FORGIVING contract that an
// out-of-range index snaps to the nearest edge (reorderNode itself is strict), so page
// z-order nudges and older callers never throw on a clamp. One journal entry.
export function reorderElement(root, { projectId, elementId, index } = {}) {
  if (!projectId) throw new Error("reorderElement requires projectId");
  if (!elementId) throw new Error("reorderElement requires elementId");
  if (!Number.isFinite(Number(index))) throw new Error("reorderElement requires a numeric index");
  const project = getProject(root, projectId);
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const count = orderedChildren(project, nodeScope(project, element)).length;
  const clamped = Math.max(0, Math.min(count - 1, Math.round(Number(index))));
  const result = reorderNode(root, { projectId, nodeId: elementId, index: clamped });
  return { project: result.project, element: (result.project.elements || []).find((item) => item.id === elementId), index: clamped };
}

// Move several NODES (elements AND/OR groups) to absolute positions in ONE journaled
// gesture — the page's mixed marquee/multi-select move commit (loose elements + one or
// more group frames). Each move is {nodeId, x, y} (the node's new top-left). A group move
// cascades its FULL descendant closure (nested subgroup frames AND every element in the
// subtree) by the same delta, exactly like the single patchGroup move path; an element
// move just repositions that element. Overlap-safe: when the selection holds a group AND a
// node inside its subtree, the node shifts by the TOPMOST moved ancestor's delta only (once,
// with the parent), never twice. Loud + atomic: an unknown nodeId, a non-object move, or a
// non-finite x/y throws before any write; an empty moves list is a loud error (the page only
// calls this with real moves). One commitMutation, so a single undo restores every position.
export function moveNodes(root, { projectId, moves } = {}) {
  if (!projectId) throw new Error("moveNodes requires projectId");
  if (!Array.isArray(moves) || !moves.length) throw new Error("moveNodes requires a non-empty moves array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);

  // Resolve + validate every move against `before` FIRST (atomic). directDelta maps a
  // moved node id -> the {dx, dy} it shifts by (absolute target minus current position),
  // so an element lands exactly on x/y and a group carries that delta across its subtree.
  const directDelta = new Map();
  const nodeIds = [];
  moves.forEach((move, index) => {
    if (!move || typeof move !== "object") throw new Error(`move ${index} is not an object`);
    const nodeId = String(move.nodeId == null ? "" : move.nodeId).trim();
    if (!nodeId) throw new Error(`move ${index} is missing a nodeId`);
    if (!Number.isFinite(Number(move.x)) || !Number.isFinite(Number(move.y))) {
      throw new Error(`move ${index} (${nodeId}) requires finite x and y`);
    }
    const node = findNode(before, nodeId); // throws on an unknown id
    directDelta.set(nodeId, { dx: Number(move.x) - (Number(node.ref.x) || 0), dy: Number(move.y) - (Number(node.ref.y) || 0) });
    nodeIds.push(nodeId);
  });

  // The delta a node actually shifts by = the delta of the TOPMOST moved node in its
  // ancestor-or-self chain (ancestorsOf is nearest-first, so overwriting ends on the
  // topmost). null = the node is untouched.
  const effectiveDelta = (node) => {
    let best = directDelta.has(node.id) ? directDelta.get(node.id) : null;
    for (const ancestor of ancestorsOf(before, node)) {
      if (directDelta.has(ancestor.id)) best = directDelta.get(ancestor.id);
    }
    return best;
  };

  const nextElements = (before.elements || []).map((element) => {
    const delta = effectiveDelta(element);
    return delta ? { ...element, x: (Number(element.x) || 0) + delta.dx, y: (Number(element.y) || 0) + delta.dy } : element;
  });
  const nextGroups = groupsOf(before).map((group) => {
    const delta = effectiveDelta(group);
    return delta ? { ...group, x: (Number(group.x) || 0) + delta.dx, y: (Number(group.y) || 0) + delta.dy } : group;
  });

  const after = updateProject(root, projectId, { elements: nextElements, groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "moveNodes",
    args_summary: { count: nodeIds.length, nodeIds },
    before,
    after,
    startedAt,
  });
  return { project, count: nodeIds.length, nodeIds };
}

// Move a SET of nodes (elements AND/OR groups) as ONE journaled z-order gesture — the
// page's multi-selection Ctrl+[/] and the Order menu on a multi-selection. The selected
// same-scope siblings move together as a BLOCK preserving their relative order (Figma
// semantics: front/back jump to the edge, forward/backward nudge one step past the nearest
// unselected neighbor). A CROSS-scope selection applies the same block move independently
// per scope but stays ONE commitMutation (one undo). `direction` is front|back|forward|
// backward; `index` is an absolute slot among the unselected siblings (single-scope only).
// Exactly one of direction|index is required. Assigning contiguous order to each touched
// scope makes it explicit (the reorderNode normalization; scopes never go half-explicit).
// Loud + atomic: an unknown nodeId, an empty set, both/neither of direction|index, an
// unknown direction, or an index on a cross-scope selection throws before any write.
export function reorderNodes(root, { projectId, nodeIds, direction, index } = {}) {
  if (!projectId) throw new Error("reorderNodes requires projectId");
  if (!Array.isArray(nodeIds) || !nodeIds.length) throw new Error("reorderNodes requires a non-empty nodeIds array");
  const hasDirection = direction !== undefined && direction !== null && direction !== "";
  const hasIndex = index !== undefined && index !== null && index !== "" && Number.isFinite(Number(index));
  if (hasDirection === hasIndex) {
    throw new Error("reorderNodes requires exactly one of direction (front|back|forward|backward) or index");
  }
  const startedAt = performance.now();
  const before = getProject(root, projectId);

  // Resolve every node (throws on an unknown id) and record its scope. Ids in one scope
  // move as a block; a selection spanning several scopes applies per scope.
  const ids = nodeIds.map((value) => String(value));
  const idSet = new Set(ids);
  const scopeById = new Map();
  for (const id of ids) {
    const node = findNode(before, id);
    scopeById.set(id, node.scopeId == null ? null : node.scopeId);
  }
  const scopes = [...new Set(scopeById.values())];
  if (hasIndex && scopes.length > 1) {
    throw new Error("reorderNodes index is only valid for a single-scope selection; use direction across scopes");
  }
  const spec = hasIndex ? { index: Math.round(Number(index)) } : { direction: String(direction) };

  // Per scope: reorder its merged siblings with the selected ones moved as a block, then
  // assign contiguous 0..N-1 order across that scope (makes it explicit). A gesture that
  // doesn't change any scope's sequence (e.g. "bring forward" already at the front) is a
  // no-op that writes nothing — matching the single-node reorderNode guard.
  const orderByNodeId = new Map();
  let changed = false;
  for (const scope of scopes) {
    const siblings = orderedChildren(before, scope);
    const arranged = blockReorder(siblings, idSet, spec); // throws on a bad spec
    if (arranged.some((node, position) => node.id !== siblings[position].id)) changed = true;
    arranged.forEach((node, order) => orderByNodeId.set(node.id, order));
  }
  if (!changed) return { project: before, nodeIds: ids, count: ids.length };

  const nextElements = (before.elements || []).map((element) =>
    orderByNodeId.has(element.id) ? { ...element, order: orderByNodeId.get(element.id) } : element,
  );
  const nextGroups = groupsOf(before).map((group) =>
    orderByNodeId.has(group.id) ? { ...group, order: orderByNodeId.get(group.id) } : group,
  );

  const after = updateProject(root, projectId, { elements: nextElements, groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "reorderNodes",
    args_summary: {
      nodeIds: ids,
      direction: hasDirection ? String(direction) : undefined,
      index: hasIndex ? Math.round(Number(index)) : undefined,
      scopes: scopes.map((scope) => (scope == null ? null : scope)),
    },
    before,
    after,
    startedAt,
  });
  return { project, nodeIds: ids, count: ids.length };
}

// Replace an element's regions array (the ADJUST/SELECT step before slicing).
// Validates each region carries an id and an in-source-bounds integer rect while
// preserving any extra fields the detector/slicer attach (content_bbox, area_px,
// merged_from, ...). Journaled like any metadata mutation, so the before/after
// snapshot restores the previous regions on undo/redo. This is the one op behind
// both the page's region editing (drag/resize/rubber-band) and the CLI regions-set.
export function setRegions(root, { projectId, elementId, regions } = {}) {
  if (!projectId) throw new Error("setRegions requires projectId");
  if (!elementId) throw new Error("setRegions requires elementId");
  if (!Array.isArray(regions)) throw new Error("setRegions requires a regions array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const boundsW = Number(element.source_w) || Number(element.w) || 0;
  const boundsH = Number(element.source_h) || Number(element.h) || 0;

  const seen = new Set();
  const clean = regions.map((region, index) => {
    if (!region || typeof region !== "object") throw new Error(`region ${index} is not an object`);
    const id = String(region.id == null ? "" : region.id).trim();
    if (!id) throw new Error(`region ${index} is missing an id`);
    if (seen.has(id)) throw new Error(`duplicate region id: ${id}`);
    seen.add(id);

    // Optional polygon shape: >=3 finite [x, y] pairs, each rounded to int and clamped
    // to source bounds (far edge inclusive). A clean polygon makes this a polygonal
    // region — the discriminator is polygon.length >= 3 (no `shape` field), matching the
    // Python slicer. <3 clean points drops the polygon back to a plain rect region.
    let polygon = null;
    if (region.polygon !== undefined && region.polygon !== null) {
      if (!Array.isArray(region.polygon)) throw new Error(`region ${id} polygon must be an array of [x, y] points`);
      const points = [];
      for (const point of region.polygon) {
        if (!Array.isArray(point) || point.length !== 2) continue;
        const px = Number(point[0]);
        const py = Number(point[1]);
        if (!Number.isFinite(px) || !Number.isFinite(py)) continue;
        points.push([clampRound(px, boundsW), clampRound(py, boundsH)]);
      }
      if (points.length >= 3) polygon = points;
    }

    // rect: a polygon's stored rect IS its bounding box (rectFromPolygon), so shape and
    // bbox never diverge; otherwise validate the supplied rect as an in-bounds integer box.
    let box;
    if (polygon) {
      box = polygonBBox(polygon, boundsW, boundsH);
    } else {
      const rect = region.rect;
      if (!Array.isArray(rect) || rect.length !== 4 || !rect.every((value) => Number.isFinite(Number(value)))) {
        throw new Error(`region ${id} rect must be [x, y, w, h] numbers`);
      }
      const x = Math.round(Number(rect[0]));
      const y = Math.round(Number(rect[1]));
      const w = Math.round(Number(rect[2]));
      const h = Math.round(Number(rect[3]));
      if (w <= 0 || h <= 0) throw new Error(`region ${id} rect must have positive width and height`);
      if (x < 0 || y < 0 || x + w > boundsW || y + h > boundsH) {
        throw new Error(`region ${id} rect [${x}, ${y}, ${w}, ${h}] is out of source bounds ${boundsW}x${boundsH}`);
      }
      box = [x, y, w, h];
    }

    // Preserve extra detector/slicer fields (content_bbox, area_px, ...); normalize id +
    // rect, the optional polygon, and the optional first-class `name` (trimmed string).
    const out = { ...region, id, rect: box };
    if (polygon) out.polygon = polygon;
    else delete out.polygon;
    if (out.name !== undefined && out.name !== null) {
      const name = String(out.name).trim();
      if (name) out.name = name;
      else delete out.name;
    }
    return out;
  });

  const nextElements = (before.elements || []).map((item) =>
    item.id === elementId ? { ...item, regions: clean } : item,
  );
  const after = updateProject(root, projectId, { elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "setRegions",
    args_summary: { elementId, region_count: clean.length },
    before,
    after,
    startedAt,
  });
  const updated = (project.elements || []).find((item) => item.id === elementId);
  return { project, element: updated, regions: (updated && updated.regions) || [] };
}

// ---- per-element export settings (Figma-style rows) --------------------------
//
// An element carries an optional `export` array of rows
// [{scale, format, quality?, resample, base?}] — the Figma Export section persisted on
// the layer (T0229 removed the per-row suffix; file names are automatic at export time;
// T0235 added the additive `base` field: "source" (default, absent) or "canvas").
// setExportSettings validates + normalizes the rows and journals them
// like any metadata mutation (undo/redo restore the previous rows). The scale math
// (resolveExportScale) also runs at export time; validated here so an unknown
// scale/format/resample/base is a clear error the moment it is set, from either client.

const EXPORT_FORMATS = new Set(["png", "jpg", "webp"]);
const EXPORT_RESAMPLE = new Set(["lanczos", "nearest"]);
// T0235: export base — which dims a row's scale token resolves against. "source" (the
// default, stored as an ABSENT field to keep old rows/JSON untouched) resolves against
// the element's original source pixels (source_w/h); "canvas" resolves against the
// element's CURRENT on-canvas w/h at export time (tracks later resizes).
const EXPORT_BASES = new Set(["source", "canvas"]);
const MAX_EXPORT_DIM = 16384;
// T0229: the per-row filename suffix was removed — file naming is now automatic
// (element/screen name + a Figma-style scale marker only when several rows would
// collide). The default row therefore carries no suffix.
const DEFAULT_EXPORT_ROW = { scale: "1x", format: "png", resample: "lanczos" };

// Parse a Figma-style scale token into a spec: a multiplier ("0.5x", "1x", "2x",
// "3x", "4x", or a bare "2") or a fixed target dimension ("512w" = 512px wide,
// "512h" = 512px tall; the other axis keeps aspect). Throws on anything else so an
// unknown scale is a clear validation error, not a silent fallback.
export function parseScaleSpec(token) {
  const text = String(token == null ? "" : token).trim().toLowerCase();
  if (!text) throw new Error("export scale is required (e.g. 1x, 2x, 512w, 512h)");
  const mul = /^(\d+(?:\.\d+)?)x?$/.exec(text);
  if (mul) {
    const value = Number(mul[1]);
    if (!(value > 0)) throw new Error(`export scale must be > 0: ${JSON.stringify(token)}`);
    return { kind: "mul", value, token: `${mul[1]}x` };
  }
  const dim = /^(\d+(?:\.\d+)?)(w|h)$/.exec(text);
  if (dim) {
    const value = Number(dim[1]);
    if (!(value > 0)) throw new Error(`export scale pixels must be > 0: ${JSON.stringify(token)}`);
    return { kind: dim[2], value, token: text };
  }
  throw new Error(`invalid export scale ${JSON.stringify(token)} (use 0.5x/1x/2x, or 512w/512h)`);
}

// Resolve a scale token against a source size to the exact target pixels. A fixed
// w/h target keeps aspect on the other axis. Guards against an accidentally huge
// render (a bare multiplier like "512" would be 512x) by capping each axis.
export function resolveExportScale(token, srcW, srcH) {
  const spec = parseScaleSpec(token);
  const w0 = Math.max(1, Number(srcW) || 0);
  const h0 = Math.max(1, Number(srcH) || 0);
  let width;
  let height;
  if (spec.kind === "mul") {
    width = Math.max(1, Math.round(w0 * spec.value));
    height = Math.max(1, Math.round(h0 * spec.value));
  } else if (spec.kind === "w") {
    width = Math.max(1, Math.round(spec.value));
    height = Math.max(1, Math.round(h0 * (spec.value / w0)));
  } else {
    height = Math.max(1, Math.round(spec.value));
    width = Math.max(1, Math.round(w0 * (spec.value / h0)));
  }
  if (width > MAX_EXPORT_DIM || height > MAX_EXPORT_DIM) {
    throw new Error(`export scale ${JSON.stringify(token)} exceeds ${MAX_EXPORT_DIM}px (${width}x${height})`);
  }
  return { width, height };
}

// Validate + normalize export rows to {scale, format, resample, quality?, base?}.
// quality (1-100) is kept only for the lossy formats (jpg/webp), defaulting to 90;
// a png row never carries a quality. base (T0235) is kept ONLY when it resolves to
// "canvas" — the default "source" (absent or explicit) is dropped so stored JSON stays
// minimal and old rows are untouched. Throws on any invalid field.
//
// Suffix (T0229): the field is GONE. `rejectSuffix` splits the two callers by the
// additive-schema stance: setExportSettings (a NEW WRITE) rejects any row carrying a
// `suffix` LOUDLY (a stale client) so bad rows never persist; the export READERS
// (rowsForElement / override rows) leave it false and simply ignore a legacy stored
// `suffix` — no silent write-back, filenames come from the automatic namer instead.
function cleanExportRows(rows, { rejectSuffix = false } = {}) {
  if (!Array.isArray(rows)) throw new Error("export rows must be an array");
  return rows.map((row, index) => {
    if (!row || typeof row !== "object") throw new Error(`export row ${index} is not an object`);
    if (rejectSuffix && row.suffix !== undefined) {
      throw new Error(
        `export row ${index} carries a removed "suffix" field — export file names are automatic now (T0229); drop suffix`,
      );
    }
    const scale = String(row.scale == null ? "" : row.scale).trim() || "1x";
    parseScaleSpec(scale); // validate syntax now (throws on invalid)
    const format = String(row.format == null ? "png" : row.format).trim().toLowerCase();
    if (!EXPORT_FORMATS.has(format)) {
      throw new Error(`export row ${index} format must be png/jpg/webp, got ${JSON.stringify(row.format)}`);
    }
    const resample = String(row.resample == null ? "lanczos" : row.resample).trim().toLowerCase();
    if (!EXPORT_RESAMPLE.has(resample)) {
      throw new Error(`export row ${index} resample must be lanczos/nearest, got ${JSON.stringify(row.resample)}`);
    }
    const clean = { scale, format, resample };
    if (format === "jpg" || format === "webp") {
      const raw = row.quality === undefined || row.quality === null || row.quality === "" ? 90 : Number(row.quality);
      if (!Number.isFinite(raw)) throw new Error(`export row ${index} quality must be a number 1-100`);
      clean.quality = Math.max(1, Math.min(100, Math.round(raw)));
    }
    if (row.base !== undefined && row.base !== null && row.base !== "") {
      const base = String(row.base).trim().toLowerCase();
      if (!EXPORT_BASES.has(base)) {
        throw new Error(`export row ${index} base must be source/canvas, got ${JSON.stringify(row.base)}`);
      }
      if (base === "canvas") clean.base = base; // "source" is the default: omit, keep JSON minimal
    }
    return clean;
  });
}

// Replace an element's export rows (the Figma Export section persisted on the layer).
// Journaled like setRegions: the before/after snapshot restores the previous rows on
// undo/redo. Both the page's Export section and the CLI export-set drive this one op.
export function setExportSettings(root, { projectId, elementId, rows } = {}) {
  if (!projectId) throw new Error("setExportSettings requires projectId");
  if (!elementId) throw new Error("setExportSettings requires elementId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const clean = cleanExportRows(rows, { rejectSuffix: true });
  const nextElements = (before.elements || []).map((item) =>
    item.id === elementId ? { ...item, export: clean } : item,
  );
  const after = updateProject(root, projectId, { elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "setExportSettings",
    args_summary: { elementId, row_count: clean.length },
    before,
    after,
    startedAt,
  });
  const updated = (project.elements || []).find((item) => item.id === elementId);
  return { project, element: updated, rows: (updated && updated.export) || [] };
}

// ---- project-level ops -------------------------------------------------------

// Instant-create default titles (Figma-style: no name prompt). Two small
// game-flavored word lists; the generator lives here in the op layer, not the
// page, so the CLI and the "+ New project" button get identical defaults.
const TITLE_ADJECTIVES = [
  "Amber", "Blazing", "Crimson", "Drifting", "Emerald", "Frosty", "Golden",
  "Hidden", "Indigo", "Jade", "Lunar", "Mystic", "Neon", "Obsidian", "Radiant", "Velvet",
];
const TITLE_NOUNS = [
  "Fox", "Griffin", "Nebula", "Phoenix", "Quest", "Raven", "Relic", "Rogue",
  "Sentinel", "Talisman", "Voyager", "Wraith", "Wyvern", "Citadel", "Compass", "Portal",
];

function randomProjectTitle() {
  const adjective = TITLE_ADJECTIVES[Math.floor(Math.random() * TITLE_ADJECTIVES.length)];
  const noun = TITLE_NOUNS[Math.floor(Math.random() * TITLE_NOUNS.length)];
  return `${adjective} ${noun}`;
}

// Create a project. `title` is optional: a missing/empty title gets a random
// "Adjective Noun" default (Title Case) instead of a name prompt. The id still
// derives from the (possibly generated) title via the store's existing slug
// scheme, so a random title yields ids like amber-fox-a1b2c3.
export function createProject(root, { title } = {}) {
  const cleanTitle = String(title || "").trim() || randomProjectTitle();
  return storeCreateProject(root, { title: cleanTitle });
}

// Rename a project. Journaled like any metadata mutation: the title lives in the
// snapshot, so undo/redo restore it together with elements/groups/tool_runs.
export function patchProject(root, { projectId, title } = {}) {
  if (!projectId) throw new Error("patchProject requires projectId");
  if (title === undefined) throw new Error("patchProject requires a title");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const cleanTitle = String(title).trim() || before.title;
  const after = updateProject(root, projectId, { title: cleanTitle });
  const project = commitMutation(root, projectId, {
    op: "patchProject",
    args_summary: { title: cleanTitle },
    before,
    after,
    startedAt,
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

// Validate + normalize an optional group background (additive field). Accepts null
// (clear) or {type:"color", color:"#rrggbb"}; anything else throws a loud error (no
// silent fallback). Returns null or the normalized {type:"color", color} object.
function normalizeGroupBackground(background) {
  if (background === null) return null;
  if (typeof background !== "object" || Array.isArray(background)) {
    throw new Error(`group background must be null or {type:"color", color:"#rrggbb"}, got ${JSON.stringify(background)}`);
  }
  if (background.type !== "color") {
    throw new Error(`group background type must be "color", got ${JSON.stringify(background.type)}`);
  }
  const color = hexColor(background.color);
  if (!color) throw new Error(`group background color must be #rrggbb, got ${JSON.stringify(background.color)}`);
  return { type: "color", color };
}

// Validate an optional group clip flag (additive field). Accepts only a real boolean;
// anything else is a loud error (no silent coercion — the CLI converts its string flag
// before calling). Returns the boolean; `false` is the "unclipped" default that patchGroup
// stores as an ABSENT field (mirrors background:null), so an untouched group stays clean.
function normalizeGroupClip(clip) {
  if (typeof clip !== "boolean") {
    throw new Error(`group clip must be a boolean (true|false), got ${JSON.stringify(clip)}`);
  }
  return clip;
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
// which are assigned this group. Optional `parentId` NESTS the new group inside an
// existing group (validated; null/absent = a top-level screen); for fromElements a
// missing parentId defaults to the members' COMMON groupId (nest a widget group
// inside the screen it was built from), root when they differ. One journal entry.
export function createGroup(root, { projectId, name, x, y, w, h, fromElements, parentId } = {}) {
  if (!projectId) throw new Error("createGroup requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const groupId = `grp_${randomUUID().slice(0, 8)}`;
  const cleanName = String(name || "").trim() || "Group";

  let bounds;
  let memberIds = [];
  let members = [];
  if (Array.isArray(fromElements) && fromElements.length) {
    memberIds = fromElements.map(String);
    members = memberIds.map((id) => {
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

  // Resolve the parent scope. An explicit parentId (validated below) wins; else, for
  // fromElements, the members' common groupId; else root.
  let parentScope;
  if (parentId !== undefined) {
    parentScope = parentId == null || parentId === "" ? null : String(parentId);
  } else if (members.length) {
    const scopes = new Set(members.map((m) => (m.groupId == null || m.groupId === "" ? null : String(m.groupId))));
    parentScope = scopes.size === 1 ? [...scopes][0] : null;
  } else {
    parentScope = null;
  }
  if (parentScope != null) findGroup(before, parentScope); // loud error on an unknown parent

  const group = { id: groupId, name: cleanName, x: bounds.x, y: bounds.y, w: bounds.w, h: bounds.h, visible: true };
  if (parentScope != null) group.parentId = parentScope;
  // Keep an explicitly-ordered destination scope explicit by giving the new group a
  // front order (no-op on a never-reordered scope).
  const groupFront = frontOrder(before, parentScope);
  if (groupFront !== null) group.order = groupFront;
  const memberSet = new Set(memberIds);
  // Members entering the fresh group scope drop any stale `order` from their old scope,
  // so the new group starts implicit (v1 array-order fallback) rather than half-explicit.
  const nextElements = (before.elements || []).map((element) => {
    if (!memberSet.has(element.id)) return element;
    const moved = { ...element, groupId };
    delete moved.order;
    return moved;
  });
  const after = updateProject(root, projectId, {
    groups: [...groupsOf(before), group],
    elements: nextElements,
  });
  const project = commitMutation(root, projectId, {
    op: "createGroup",
    args_summary: { groupId, name: cleanName, members: memberIds, bounds, parentId: parentScope },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Patch a group's name/bounds/visibility/background/clip. When x or y change, translate
// the group's FULL descendant closure by the same delta — nested subgroup frames AND
// every element in the subtree — so the whole screen (and its nested widget groups)
// moves as one; resize (w/h) never moves members. `background` is the optional solid
// fill (null clears it; {type:"color", color:"#rrggbb"} sets it — validated, no silent
// fallback). `clip` is the optional Figma-frame clip flag: `true` clips members to the
// group bounds on canvas AND in the subgroup render; `false` (the default) clears it and
// is stored as an ABSENT field, so an untouched group stays clean and clip:false on an
// already-unclipped group makes no change (no journal entry). One journal entry restores
// everything on undo.
export function patchGroup(root, { projectId, groupId, name, x, y, w, h, visible, background, clip } = {}) {
  if (!projectId) throw new Error("patchGroup requires projectId");
  if (!groupId) throw new Error("patchGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const current = findGroup(before, groupId);

  const dx = finite(x) ? Number(x) - Number(current.x || 0) : 0;
  const dy = finite(y) ? Number(y) - Number(current.y || 0) : 0;
  // Validate background + clip BEFORE any write so an invalid value throws atomically.
  const bgProvided = background !== undefined;
  const bgResolved = bgProvided ? normalizeGroupBackground(background) : undefined;
  const clipProvided = clip !== undefined;
  const clipResolved = clipProvided ? normalizeGroupClip(clip) : undefined;

  // On a move, gather the FULL descendant closure once: nested subgroup frames AND
  // every element in the subtree translate with the group.
  const moving = dx !== 0 || dy !== 0;
  const descendants = moving ? descendantsOf(before, groupId) : { groups: [], elements: [] };
  const descGroupIds = new Set(descendants.groups.map((g) => g.id));
  const descElementIds = new Set(descendants.elements.map((e) => e.id));

  const nextGroups = groupsOf(before).map((group) => {
    if (group.id === groupId) {
      const patched = { ...group };
      if (name !== undefined) patched.name = String(name);
      if (finite(x)) patched.x = Number(x);
      if (finite(y)) patched.y = Number(y);
      if (finite(w)) patched.w = Number(w);
      if (finite(h)) patched.h = Number(h);
      if (visible !== undefined) patched.visible = !(visible === false || visible === "false");
      if (bgProvided) {
        if (bgResolved === null) delete patched.background; // "None" -> absent field
        else patched.background = bgResolved;
      }
      if (clipProvided) {
        if (clipResolved === false) delete patched.clip; // unclipped -> absent field
        else patched.clip = true;
      }
      return patched;
    }
    // A nested subgroup frame translates with the closure (its own members are in the
    // element closure below).
    if (moving && descGroupIds.has(group.id)) {
      return { ...group, x: (Number(group.x) || 0) + dx, y: (Number(group.y) || 0) + dy };
    }
    return group;
  });

  const nextElements = moving
    ? (before.elements || []).map((element) =>
        descElementIds.has(element.id)
          ? { ...element, x: (Number(element.x) || 0) + dx, y: (Number(element.y) || 0) + dy }
          : element,
      )
    : (before.elements || []);

  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "patchGroup",
    args_summary: { groupId, name, x, y, w, h, visible, dx, dy, background: bgProvided ? bgResolved : undefined, clip: clipProvided ? clipResolved : undefined },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Patch several groups with the SAME shared field(s) in ONE journaled gesture — the
// multi-group inspector's shared toggles (Visible / Clip). Only fields that make sense to
// set uniformly across a selection are honored here: `visible` and `clip`. Per-group
// geometry (x/y/w/h/name/background) is intentionally NOT batched (moves would need the
// subtree cascade; a shared name/color is meaningless across a selection). Loud + atomic:
// every id must resolve (throws before any write), `clip` is validated once, and at least
// one of visible/clip must be provided. The whole batch is ONE commitMutation, so a single
// undo restores every group. `clip:false` clears the flag to an ABSENT field (mirrors
// patchGroup), so a no-op toggle on already-unclipped groups changes nothing.
export function patchGroups(root, { projectId, groupIds, visible, clip } = {}) {
  if (!projectId) throw new Error("patchGroups requires projectId");
  if (!Array.isArray(groupIds) || !groupIds.length) throw new Error("patchGroups requires a non-empty groupIds array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const ids = groupIds.map((value) => String(value));
  for (const groupId of ids) findGroup(before, groupId); // atomic: throws on an unknown id
  const idSet = new Set(ids);

  const visProvided = visible !== undefined;
  const clipProvided = clip !== undefined;
  if (!visProvided && !clipProvided) throw new Error("patchGroups requires at least one of visible, clip");
  const clipResolved = clipProvided ? normalizeGroupClip(clip) : undefined; // validate before any write

  const nextGroups = groupsOf(before).map((group) => {
    if (!idSet.has(group.id)) return group;
    const patched = { ...group };
    if (visProvided) patched.visible = !(visible === false || visible === "false");
    if (clipProvided) {
      if (clipResolved === false) delete patched.clip;
      else patched.clip = true;
    }
    return patched;
  });
  const after = updateProject(root, projectId, { groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "patchGroups",
    args_summary: {
      groupIds: ids,
      count: ids.length,
      visible: visProvided ? !(visible === false || visible === "false") : undefined,
      clip: clipProvided ? clipResolved : undefined,
    },
    before,
    after,
    startedAt,
  });
  return { project, groups: (project.groups || []).filter((group) => idSet.has(group.id)), count: ids.length };
}

// Resize a group's frame to fit its content (Figma "Resize to fit"). The new frame is
// the union bounding box of the group's FULL descendant closure — every descendant
// element AND every nested subgroup frame (both carry x/y/w/h; reuses the same
// elementsBBox math createGroup/sliceRegions use) — expanded by `padding` on all sides
// (default 24, the shared slice/group-create pad). Children NEVER move: only the group's
// own x/y/w/h change, so with clip=true the new frame re-evaluates the clip (the whole
// point of the button). An empty group (no descendant content) is a loud error, as is a
// non-finite or negative padding (no silent fallback). One journal entry; undo restores
// the old frame. `background`/`clip`/`parentId`/`order`/`name`/`visible` are preserved.
export function fitGroup(root, { projectId, groupId, padding } = {}) {
  if (!projectId) throw new Error("fitGroup requires projectId");
  if (!groupId) throw new Error("fitGroup requires groupId");
  const pad = padding === undefined || padding === null ? 24 : Number(padding);
  if (!Number.isFinite(pad) || pad < 0) {
    throw new Error(`fitGroup padding must be a finite number >= 0, got ${JSON.stringify(padding)}`);
  }
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  findGroup(before, groupId); // loud error on an unknown group
  const descendants = descendantsOf(before, groupId);
  const boxes = [...descendants.elements, ...descendants.groups];
  if (!boxes.length) throw new Error(`group ${groupId} has nothing to fit (no descendant content)`);
  const { minX, minY, maxX, maxY } = elementsBBox(boxes);
  const frame = { x: minX - pad, y: minY - pad, w: maxX - minX + pad * 2, h: maxY - minY + pad * 2 };

  const nextGroups = groupsOf(before).map((group) =>
    group.id === groupId ? { ...group, x: frame.x, y: frame.y, w: frame.w, h: frame.h } : group,
  );
  const after = updateProject(root, projectId, { groups: nextGroups });
  const project = commitMutation(root, projectId, {
    op: "fitGroup",
    args_summary: { groupId, padding: pad, x: frame.x, y: frame.y, w: frame.w, h: frame.h },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Assign elements to a group (groupId) or clear their group (groupId=null). One
// journal entry.
export function assignToGroup(root, { projectId, elementIds, groupId } = {}) {
  if (!projectId) throw new Error("assignToGroup requires projectId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const ids = Array.isArray(elementIds) ? elementIds.map(String) : [];
  if (!ids.length) throw new Error("assignToGroup requires elementIds");
  const target = groupId == null || groupId === "" ? null : String(groupId);
  if (target) findGroup(before, target);
  const idSet = new Set(ids);
  for (const id of ids) {
    if (!(before.elements || []).some((item) => item.id === id)) throw new Error(`element not found: ${id}`);
  }
  // Scope-change order rule (scopes never go half-explicit): when the destination scope
  // is explicitly ordered, each moved element gets a fresh FRONT order (stacked in
  // elements[] order); otherwise its `order` is dropped so it sorts by the v1 fallback in
  // the new scope and never leaves a stale key from its previous scope behind.
  let fo = frontOrder(before, target);
  const nextElements = (before.elements || []).map((element) => {
    if (!idSet.has(element.id)) return element;
    const moved = { ...element, groupId: target };
    if (fo === null) delete moved.order;
    else moved.order = fo++;
    return moved;
  });
  const after = updateProject(root, projectId, { elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "assignToGroup",
    args_summary: { elementIds: ids, groupId: target },
    before,
    after,
    startedAt,
  });
  return { project, count: ids.length, groupId: target };
}

// Deleting a group deletes its ENTIRE SUBTREE with it (lead 2026-07-02: a group is a
// container — dissolving one without deleting content is Ungroup). The full closure —
// nested subgroups AND every element in the subtree — goes in ONE journal entry; undo
// restores all of it. Member image files stay in files/ (non-destructive storage).
export function deleteGroup(root, { projectId, groupId } = {}) {
  if (!projectId) throw new Error("deleteGroup requires projectId");
  if (!groupId) throw new Error("deleteGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  findGroup(before, groupId);
  const descendants = descendantsOf(before, groupId);
  const removedGroupIds = new Set([groupId, ...descendants.groups.map((group) => group.id)]);
  const removedElementIds = new Set(descendants.elements.map((element) => element.id));
  const nextGroups = groupsOf(before).filter((group) => !removedGroupIds.has(group.id));
  const removedElements = (before.elements || []).filter((element) => removedElementIds.has(element.id));
  const nextElements = (before.elements || []).filter((element) => !removedElementIds.has(element.id));
  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "deleteGroup",
    args_summary: {
      groupId,
      deletedGroups: [...removedGroupIds],
      deletedElements: removedElements.map((element) => element.id),
    },
    before,
    after,
    startedAt,
  });
  return {
    project,
    removed: groupId,
    removedGroups: [...removedGroupIds],
    removedElements: removedElements.map((element) => element.id),
  };
}

// Move a group under a new parent (null = root) at an optional merged-sibling `index`
// (default = front of the destination scope). CYCLE GUARD: reject a parent that is the
// group itself or any group in its subtree (tree.wouldCycle) — a loud error, never a
// silent no-op. Order handling mirrors the "scopes never go half-explicit" invariant:
//   - with an explicit `index`, assign contiguous order 0..N over the destination's new
//     arrangement (destination becomes explicit — the reorderNode normalization);
//   - without an index (front), give the group a FRONT order iff the destination scope
//     is already explicit, else drop its (now-stale) order so the scope stays implicit.
// The group's old scope keeps its remaining siblings' orders (still explicit, gaps are
// harmless); the moved group never leaves a stale order behind. One journal entry.
export function reparentGroup(root, { projectId, groupId, parentId, index } = {}) {
  if (!projectId) throw new Error("reparentGroup requires projectId");
  if (!groupId) throw new Error("reparentGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  findGroup(before, groupId); // loud error on an unknown group
  const target = parentId == null || parentId === "" ? null : String(parentId);
  if (target != null) findGroup(before, target); // loud error on an unknown parent
  if (wouldCycle(before, groupId, target)) {
    throw new Error(
      `reparentGroup would create a cycle: cannot move ${groupId} under ${
        target === groupId ? "itself" : `its own descendant ${target}`
      }`,
    );
  }

  const hasIndex = index !== undefined && index !== null && Number.isFinite(Number(index));

  // The moved group's new parentId (root => drop the field).
  const withParent = (group) => {
    const next = { ...group };
    if (target == null) delete next.parentId;
    else next.parentId = target;
    return next;
  };

  let nextGroups;
  let nextElements = before.elements || [];
  if (hasIndex) {
    // Explicit placement: contiguous order 0..N over the destination's new arrangement
    // (destination merged siblings BEFORE the move, excluding the group itself).
    const destSiblings = orderedChildren(before, target).filter((node) => node.id !== groupId);
    const clampedIndex = Math.max(0, Math.min(destSiblings.length, Math.round(Number(index))));
    const arranged = destSiblings.slice();
    arranged.splice(clampedIndex, 0, { kind: "group", id: groupId });
    const orderByNodeId = new Map(arranged.map((node, order) => [node.id, order]));
    nextGroups = groupsOf(before).map((group) => {
      if (group.id === groupId) return { ...withParent(group), order: orderByNodeId.get(groupId) };
      return orderByNodeId.has(group.id) ? { ...group, order: orderByNodeId.get(group.id) } : group;
    });
    nextElements = (before.elements || []).map((element) =>
      orderByNodeId.has(element.id) ? { ...element, order: orderByNodeId.get(element.id) } : element,
    );
  } else {
    const fo = frontOrder(before, target);
    nextGroups = groupsOf(before).map((group) => {
      if (group.id !== groupId) return group;
      const moved = withParent(group);
      if (fo === null) delete moved.order;
      else moved.order = fo;
      return moved;
    });
  }

  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "reparentGroup",
    args_summary: { groupId, parentId: target, index: hasIndex ? Math.round(Number(index)) : undefined },
    before,
    after,
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Dissolve ONE group level in ONE journaled gesture (Figma Ungroup): the group's DIRECT
// children — elements AND direct subgroups — move up into the group's OWN parent scope
// (root when the group was top-level, preserving nesting depth otherwise), landing AT the
// group's former sibling z-slot in their internal relative order, and the now-empty group
// is removed. The parent scope is rewritten with contiguous order (the children occupy the
// vacated slot, everything else keeps its relative order), so z-order is exact — not the
// old page-composed "children jump to the front". Grandchildren keep pointing at the
// surviving subgroups (only one level dissolves). One commitMutation; a single undo
// restores the group and every child's scope + order exactly. Backing image files stay on
// disk. A loud error on an unknown group; an empty group simply dissolves (its slot closes).
export function ungroupGroup(root, { projectId, groupId } = {}) {
  if (!projectId) throw new Error("ungroupGroup requires projectId");
  if (!groupId) throw new Error("ungroupGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const group = findGroup(before, groupId); // loud error on an unknown group
  // The group's resolved parent scope (null = root; a dangling parent resolves to root,
  // mirroring tree scope resolution) — where the children land.
  const scope = nodeScope(before, group);

  // Parent-scope arrangement with the group node REPLACED by its direct children (kept in
  // their own internal back->front order), so the children take the group's exact z-slot.
  const parentSiblings = orderedChildren(before, scope);
  const children = orderedChildren(before, groupId);
  const slot = parentSiblings.findIndex((node) => node.id === groupId);
  const arranged = parentSiblings.slice();
  arranged.splice(slot, 1, ...children);
  const orderByNodeId = new Map(arranged.map((node, order) => [node.id, order]));

  const childElementIds = new Set(children.filter((node) => node.kind === "element").map((node) => node.id));
  const childGroupIds = new Set(children.filter((node) => node.kind === "group").map((node) => node.id));

  const nextGroups = groupsOf(before)
    .filter((item) => item.id !== groupId) // remove the dissolved group
    .map((item) => {
      if (!childGroupIds.has(item.id) && !orderByNodeId.has(item.id)) return item;
      const next = { ...item };
      if (childGroupIds.has(item.id)) {
        if (scope == null) delete next.parentId;
        else next.parentId = scope;
      }
      if (orderByNodeId.has(item.id)) next.order = orderByNodeId.get(item.id);
      return next;
    });
  const nextElements = (before.elements || []).map((item) => {
    if (!childElementIds.has(item.id) && !orderByNodeId.has(item.id)) return item;
    const next = { ...item };
    if (childElementIds.has(item.id)) {
      if (scope == null) delete next.groupId;
      else next.groupId = scope;
    }
    if (orderByNodeId.has(item.id)) next.order = orderByNodeId.get(item.id);
    return next;
  });

  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "ungroupGroup",
    args_summary: {
      groupId,
      parentId: scope,
      movedElements: [...childElementIds],
      movedGroups: [...childGroupIds],
    },
    before,
    after,
    startedAt,
  });
  return {
    project,
    ungrouped: groupId,
    parentId: scope,
    movedElements: [...childElementIds],
    movedGroups: [...childGroupIds],
  };
}

// ---- clipboard: paste / duplicate / delete nodes (T0227) ---------------------
//
// Figma-like copy/paste/duplicate for canvas objects (elements AND groups, mixed OK) and
// a batched mixed delete. The COPY BUFFER is page view-state (never journaled — see the
// site); the journaled gesture is the PASTE/DUPLICATE/DELETE op here. Each is ONE
// commitMutation (one undo). Ids are minted server-side; specs are validated LOUDLY
// before any write (unknown file ref / malformed node throws atomically).

// Instantiate a node spec (tree.buildNodesSpec shape) into the CURRENT scope as ONE
// journaled gesture — the page's Ctrl+V and the CLI nodes-paste. Every node gets a FRESH
// id; the internal structure (nesting) and relative back->front order are preserved, and
// the whole paste is shifted by (dx, dy). `scopeId` (null/absent = root) is the
// destination group (the page's enteredGroupId). Loud + atomic: the spec is fully
// validated (structure + every image `src` must resolve in this project's immutable
// files/) BEFORE any id is minted or written; an unknown scope, non-finite offset, or an
// empty/malformed spec throws before any write. Top-level roots keep an explicitly-ordered
// destination scope explicit (front orders in spec order) and stay implicit otherwise
// (array-append order); nested pasted scopes are fresh, so their children get explicit
// contiguous order (exact internal z-order). Returns the new TOP-LEVEL node ids so the
// caller can select the pasted copy.
export function pasteNodes(root, { projectId, spec, dx, dy, scopeId } = {}) {
  if (!projectId) throw new Error("pasteNodes requires projectId");
  if (!spec || typeof spec !== "object" || Array.isArray(spec)) throw new Error("pasteNodes requires a spec object");
  const nodes = spec.nodes;
  if (!Array.isArray(nodes) || !nodes.length) throw new Error("pasteNodes spec has no nodes to paste");
  const offX = dx === undefined || dx === null || dx === "" ? 0 : Number(dx);
  const offY = dy === undefined || dy === null || dy === "" ? 0 : Number(dy);
  if (!Number.isFinite(offX) || !Number.isFinite(offY)) throw new Error("pasteNodes dx/dy must be finite numbers");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const scope = scopeId == null || scopeId === "" ? null : String(scopeId);
  if (scope != null) findGroup(before, scope); // loud on an unknown destination scope

  // Validate the WHOLE spec loudly before any mint/write (atomic): structure + every
  // image file ref must resolve in this project's immutable files/.
  const validateNode = (node, path) => {
    if (!node || typeof node !== "object") throw new Error(`paste node ${path} is not an object`);
    if (node.kind === "element") {
      const def = node.element;
      if (!def || typeof def !== "object") throw new Error(`paste node ${path} has no element def`);
      if (!Number.isFinite(Number(def.x)) || !Number.isFinite(Number(def.y))) {
        throw new Error(`paste node ${path} element has non-finite x/y`);
      }
      if ((def.type || "image") === "image") {
        if (!def.src || typeof def.src !== "string") throw new Error(`paste node ${path} image element has no src`);
        if (!existsSync(resolveProjectFile(root, projectId, def.src))) {
          throw new Error(`paste node ${path} references an unknown file: ${def.src}`);
        }
      }
    } else if (node.kind === "group") {
      if (!node.group || typeof node.group !== "object") throw new Error(`paste node ${path} has no group def`);
      const children = node.children || [];
      if (!Array.isArray(children)) throw new Error(`paste node ${path} group children must be an array`);
      children.forEach((child, index) => validateNode(child, `${path}.${index}`));
    } else {
      throw new Error(`paste node ${path} has unknown kind: ${JSON.stringify(node.kind)}`);
    }
  };
  nodes.forEach((node, index) => validateNode(node, String(index)));

  const newElements = [];
  const newGroups = [];
  const instantiate = (node, parentScope, orderVal) => {
    if (node.kind === "element") {
      const def = JSON.parse(JSON.stringify(node.element)); // isolate from the (page-held) spec
      const rec = { ...def, id: `el_${randomUUID().slice(0, 8)}`, x: Number(def.x) + offX, y: Number(def.y) + offY };
      delete rec.order;
      if (parentScope != null) rec.groupId = parentScope;
      else delete rec.groupId;
      if (orderVal != null) rec.order = orderVal;
      newElements.push(rec);
      return { kind: "element", id: rec.id };
    }
    const gdef = JSON.parse(JSON.stringify(node.group));
    const grec = { ...gdef, id: `grp_${randomUUID().slice(0, 8)}`, x: Number(gdef.x || 0) + offX, y: Number(gdef.y || 0) + offY };
    delete grec.parentId;
    delete grec.order;
    if (parentScope != null) grec.parentId = parentScope;
    if (orderVal != null) grec.order = orderVal;
    newGroups.push(grec);
    // A fresh pasted scope: assign contiguous 0..N-1 order so internal z-order is exact.
    (node.children || []).forEach((child, index) => instantiate(child, grec.id, index));
    return { kind: "group", id: grec.id };
  };

  let fo = frontOrder(before, scope); // null on a never-reordered (implicit) destination
  const roots = nodes.map((node) => instantiate(node, scope, fo === null ? null : fo++));

  const after = updateProject(root, projectId, {
    elements: [...(before.elements || []), ...newElements],
    groups: [...groupsOf(before), ...newGroups],
  });
  const project = commitMutation(root, projectId, {
    op: "pasteNodes",
    args_summary: {
      count: roots.length,
      scope,
      nodeIds: roots.map((node) => node.id),
      elements: newElements.length,
      groups: newGroups.length,
    },
    before,
    after,
    startedAt,
  });
  const elementIds = roots.filter((node) => node.kind === "element").map((node) => node.id);
  const groupIds = roots.filter((node) => node.kind === "group").map((node) => node.id);
  return { project, nodeIds: roots.map((node) => node.id), elementIds, groupIds, count: roots.length };
}

// Duplicate LIVE nodes in place (+offset) in ONE journaled gesture — the page's Ctrl+D and
// the CLI nodes-duplicate convenience. Builds the spec from the current project (pure
// tree.buildNodesSpec, throws on an unknown id) and delegates to pasteNodes (so it stays
// ONE op = one undo). Default offset +16,+16; default destination = the originals' COMMON
// scope (a duplicate lands beside its source), overridable by an explicit scopeId
// (including null for root).
export function duplicateNodes(root, { projectId, nodeIds, dx, dy, scopeId } = {}) {
  if (!projectId) throw new Error("duplicateNodes requires projectId");
  if (!Array.isArray(nodeIds) || !nodeIds.length) throw new Error("duplicateNodes requires a non-empty nodeIds array");
  const before = getProject(root, projectId);
  const spec = buildNodesSpec(before, nodeIds); // throws on an unknown id
  const offX = dx === undefined || dx === null || dx === "" ? 16 : Number(dx);
  const offY = dy === undefined || dy === null || dy === "" ? 16 : Number(dy);
  let scope = scopeId;
  if (scope === undefined) {
    const scopes = new Set(nodeIds.map((id) => {
      const node = findNode(before, id);
      return node.scopeId == null ? null : node.scopeId;
    }));
    scope = scopes.size === 1 ? [...scopes][0] : null;
  }
  return pasteNodes(root, { projectId, spec, dx: offX, dy: offY, scopeId: scope });
}

// Delete a MIXED set of nodes (loose elements AND whole group subtrees) in ONE journaled
// gesture — the page's Delete key on a multi-group or mixed selection, and the CLI
// nodes-delete. A group id deletes its FULL closure (nested subgroups AND every element in
// the subtree), de-duplicated against any directly-selected member. Loud + atomic: an
// unknown id throws before any write; an empty list is a loud error (the page only calls
// this for 2+ selections). One commitMutation, so a single undo deep-restores every group
// and element at its exact z-slot (the before/after snapshot). Backing image files stay in
// files/ (non-destructive storage).
export function deleteNodes(root, { projectId, nodeIds } = {}) {
  if (!projectId) throw new Error("deleteNodes requires projectId");
  if (!Array.isArray(nodeIds) || !nodeIds.length) throw new Error("deleteNodes requires a non-empty nodeIds array");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const ids = [...new Set(nodeIds.map((value) => String(value)))];
  const removedGroupIds = new Set();
  const removedElementIds = new Set();
  for (const id of ids) {
    const node = findNode(before, id); // throws on an unknown id (atomic)
    if (node.kind === "element") {
      removedElementIds.add(id);
    } else {
      removedGroupIds.add(id);
      const descendants = descendantsOf(before, id);
      for (const group of descendants.groups) removedGroupIds.add(group.id);
      for (const element of descendants.elements) removedElementIds.add(element.id);
    }
  }
  const nextGroups = groupsOf(before).filter((group) => !removedGroupIds.has(group.id));
  const nextElements = (before.elements || []).filter((element) => !removedElementIds.has(element.id));
  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "deleteNodes",
    args_summary: { nodeIds: ids, deletedGroups: [...removedGroupIds], deletedElements: [...removedElementIds] },
    before,
    after,
    startedAt,
  });
  return { project, removedGroups: [...removedGroupIds], removedElements: [...removedElementIds], nodeIds: ids };
}

// ---- undo / redo / history ---------------------------------------------------

// Agent-side concurrency guard (T0234, after the 2026-07-03 history-jump race: an
// agent read a project at head 823, the lead kept working live to head 876, and the
// agent's jump forked the spine and orphaned the lead's newest entries). undoOp/
// redoOp/jumpHistory accept an optional `expectHead` the caller reads BEFORE calling
// (from listHistory/history-list); when given, it must still match the ACTUAL current
// head or the call refuses LOUDLY before any write. `expectHead` may arrive as a
// string from the CLI, so it is coerced with Number() and validated as a finite
// integer here (a non-numeric value is its own loud error). Absent/null/"" means no
// guard was requested — the page path today, byte-identical to pre-T0234 behavior.
function checkExpectHead(expectHead, head) {
  if (expectHead === undefined || expectHead === null || expectHead === "") return;
  const expected = Number(expectHead);
  if (!Number.isFinite(expected) || !Number.isInteger(expected)) {
    throw new Error(`expectHead must be a finite integer, got ${JSON.stringify(expectHead)}`);
  }
  if (expected !== head) {
    throw new Error(
      `history advanced: head is now ${head}, you read ${expected} — the project is live; re-read history (history-list) and retry`,
    );
  }
}

export function undoOp(root, { projectId, expectHead } = {}) {
  if (!projectId) throw new Error("undoOp requires projectId");
  const startedAt = performance.now();
  ensureThinJournal(root, projectId); // migrating open is a mutating open
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
  checkExpectHead(expectHead, head);
  if (!head) throw new Error("nothing to undo");
  const entry = readJournal(root, projectId).find((item) => Number(item.seq) === head && isMutation(item));
  // The head entry can be absent once history has been compacted past this point
  // (its parent was rebased to 0), so undo bottoms out cleanly at the horizon.
  if (!entry) throw new Error("nothing to undo");
  const undoPatch = snapshotForEntry(root, projectId, entry).undo_patch || {};
  const restore = {
    elements: undoPatch.elements || [],
    groups: undoPatch.groups || [],
    tool_runs: undoPatch.tool_runs || [],
    history_seq: Number(entry.parent) || 0,
  };
  // Older snapshots predate title-in-snapshot; only restore title when present so
  // updateProject never clobbers the live title with undefined.
  if (undoPatch.title !== undefined) restore.title = undoPatch.title;
  const saved = updateProject(root, projectId, restore);
  appendJournal(root, projectId, { op: "undo", target_seq: head, duration_ms: ms(performance.now() - startedAt) });
  return { project: saved, undone_seq: head, history_seq: saved.history_seq };
}

export function redoOp(root, { projectId, expectHead } = {}) {
  if (!projectId) throw new Error("redoOp requires projectId");
  const startedAt = performance.now();
  ensureThinJournal(root, projectId);
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
  checkExpectHead(expectHead, head);
  const candidates = readJournal(root, projectId).filter((item) => isMutation(item) && (Number(item.parent) || 0) === head);
  if (!candidates.length) throw new Error("nothing to redo");
  const entry = candidates.reduce((best, item) => (Number(item.seq) > Number(best.seq) ? item : best));
  const state = snapshotForEntry(root, projectId, entry).state || {};
  const restore = {
    elements: state.elements || [],
    groups: state.groups || [],
    tool_runs: state.tool_runs || [],
    history_seq: Number(entry.seq),
  };
  if (state.title !== undefined) restore.title = state.title;
  const saved = updateProject(root, projectId, restore);
  appendJournal(root, projectId, { op: "redo", target_seq: entry.seq, duration_ms: ms(performance.now() - startedAt) });
  return { project: saved, redone_seq: entry.seq, history_seq: saved.history_seq };
}

// Compact journal view for `history <id>` / GET .../history: seq, timestamp, op,
// the small args_summary (or marker target), and duration_ms — never the big
// snapshots (thin lines carry no snapshot, so this is a cheap metadata scan). Back-
// compatible: canUndo/canRedo are computed from metadata only (no snapshot loads)
// and existing fields are unchanged; duration_ms is added, never removed/renamed.
// Undo/redo availability for a project from its history head + journal: canUndo iff
// the head entry (seq === history_seq) is a live mutation; canRedo iff any mutation's
// parent is the current head. Shared by readHistory (full view) and historyFlags (the
// lightweight summary folded into mutation responses).
function historyAvailability(project, journal) {
  const head = Number(project.history_seq) || 0;
  const canUndo = head > 0 && journal.some((item) => Number(item.seq) === head && isMutation(item));
  const canRedo = journal.some((item) => isMutation(item) && (Number(item.parent) || 0) === head);
  return { seq: head, canUndo, canRedo };
}

// The undo/redo summary { seq, canUndo, canRedo } for a project — the same flags
// readHistory computes, minus the (capped) entries list. Cheap on the thin journal,
// and folded into every mutating API response (api.mjs sendMutation) so the page
// updates state.history straight from the op result with ZERO extra /history GET.
export function historyFlags(root, { projectId } = {}) {
  if (!projectId) throw new Error("historyFlags requires projectId");
  return historyAvailability(getProject(root, projectId), readJournal(root, projectId));
}

export function readHistory(root, { projectId } = {}) {
  if (!projectId) throw new Error("readHistory requires projectId");
  const project = getProject(root, projectId);
  const journal = readJournal(root, projectId);
  const { seq: head, canUndo, canRedo } = historyAvailability(project, journal);
  const entries = journal.map((item) => ({
    seq: item.seq,
    at: item.at,
    op: item.op,
    ...(item.args_summary ? { args_summary: item.args_summary } : {}),
    ...(item.target_seq !== undefined ? { target_seq: item.target_seq } : {}),
    ...(item.duration_ms !== undefined ? { duration_ms: item.duration_ms } : {}),
  }));
  return { history_seq: head, canUndo, canRedo, entries };
}

// Per-op timing rollup for `ops-stats <id>` / GET .../ops-stats: from the thin
// journal, group by op and report count + median + p95 duration_ms; from
// errors.jsonl, the failure count (and a small tail of recent errors). A read-only
// observability op — no journal entry, no mutation.
export function opsStats(root, { projectId } = {}) {
  if (!projectId) throw new Error("opsStats requires projectId");
  if (!projectExists(root, projectId)) throw new Error(`canvas project not found: ${projectId}`);
  const journal = readJournal(root, projectId);
  const byOp = new Map();
  for (const line of journal) {
    const op = String(line.op || "");
    if (!byOp.has(op)) byOp.set(op, { op, count: 0, durations: [] });
    const bucket = byOp.get(op);
    bucket.count += 1;
    if (Number.isFinite(Number(line.duration_ms))) bucket.durations.push(Number(line.duration_ms));
  }
  const percentile = (sorted, q) => {
    if (!sorted.length) return null;
    const idx = Math.min(sorted.length - 1, Math.max(0, Math.ceil(q * sorted.length) - 1));
    return sorted[idx];
  };
  const ops = [...byOp.values()].map(({ op, count, durations }) => {
    const sorted = [...durations].sort((a, b) => a - b);
    const mid = Math.floor(sorted.length / 2);
    const median = sorted.length ? (sorted.length % 2 ? sorted[mid] : ms((sorted[mid - 1] + sorted[mid]) / 2)) : null;
    return { op, count, timed: sorted.length, median_ms: median, p95_ms: percentile(sorted, 0.95) };
  }).sort((a, b) => a.op.localeCompare(b.op));
  const errors = readErrors(root, projectId);
  return {
    projectId,
    total_entries: journal.length,
    ops,
    errors: { count: errors.length, recent: errors.slice(-5) },
  };
}

// ---- history panel view + jump navigation (T0204) ----------------------------
//
// The Photoshop-style history panel (page) and `history-list` (CLI) are a THIN view
// over the journal: the op layer computes the human LABEL/summary AND the current
// linear spine, so neither client parses the journal. jumpHistory is history
// NAVIGATION — it moves the applied head to any seq on that spine by restoring the
// seq's EXISTING sidecar snapshot. It composes with undo/redo: jumping N steps
// back/forward lands on exactly the state N undos/redos would (the same snapshots), in
// ONE call, appending only a nav marker (like the undo/redo markers, never a mutation).
// No parent pointer changes and no snapshot is written, so undo/redo/jump from the new
// head behave identically to N manual steps — the undo/redo chain stays coherent and a
// jump is itself reversible (redo/jump-forward after a back jump, and vice-versa).

// The label for a patchElement entry, derived from which fields its patch touched (the
// op records the clean patch in args_summary). Move/Resize/Transform/Rename/Show/Hide/
// Edit text — the frequent single-field edits get a precise verb.
function patchElementLabel(patch = {}) {
  const p = patch && typeof patch === "object" ? patch : {};
  const has = (key) => p[key] !== undefined;
  if (has("content") || has("style")) return "Edit text";
  if (has("visible")) return p.visible === false ? "Hide" : "Show";
  const geo = ["x", "y", "w", "h"].filter(has);
  if (geo.length) {
    const sized = has("w") || has("h");
    const moved = has("x") || has("y");
    if (sized && !moved) return "Resize";
    if (moved && !sized) return "Move";
    return "Transform";
  }
  if (has("name")) return "Rename";
  return "Edit element";
}

// Human { label, summary } for a journaled mutation, from its op name + the small
// args_summary the op recorded. PURE (no disk) and exported so BOTH clients render
// identical text off listHistory's output — the page never maps op names itself. An
// unknown op falls back to the raw op name (no silent blank row).
export function historyEntryLabel(op, args = {}) {
  const a = args && typeof args === "object" ? args : {};
  const count = (value) => (Number.isFinite(Number(value)) ? Number(value) : 0);
  const items = (value) => (Array.isArray(value) ? value.length : 0);
  const plural = (n, noun) => `${n} ${noun}${n === 1 ? "" : "s"}`;
  switch (op) {
    case "base": return { label: "Base", summary: "" };
    case "addImage": return { label: "Add image", summary: String(a.name || "") };
    case "addImages": return { label: "Add images", summary: plural(count(a.count), "image") };
    case "addText": return { label: "Add text", summary: String(a.content || "") };
    case "patchElement": return { label: patchElementLabel(a.patch), summary: "" };
    case "patchElements": return { label: "Move elements", summary: plural(count(a.count), "element") };
    case "removeElement": return { label: "Delete element", summary: "" };
    case "removeElements": return { label: "Delete elements", summary: plural(count(a.count), "element") };
    case "moveNodes": return { label: "Move", summary: plural(count(a.count), "item") };
    case "reorderNode": return { label: "Reorder", summary: String(a.kind || "") };
    case "reorderElement": return { label: "Reorder", summary: "" };
    case "reorderNodes": return { label: "Reorder", summary: plural(items(a.nodeIds), "item") };
    case "setRegions": return { label: "Edit regions", summary: a.region_count != null ? plural(count(a.region_count), "region") : "" };
    case "detectRegions": return { label: "Detect regions", summary: "" };
    case "sliceRegions": return { label: "Slice", summary: "" };
    case "alphaCutout": {
      if (a.count) return { label: "Alpha cutout", summary: plural(count(a.count), "image") };
      return { label: "Alpha cutout", summary: a.region_count ? plural(count(a.region_count), "region") : String(a.method || "") };
    }
    case "alphaDualPlate": return { label: "Dual-plate alpha", summary: "" };
    case "setExportSettings": return { label: "Export settings", summary: "" };
    case "patchProject": return { label: "Rename project", summary: String(a.title || "") };
    case "createGroup": return { label: "Group", summary: String(a.name || "") };
    case "patchGroup": return { label: "Edit group", summary: "" };
    case "patchGroups": return { label: "Edit groups", summary: plural(items(a.groupIds), "group") };
    case "deleteGroup": return { label: "Delete group", summary: "" };
    case "assignToGroup": return { label: "Move to group", summary: "" };
    case "fitGroup": return { label: "Fit group", summary: "" };
    case "reparentGroup": return { label: "Nest group", summary: "" };
    case "ungroupGroup": return { label: "Ungroup", summary: "" };
    case "pasteNodes": return { label: "Paste", summary: plural(count(a.count), "item") };
    case "duplicateNodes": return { label: "Duplicate", summary: plural(count(a.count), "item") };
    case "deleteNodes": return { label: "Delete", summary: plural(items(a.deletedElements) + items(a.deletedGroups), "item") };
    default: return { label: String(op || "Edit"), summary: "" };
  }
}

// The current LINEAR history spine (Photoshop-style): the undo chain from the applied
// head back to base, plus the redo chain forward from the head to the tip. Both mirror
// the exact walks undoOp/redoOp take — undoChain follows `parent` to 0; redoChain picks
// the greatest-seq child at each step (the branch redo would re-enter) — so a stale
// (invalidated) branch is never on the spine. `seen` guards a malformed parent cycle.
function historySpine(project, journal) {
  const head = Number(project.history_seq) || 0;
  const mutationsBySeq = new Map();
  for (const line of journal) if (isMutation(line)) mutationsBySeq.set(Number(line.seq), line);

  const undoChain = [];
  const seen = new Set();
  let cursor = head;
  while (cursor && mutationsBySeq.has(cursor) && !seen.has(cursor)) {
    seen.add(cursor);
    const entry = mutationsBySeq.get(cursor);
    undoChain.push(entry);
    cursor = Number(entry.parent) || 0;
  }
  undoChain.reverse(); // oldest → head (head is last)

  const redoChain = [];
  cursor = head;
  for (;;) {
    let best = null;
    for (const line of journal) {
      if (!isMutation(line)) continue;
      const seq = Number(line.seq);
      if (seen.has(seq)) continue;
      if ((Number(line.parent) || 0) !== cursor) continue;
      if (!best || seq > Number(best.seq)) best = line;
    }
    if (!best) break;
    seen.add(Number(best.seq));
    redoChain.push(best);
    cursor = Number(best.seq);
  }
  return { head, undoChain, redoChain };
}

// Labeled linear history for the panel / `history-list`: a synthetic `Base` (seq 0) row
// then the undo chain (applied) then the redo chain (undone — dimmed, still clickable).
// Each entry is { seq, op, label, summary, at, current, undone }. current marks the
// applied head; undone marks the redo-tail (future) states. canUndo/canRedo match
// historyFlags. A cheap metadata scan (no snapshot loads).
export function listHistory(root, { projectId } = {}) {
  if (!projectId) throw new Error("listHistory requires projectId");
  const project = getProject(root, projectId);
  const journal = readJournal(root, projectId);
  const { head, undoChain, redoChain } = historySpine(project, journal);
  const rowOf = (line, undone) => {
    const { label, summary } = historyEntryLabel(line.op, line.args_summary);
    // Agent-made entries carry the robot marker IN the label (lead 2026-07-03), so the
    // page palette and CLI history-list render identical text — parity by construction.
    const actor = line.actor === "agent" ? "agent" : "user";
    return {
      seq: Number(line.seq),
      op: line.op,
      actor,
      label: actor === "agent" ? `🤖 ${label}` : label,
      summary,
      at: line.at ?? null,
      current: Number(line.seq) === head,
      undone,
    };
  };
  const entries = [
    { seq: 0, op: "base", actor: "user", label: "Base", summary: "", at: null, current: head === 0, undone: false },
    ...undoChain.map((line) => rowOf(line, false)),
    ...redoChain.map((line) => rowOf(line, true)),
  ];
  // `head` duplicates `history_seq` under the same vocabulary the concurrency-guard
  // error message uses ("head is now N") — additive (T0234); history_seq is unchanged
  // and still the field both existing clients (page, API) read.
  return { history_seq: head, head, canUndo: head > 0 && undoChain.length > 0, canRedo: redoChain.length > 0, entries };
}

// Jump the applied head to `seq` — any seq on the current spine (0 = base, an undo-chain
// seq = jump back, a redo-chain seq = jump forward). Restores that seq's EXISTING sidecar
// snapshot (state for a real entry; the oldest retained entry's undo_patch for base) and
// repoints history_seq, so the result equals N undos/redos with ZERO recomputation. One
// call; appends only a `jump` nav marker (no snapshot, not a mutation) so undo/redo stay
// coherent and the jump is reversible. LOUD on a non-integer/negative seq or a seq that is
// not on the current spine (an unknown or stale-branch seq). No-op (no marker) when already
// there. Like undo/redo it never grows depth, so no compaction runs. Optional `expectHead`
// (T0234) is the caller's concurrency guard — see checkExpectHead above.
export function jumpHistory(root, { projectId, seq, expectHead } = {}) {
  if (!projectId) throw new Error("jumpHistory requires projectId");
  if (seq === undefined || seq === null || seq === "") throw new Error("jumpHistory requires a target seq");
  const target = Number(seq);
  if (!Number.isInteger(target) || target < 0) {
    throw new Error(`jumpHistory seq must be a non-negative integer, got ${JSON.stringify(seq)}`);
  }
  const startedAt = performance.now();
  ensureThinJournal(root, projectId); // a migrating open is a mutating open
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
  checkExpectHead(expectHead, head);
  if (target === head) return { project, history_seq: head, jumped_from: head, jumped_to: head }; // already here

  const journal = readJournal(root, projectId);
  const { undoChain, redoChain } = historySpine(project, journal);
  const back = undoChain.find((entry) => Number(entry.seq) === target);
  const forward = redoChain.find((entry) => Number(entry.seq) === target);
  if (target !== 0 && !back && !forward) {
    const reachable = [0, ...undoChain.map((entry) => Number(entry.seq)), ...redoChain.map((entry) => Number(entry.seq))];
    throw new Error(`jumpHistory seq ${target} is not on the current history (reachable seqs: ${reachable.join(", ")})`);
  }

  // Resolve the target project state from EXISTING snapshots only. A jump to base (0)
  // restores the oldest retained entry's BEFORE snapshot (undo_patch) — exactly where undo
  // bottoms out; any other seq restores its AFTER snapshot (state) — exactly where redo/undo
  // to that seq would land.
  let snap;
  if (target === 0) {
    snap = snapshotForEntry(root, projectId, undoChain[0]).undo_patch || {}; // head>0 here → non-empty chain
  } else {
    snap = snapshotForEntry(root, projectId, back || forward).state || {};
  }
  const restore = {
    elements: snap.elements || [],
    groups: snap.groups || [],
    tool_runs: snap.tool_runs || [],
    history_seq: target,
  };
  if (snap.title !== undefined) restore.title = snap.title;
  const saved = updateProject(root, projectId, restore);
  appendJournal(root, projectId, { op: "jump", target_seq: target, from_seq: head, duration_ms: ms(performance.now() - startedAt) });
  return { project: saved, history_seq: saved.history_seq, jumped_from: head, jumped_to: target };
}

// ---- detectRegions (bridged) -------------------------------------------------

// Meaningful numbered names for freshly detected regions: "<base> 1..N" (base =
// element name, else "Region"). Regions that already carry a name keep it.
export function nameDetectedRegions(regions, baseName) {
  const base = String(baseName || "").trim() || "Region";
  return (regions || []).map((region, index) =>
    region && !region.name ? { ...region, name: `${base} ${index + 1}` } : region,
  );
}

// Read the element's stored image, run it through the image tools upload +
// detect pipeline (imported unmodified from ../tools/image/{sources,regions}/api.mjs), then
// persist the detected regions on the element and record a tool_runs entry. One
// journal entry makes the detection undoable.
export async function detectRegions(root, { projectId, elementId, params = {} } = {}) {
  if (!projectId) throw new Error("detectRegions requires projectId");
  if (!elementId) throw new Error("detectRegions requires elementId");
  const startedAt = performance.now();
  const { buffer, fileName } = readElementBytes(root, projectId, elementId);
  const dims = imageSize(buffer);

  const dataUrl = `data:${mimeForExt(fileName)};base64,${buffer.toString("base64")}`;
  const uploaded = await uploadImageSource(root, { fileName, dataUrl });
  const detected = await detectImageRegions(root, {
    sourcePath: uploaded.sourcePath,
    options: params || {},
  });
  const rawRegions = Array.isArray(detected.regions && detected.regions.regions)
    ? detected.regions.regions
    : [];

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "detect_regions",
    elementId,
    at: new Date().toISOString(),
    params: params || {},
    result_summary: {
      region_count: rawRegions.length,
      session_id: detected.sessionId,
      background_mode: (detected.regions && detected.regions.mode) || "",
    },
  };

  // Re-read to avoid clobbering concurrent edits, snapshot before, then persist
  // regions (and backfill source dimensions) + the tool_runs entry atomically.
  const before = getProject(root, projectId);
  const target = (before.elements || []).find((item) => item.id === elementId);
  if (!target) {
    throw new Error(`element not found: ${elementId}`);
  }
  // Detected regions get meaningful numbered names ("<element name> 1..N") so
  // region rows read as content instead of raw sizes; sliced crops inherit them.
  const regions = nameDetectedRegions(rawRegions, target.name);
  const nextElements = (before.elements || []).map((item) =>
    item.id === elementId
      ? { ...item, source_w: item.source_w || dims.width, source_h: item.source_h || dims.height, regions }
      : item,
  );
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(before.tool_runs || []), run]),
  });
  const project = commitMutation(root, projectId, {
    op: "detectRegions",
    args_summary: { elementId, region_count: regions.length },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === elementId);
  return { project, element, run, regions };
}

// ---- sliceRegions (own crop tool) --------------------------------------------

// Slice an element's stored regions into new immutable image elements. Cropping is
// done by our OWN Python tool (tools/crop_regions.py, PIL): ops writes a crop spec
// (absolute source path + the element's regions with their exact rects) and spawns
// the script once. Each region is cropped from the element's own pixels by the
// STORED rect — verbatim, no re-detection — so user-moved, resized, and hand-drawn
// regions all crop exactly where they sit (unlike a detect-then-export bridge,
// which would key/normalize the pixels and re-derive geometry). Each crop becomes a
// content-addressed file + a new image element placed in a grid to the right of the
// parent, with provenance in meta.parent. The whole slice is one journal entry
// (undo removes every crop). detectRegions still uses the image tools bridge; only
// slice is ours. Per-region spec entries are objects carrying the rect and, for a
// polygonal region, its vertex ring (the crop tool then alpha-masks outside it);
// mixed rect + polygon sets stay one spawn.
export async function sliceRegions(root, { projectId, elementId, regionIds } = {}) {
  if (!projectId) throw new Error("sliceRegions requires projectId");
  if (!elementId) throw new Error("sliceRegions requires elementId");
  const startedAt = performance.now();
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

  const sourceAbs = resolveProjectFile(root, projectId, parent.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-slice-"));
  const created = [];
  try {
    const specPath = join(workDir, "crop_spec.json");
    const reportPath = join(workDir, "crop_report.json");
    const spec = {
      schema: "ai_studio.canvas.crop_regions_spec.v1",
      source: sourceAbs,
      output_dir: workDir,
      report: reportPath,
      // Objects (not bare rects): a polygonal region also carries its vertex ring, so
      // the crop tool masks alpha outside the polygon (bbox crop + ImageDraw.polygon).
      regions: selected.map((region) => {
        const rect = region.rect || region.content_bbox;
        if (!Array.isArray(rect) || rect.length !== 4) {
          throw new Error(`region ${region.id} has no rect to slice`);
        }
        const entry = { id: String(region.id), rect };
        if (Array.isArray(region.polygon) && region.polygon.length >= 3) entry.polygon = region.polygon;
        return entry;
      }),
    };
    writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
    await runToolPython(root, ["ai_studio/assets/canvas/tools/crop_regions.py", "--spec", specPath]);
    const report = JSON.parse(readFileSync(reportPath, "utf8"));
    const crops = (report && report.crops) || [];
    if (!crops.length) throw new Error("crop_regions produced no crops");

    // Place crops in a neat grid to the right of the parent (gap in source pixels).
    const gap = 16;
    const startX = parent.x + parent.w + gap;
    const columns = Math.max(1, Math.ceil(Math.sqrt(crops.length)));
    let col = 0;
    let cursorX = startX;
    let rowY = parent.y;
    let rowMaxH = 0;
    for (const crop of crops) {
      const bytes = readFileSync(join(workDir, crop.file));
      // Name the crop after the region's name when set (sanitized/trimmed), else
      // fall back to the <parent-name>#<region-id> provenance scheme.
      const region = selected.find((item) => String(item.id) === String(crop.id));
      const cropName = region && region.name ? String(region.name).trim() : "";
      const added = storeAddImage(root, projectId, {
        name: cropName || `${parent.name}#${crop.id}`,
        bytes,
        x: cursorX,
        y: rowY,
        meta: { parent: { elementId, regionId: crop.id, sheetSrc: parent.src } },
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
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }

  // Wrap every crop in a fresh "<sheet name> slices" group so a big slice never
  // dumps N loose elements onto the scene (lead 2026-07-02). Same journal entry:
  // one undo removes the group AND every crop together.
  const pad = 24;
  const { minX, minY, maxX, maxY } = elementsBBox(created);
  const group = {
    id: `grp_${randomUUID().slice(0, 8)}`,
    name: `${String(parent.name || "sheet").trim()} slices`,
    x: minX - pad,
    y: minY - pad,
    w: maxX - minX + pad * 2,
    h: maxY - minY + pad * 2,
    visible: true,
  };
  // Top-level slices group: front order keeps an explicitly-ordered root explicit
  // (no-op on a never-reordered root). The crops sit in the group's own fresh scope.
  const sliceGroupFront = frontOrder(before, null);
  if (sliceGroupFront !== null) group.order = sliceGroupFront;
  const createdIds = new Set(created.map((element) => element.id));

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "slice_regions",
    elementId,
    at: new Date().toISOString(),
    params: { regionIds: selected.map((region) => String(region.id)) },
    result_summary: { slice_count: created.length, group_id: group.id },
  };
  const current = getProject(root, projectId);
  const withRun = updateProject(root, projectId, {
    groups: [...groupsOf(current), group],
    elements: (current.elements || []).map((element) =>
      createdIds.has(element.id) ? { ...element, groupId: group.id } : element,
    ),
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), run]),
  });
  const project = commitMutation(root, projectId, {
    op: "slice",
    args_summary: {
      elementId,
      regionIds: selected.map((region) => String(region.id)),
      created: created.map((element) => element.id),
      count: created.length,
      groupId: group.id,
    },
    before,
    after: withRun,
    startedAt,
  });
  // Hand back the STORED crops (they now carry groupId), not the pre-group copies.
  const fresh = (project.elements || []).filter((element) => createdIds.has(element.id));
  return {
    project,
    created: created.map((element) => fresh.find((item) => item.id === element.id) || element),
    group: (project.groups || []).find((item) => item.id === group.id),
    run,
    regions: selected,
  };
}

// ---- alphaCutout (own alpha tool) --------------------------------------------

// Accepted alpha methods on the canvas: the auto route (soft_score router picks
// key_matte, and refuses a wide soft zone that would need a dual-plate pair) and an
// explicit key_matte force. alpha_dualplate needs a white+black plate PAIR — a single
// elementId call can't provide one, so asking for it here is a loud error that points
// at the separate alphaDualPlate op (T0237), which takes 2 elementIds instead.
const ALPHA_METHODS = new Set(["auto", "matte"]);

// Resolve the optional region-id selection against the element's STORED regions (same
// model as slice's regionIds), so moved/resized/hand-drawn regions key exactly where they
// sit. Each spec entry carries the rect and, for a polygonal region, its vertex ring.
// Returns null for "whole element" (no regions requested).
function resolveAlphaRegions(element, regions) {
  const requested = Array.isArray(regions) ? regions.map(String) : [];
  if (!requested.length) return null;
  const allRegions = Array.isArray(element.regions) ? element.regions : [];
  const wanted = new Set(requested);
  const selected = allRegions.filter((region) => wanted.has(String(region.id)));
  const found = new Set(selected.map((region) => String(region.id)));
  const missing = requested.filter((id) => !found.has(id));
  if (missing.length) throw new Error(`unknown region id(s): ${missing.join(", ")}`);
  const specRegions = selected.map((region) => {
    const rect = region.rect || region.content_bbox;
    if (!Array.isArray(rect) || rect.length !== 4) throw new Error(`region ${region.id} has no rect for alpha`);
    const entry = { id: String(region.id), rect };
    if (Array.isArray(region.polygon) && region.polygon.length >= 3) entry.polygon = region.polygon;
    return entry;
  });
  if (!specRegions.length) throw new Error("no regions selected for alpha");
  return specRegions;
}

// Run ONE element's CURRENT pixels through alpha_cutout.py (own worker spawn + own temp
// dir) and return the new content-addressed src + the tool's report, WITHOUT touching
// project.json — the caller (single or batch) commits. Shared so there is exactly one
// implementation of the actual keying step for both paths.
async function runAlphaCutoutTool(root, projectId, element, chosen, specRegions) {
  const sourceAbs = resolveProjectFile(root, projectId, element.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-alpha-"));
  try {
    const specPath = join(workDir, "alpha_spec.json");
    const reportPath = join(workDir, "alpha_report.json");
    const outPath = join(workDir, "alpha_out.png");
    const spec = {
      schema: "ai_studio.canvas.alpha_cutout_spec.v1",
      source: sourceAbs,
      output: outPath,
      report: reportPath,
      method: chosen,
    };
    if (specRegions) spec.regions = specRegions;
    writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
    await runToolPython(root, ["ai_studio/assets/canvas/tools/alpha_cutout.py", "--spec", specPath]);
    const report = JSON.parse(readFileSync(reportPath, "utf8"));
    const bytes = readFileSync(outPath);
    // Content-addressed write WITHOUT a new element: the caller swaps the element's src.
    const newSrc = storeAddFile(root, projectId, { bytes, name: `${slug(element.name)}.png` }).src;
    return { newSrc, report };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Build the tool_runs row + element.meta.alpha provenance for one keyed element (shared by
// the single and batch paths, so the recorded shape never drifts between them).
function buildAlphaProvenance(elementId, chosen, specRegions, report, parentSrc) {
  const at = new Date().toISOString();
  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "alpha_cutout",
    elementId,
    at,
    params: { method: chosen, regions: specRegions ? specRegions.map((region) => region.id) : [] },
    result_summary: {
      method: (report && report.method) || chosen,
      key_color: report && report.key_color,
      region_count: (report && report.region_count) || 0,
    },
  };
  const alphaMeta = {
    method: chosen,
    tool: "alpha_cutout.py",
    parentSrc,
    at,
    key_color: report && report.key_color,
    regions: run.params.regions,
    routing: (report && report.regions) || [],
  };
  return { run, alphaMeta };
}

// Single-element path (unchanged behavior/journal shape from before batching landed).
async function alphaCutoutSingle(root, projectId, before, elementId, chosen, regions, startedAt) {
  const element = (before.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} is not an image`);
  const specRegions = resolveAlphaRegions(element, regions);

  const { newSrc, report } = await runAlphaCutoutTool(root, projectId, element, chosen, specRegions);

  // Re-read to avoid clobbering concurrent edits, then swap src + record provenance on the
  // SAME element (previous src file stays in files/, so undo restores the exact bytes).
  const current = getProject(root, projectId);
  const target = (current.elements || []).find((item) => item.id === elementId);
  if (!target) throw new Error(`element not found: ${elementId}`);
  const { run, alphaMeta } = buildAlphaProvenance(elementId, chosen, specRegions, report, target.src);
  const nextElements = (current.elements || []).map((item) =>
    item.id === elementId ? { ...item, src: newSrc, meta: { ...(item.meta || {}), alpha: alphaMeta } } : item,
  );
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), run]),
  });
  const project = commitMutation(root, projectId, {
    op: "alphaCutout",
    args_summary: { elementId, method: chosen, regions: run.params.regions, region_count: run.params.regions.length },
    before,
    after,
    startedAt,
  });
  return { project, element: (project.elements || []).find((item) => item.id === elementId), run, method: chosen };
}

// Batch path — the multi-selection "Apply to N images" gesture. Every element is
// validated up front (exists, is an image; regions are NOT allowed here — regions stay
// single-element). Each element is then keyed SEQUENTIALLY through its own worker spawn;
// if ANY element fails (dual-plate refusal, not an image mid-run, tool error), the error
// propagates immediately and NOTHING is written to project.json — no partial swap, no
// journal entry (any files/ bytes already written for earlier-succeeding elements are
// inert content-addressed data, never referenced by any element, so nothing is
// "mutated" in the non-destructive sense). Only once EVERY element has keyed
// successfully does this commit ONE journal entry swapping every src + every
// element.meta.alpha — one undo restores every element byte-exact.
async function alphaCutoutBatch(root, projectId, before, elementIds, chosen, startedAt) {
  const ids = elementIds.map((value) => String(value));
  const unique = [...new Set(ids)];
  const elements = unique.map((elementId) => {
    const element = (before.elements || []).find((item) => item.id === elementId);
    if (!element) throw new Error(`element not found: ${elementId}`);
    if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} is not an image`);
    return element;
  });

  const processed = [];
  for (const element of elements) {
    const { newSrc, report } = await runAlphaCutoutTool(root, projectId, element, chosen, null);
    processed.push({ elementId: element.id, newSrc, report });
  }

  // Re-read once (defensive against a concurrent edit across the whole sequential run),
  // then swap every src + write every element's meta.alpha off the SAME snapshot.
  const current = getProject(root, projectId);
  const runs = [];
  const swapById = new Map();
  for (const item of processed) {
    const target = (current.elements || []).find((el) => el.id === item.elementId);
    if (!target) throw new Error(`element not found: ${item.elementId}`);
    const { run, alphaMeta } = buildAlphaProvenance(item.elementId, chosen, null, item.report, target.src);
    runs.push(run);
    swapById.set(item.elementId, { newSrc: item.newSrc, alphaMeta });
  }
  const nextElements = (current.elements || []).map((item) => {
    const swap = swapById.get(item.id);
    if (!swap) return item;
    return { ...item, src: swap.newSrc, meta: { ...(item.meta || {}), alpha: swap.alphaMeta } };
  });
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), ...runs]),
  });
  const project = commitMutation(root, projectId, {
    op: "alphaCutout",
    args_summary: { elementIds: unique, count: unique.length, method: chosen },
    before,
    after,
    startedAt,
  });
  const resultIds = new Set(unique);
  return {
    project,
    elements: (project.elements || []).filter((item) => resultIds.has(item.id)),
    runs,
    method: chosen,
    count: unique.length,
  };
}

// Run the element's CURRENT pixels through the image-tools alpha pipeline and swap the
// element to a NEW content-addressed alpha PNG in ONE journaled entry — the missing bridge
// that puts the matte pipeline on the canvas ("готовый арт сразу в альфу"). Cutting is done
// by our OWN Python tool (tools/alpha_cutout.py) which REUSES the image-tools route +
// key_matte modules unmodified (no matte logic duplicated in node or a second python impl);
// ops writes an alpha spec (absolute source path + method + the element's selected regions
// with their exact rects) and spawns it once through the shared warm worker. `method` is
// "auto" (route) or "matte" (force key_matte); a wide soft zone under "auto" is a loud error
// (dual-plate pair needed — no silent single-plate fallback), as is a non-image element or an
// unknown method. `regions` is an optional list of the element's stored region ids: given, the
// alpha is applied ONLY inside those region masks and the rest is untouched (region-mask
// composition happens IN python, one worker call); omitted, the whole element is keyed. Output
// dimensions equal the source, so geometry never changes. The previous src file stays in
// files/ (immutable), so undo restores the exact previous bytes; element.meta.alpha records the
// run (method, params, parent src, routing metrics) like slice provenance, plus a tool_runs row.
// `elementIds` (2+ images), given INSTEAD of `elementId`, batches a multi-selection into ONE
// journal entry (T0230) — see alphaCutoutBatch; `regions` is not accepted with a batch.
export async function alphaCutout(root, { projectId, elementId, elementIds, method, regions } = {}) {
  if (!projectId) throw new Error("alphaCutout requires projectId");
  const batch = elementIds !== undefined && elementIds !== null;
  if (batch && elementId != null) {
    throw new Error("alphaCutout accepts either elementId or elementIds, not both");
  }
  if (!batch && !elementId) throw new Error("alphaCutout requires elementId");
  if (batch) {
    // Structural batch checks are cheap and belong before any disk read, mirroring the
    // requires-elementId guard above (fail fast on bad shape, not on a missing project).
    if (!Array.isArray(elementIds) || !elementIds.length) {
      throw new Error("alphaCutout requires a non-empty elementIds array");
    }
    if (regions != null) {
      throw new Error(
        "alphaCutout batch (elementIds) does not support regions — regions stay single-element (pass a single elementId)",
      );
    }
  }
  const chosen = method == null || method === "" ? "auto" : String(method).trim().toLowerCase();
  if (!ALPHA_METHODS.has(chosen)) {
    if (chosen === "dualplate" || chosen === "dual_plate" || chosen === "generation") {
      throw new Error(
        `alpha method ${JSON.stringify(method)} needs a white+black plate PAIR (dual-plate) — a single ` +
          `elementId call can't provide one. Select BOTH plate elements and use the alphaDualPlate op ` +
          `(API POST /alpha-dual, CLI alpha-dual) instead, or use method "auto"/"matte" on a single image.`,
      );
    }
    throw new Error(`unknown alpha method ${JSON.stringify(method)} (expected "auto" or "matte")`);
  }
  const startedAt = performance.now();
  const before = getProject(root, projectId);

  if (batch) {
    return alphaCutoutBatch(root, projectId, before, elementIds, chosen, startedAt);
  }
  return alphaCutoutSingle(root, projectId, before, elementId, chosen, regions, startedAt);
}

// ---- alphaDualPlate (white+black plate pair -> one new cut element) ----------
//
// T0237 (lead, 2026-07-03): "до сих пор нет дуал пути для альфы?" — closes the loop the
// alphaCutout doc points at ("a pair source could come from later"). Runs the canvas's
// own tools/alpha_dualplate.py through the shared warm worker, which REUSES the
// image-tools dual_plate_alpha + dual_plate_pair_gate modules unmodified (no matte logic
// duplicated in node or a second python impl).

// Run the two plate elements' CURRENT pixels through alpha_dualplate.py (own worker spawn
// + own temp dir) and return the extracted RGBA bytes + the tool's report, WITHOUT
// touching project.json — the caller mints the new element and commits.
async function runAlphaDualPlateTool(root, projectId, elementA, elementB) {
  const plateAAbs = resolveProjectFile(root, projectId, elementA.src);
  const plateBAbs = resolveProjectFile(root, projectId, elementB.src);
  const workDir = mkdtempSync(join(tmpdir(), "canvas-dualplate-"));
  try {
    const specPath = join(workDir, "dualplate_spec.json");
    const reportPath = join(workDir, "dualplate_report.json");
    const outPath = join(workDir, "dualplate_out.png");
    const spec = {
      schema: "ai_studio.canvas.alpha_dualplate_spec.v1",
      plateA: plateAAbs,
      plateB: plateBAbs,
      output: outPath,
      report: reportPath,
    };
    writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
    await runToolPython(root, ["ai_studio/assets/canvas/tools/alpha_dualplate.py", "--spec", specPath]);
    const report = JSON.parse(readFileSync(reportPath, "utf8"));
    const bytes = readFileSync(outPath);
    return { bytes, report };
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}

// Build the tool_runs row + the new element's meta.alpha provenance from the tool's
// report: method "dual_plate", both parent srcs, and the pair gate's own metrics (so the
// lead can see exactly how clean the pair was) — mirrors buildAlphaProvenance's shape.
function buildDualPlateProvenance(elementIdA, elementIdB, report, srcA, srcB) {
  const at = new Date().toISOString();
  const gate = (report && report.pair_gate) || {};
  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "alpha_dualplate",
    elementIds: [elementIdA, elementIdB],
    at,
    params: {},
    result_summary: {
      method: "dual_plate",
      light_plate: report && report.light_plate,
      dark_plate: report && report.dark_plate,
      pair_gate: gate,
      visible_pixels: report && report.visible_pixels,
    },
  };
  const alphaMeta = {
    method: "dual_plate",
    tool: "alpha_dualplate.py",
    parents: [srcA, srcB],
    at,
    light_plate: report && report.light_plate,
    dark_plate: report && report.dark_plate,
    pair_gate: gate,
  };
  return { run, alphaMeta };
}

// TWO selected image elements (the SAME art rendered on a white plate and a black plate,
// in either order — the tool auto-detects which is which by overall brightness) -> ONE NEW
// content-addressed cut element in ONE journaled entry. Both plate elements stay UNTOUCHED
// (non-destructive; the lead deletes them himself once happy with the result) — unlike
// alphaCutout, this never swaps an existing element's src. The new element is named
// "<first plate's name> alpha", placed at the first plate's x/y, sized to the extracted
// output (equals the plate size), and carries element.meta.alpha provenance (method,
// both parent srcs, the pair gate's verdict/metrics) plus an alpha_dualplate tool_runs
// row. Refusals are loud and specific: not exactly 2 ids, a non-image element, or the
// pair gate's own "regenerate" message (misaligned/redrawn plates, ambiguous roles) —
// travels the python worker's SystemExit path as a clean message, no traceback.
export async function alphaDualPlate(root, { projectId, elementIds } = {}) {
  if (!projectId) throw new Error("alphaDualPlate requires projectId");
  if (!Array.isArray(elementIds)) throw new Error("alphaDualPlate requires an elementIds array");
  const ids = elementIds.map((value) => String(value));
  if (ids.length !== 2) {
    throw new Error(`alphaDualPlate requires exactly 2 elementIds (a white-plate + black-plate pair), got ${ids.length}`);
  }
  const [idA, idB] = ids;
  if (idA === idB) throw new Error("alphaDualPlate requires two DIFFERENT elementIds (a white-plate + black-plate pair)");

  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const elementA = (before.elements || []).find((item) => item.id === idA);
  if (!elementA) throw new Error(`element not found: ${idA}`);
  if (elementA.type !== "image" || !elementA.src) throw new Error(`element ${idA} is not an image`);
  const elementB = (before.elements || []).find((item) => item.id === idB);
  if (!elementB) throw new Error(`element not found: ${idB}`);
  if (elementB.type !== "image" || !elementB.src) throw new Error(`element ${idB} is not an image`);

  const { bytes, report } = await runAlphaDualPlateTool(root, projectId, elementA, elementB);

  // Re-read to avoid clobbering a concurrent edit (mirrors alphaCutout's re-read-before-write).
  const current = getProject(root, projectId);
  const plateA = (current.elements || []).find((item) => item.id === idA);
  if (!plateA) throw new Error(`element not found: ${idA}`);
  const plateB = (current.elements || []).find((item) => item.id === idB);
  if (!plateB) throw new Error(`element not found: ${idB}`);

  const { run, alphaMeta } = buildDualPlateProvenance(idA, idB, report, plateA.src, plateB.src);
  // storeAddImage mints the new element (id/type/src/x/y/w/h/source_w/h/name/meta) the
  // SAME way every other add does — no hand-rolled element shape here. Like addImage, a
  // freshly minted image never carries a groupId, so the new element lands in the root
  // scope regardless of the plates' own group membership.
  const added = storeAddImage(root, projectId, {
    name: `${plateA.name} alpha`,
    bytes,
    x: plateA.x,
    y: plateA.y,
    meta: { alpha: alphaMeta },
  });

  // Front-order hook (identical to addImage/addImages): the new element lands at the
  // FRONT of the root scope when it is already explicitly ordered; a no-op otherwise.
  const fo = frontOrder(before, null);
  const nextElements = (added.project.elements || []).map((element) =>
    fo !== null && element.id === added.element.id ? { ...element, order: fo } : element,
  );
  const after = updateProject(root, projectId, {
    elements: nextElements,
    tool_runs: capToolRuns(root, projectId, [...(current.tool_runs || []), run]),
  });

  const project = commitMutation(root, projectId, {
    op: "alphaDualPlate",
    args_summary: { elementIds: [idA, idB], newElementId: added.element.id },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === added.element.id) || added.element;
  return { project, element, run };
}

// ---- exportElements (scale + encode) -----------------------------------------

// The source image's on-disk format (png/jpg/webp/gif) from its element.src ext,
// and the output file extension for an export format.
function sourceFormat(element) {
  const ext = extname(String(element.src || "")).toLowerCase().replace(/^\./, "");
  return ext === "jpeg" ? "jpg" : ext || "png";
}
function formatExt(format) {
  return format; // png -> png, jpg -> jpg, webp -> webp (already normalized)
}

// Figma-style scale marker for an export file name (T0229 replaces the manual suffix;
// T0235 adds the "-canvas" base tag). A 1x SOURCE-base multiplier is the baseline -> no
// marker (clean "name.png"); any other scale gets "@<token>" (e.g. "@2x", "@0.5x",
// "@512w"). A CANVAS-base row always gets a marker, even at 1x ("@1x-canvas"), because
// the source-base 1x row already claims the unmarked baseline name — a canvas-base row
// must never collide with it. Only applied when an element has SEVERAL rows (a single
// row is always the clean base name); the tokens are filename-safe (digits, "x"/"w"/"h",
// "." and "@"), so they never escape the confined export folder.
function scaleMarker(scaleToken, base) {
  const spec = parseScaleSpec(scaleToken);
  const isCanvas = base === "canvas";
  if (spec.kind === "mul" && spec.value === 1 && !isCanvas) return "";
  return isCanvas ? `@${spec.token}-canvas` : `@${spec.token}`;
}

// The rows an element exports with: its persisted export settings, an explicit
// override applied to every element (CLI ad-hoc / one-off), or the default single
// 1x-png row when the layer has no settings — matching Figma's implicit 1x.
function rowsForElement(element, overrideRows) {
  if (overrideRows) return overrideRows;
  if (Array.isArray(element.export) && element.export.length) return cleanExportRows(element.export);
  return [DEFAULT_EXPORT_ROW];
}

// The element's on-canvas size (Math.round(w)/Math.round(h)) for a "canvas"-base export
// row (T0235) — the CURRENT size on the canvas at export time, not frozen into the row's
// scale token, so a later resize is picked up automatically. Loud when either dimension
// is missing or rounds to zero: a canvas-base row can't resolve without real geometry.
function canvasDimsFor(element) {
  const w = Math.round(Number(element.w));
  const h = Math.round(Number(element.h));
  if (!(w > 0) || !(h > 0)) {
    throw new Error(
      `element ${element.id} (${element.name || element.id}) has no on-canvas size for a "canvas" base export row (w=${element.w}, h=${element.h})`,
    );
  }
  return { w, h };
}

// Export selected elements to <project>/export/<utc-stamp>/, one file per element x
// export row, plus a manifest.json. Each row scales (resolveExportScale) + encodes
// (png/jpg/webp with quality/resample) via ONE Python spawn for the whole batch
// (tools/export_images.py, spec-file pattern). A 1x-png export of a png source is a
// byte-identical file COPY done in Node (no re-encode, no spawn) so the lead's
// original pixels are preserved exactly. Export makes no project mutation, so it is
// NOT journaled/undoable; it only records a tool_runs entry. `rows`, when given,
// overrides every element's settings for this one run (agent one-shots / the CLI's
// inline --scale/--format flags); omit it to honor each element's persisted rows.
// Each row's base (T0235, default "source") picks which dims its scale token resolves
// against: "source" -> the element's original source_w/h (unchanged v1 behavior);
// "canvas" -> the element's CURRENT on-canvas w/h (canvasDimsFor).
export async function exportElements(root, { projectId, elementIds, rows } = {}) {
  if (!projectId) throw new Error("exportElements requires projectId");
  const project = getProject(root, projectId);
  const ids = Array.isArray(elementIds) ? elementIds.map(String) : [];
  if (!ids.length) throw new Error("exportElements requires elementIds");
  const overrideRows = rows === undefined || rows === null ? null : cleanExportRows(rows);

  const elements = [];
  for (const id of ids) {
    const element = (project.elements || []).find((item) => String(item.id) === id);
    if (!element) throw new Error(`element not found: ${id}`);
    if (element.type === "text") {
      // Standalone per-element text-PNG export is a v1.1 feature (see T0222). Text bakes
      // into a screen today: put it in a group and export the screen (renderGroup) or run
      // a project export, which composites text with PIL through render_group.py.
      throw new Error(
        `element ${id} is a text element — standalone text export is not in v1. Put it in a group and export the screen (Render group / project export) to bake the text into the PNG.`,
      );
    }
    if (element.type !== "image" || !element.src) throw new Error(`element ${id} is not an exportable image`);
    elements.push(element);
  }

  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const folder = resolveProjectPath(root, projectId, "export", stamp);
  const used = new Set();
  const items = [];
  const copyJobs = []; // Node byte-copies (png -> png, no resize): verbatim originals.
  const encodeJobs = []; // one batched Python spawn scales + encodes the rest.

  for (const element of elements) {
    const elementRows = rowsForElement(element, overrideRows);
    const srcAbs = resolveProjectFile(root, projectId, element.src);
    const srcFmt = sourceFormat(element);
    const sourceW = Number(element.source_w) || Number(element.w) || 0;
    const sourceH = Number(element.source_h) || Number(element.h) || 0;
    const base = slug(element.name || element.id);
    // Automatic naming (T0229): a single row is the clean base name; several rows get a
    // Figma scale marker ("name@2x.png") so they never overwrite each other. Any name
    // that still collides (same scale+format twice, or two elements sharing a name) is
    // disambiguated deterministically with a numeric "_NN" against the run-wide `used`.
    const multiRow = elementRows.length > 1;

    for (const row of elementRows) {
      const baseDims = row.base === "canvas" ? canvasDimsFor(element) : { w: sourceW, h: sourceH };
      const { width, height } = resolveExportScale(row.scale, baseDims.w, baseDims.h);
      const marker = multiRow ? scaleMarker(row.scale, row.base) : "";
      let file = `${base}${marker}.${formatExt(row.format)}`;
      let counter = 2;
      while (used.has(file)) {
        file = `${base}${marker}_${String(counter).padStart(2, "0")}.${formatExt(row.format)}`;
        counter += 1;
      }
      used.add(file);
      const outAbs = resolveProjectPath(root, projectId, "export", stamp, file);
      // needsResize/pureCopy compare against the SOURCE FILE's actual pixels (sourceW/H,
      // not baseDims) — that decides whether the on-disk bytes can be copied verbatim,
      // regardless of which base the row's target dims were resolved against.
      const needsResize = width !== sourceW || height !== sourceH;
      const pureCopy = !needsResize && row.format === "png" && srcFmt === "png";

      const item = {
        elementId: element.id,
        name: element.name || element.id,
        file,
        src: element.src,
        scale: row.scale,
        base: row.base === "canvas" ? "canvas" : "source",
        format: row.format,
        resample: row.resample,
        w: width,
        h: height,
        meta: element.meta || {},
      };
      if (row.quality !== undefined) item.quality = row.quality;
      items.push(item);

      if (pureCopy) {
        copyJobs.push({ srcAbs, outAbs });
      } else {
        encodeJobs.push({
          src: srcAbs,
          out: outAbs,
          target_w: width,
          target_h: height,
          format: row.format,
          quality: row.quality === undefined ? null : row.quality,
          resample: row.resample,
        });
      }
    }
  }

  // Byte-identical copies never touch Python (offline + preserves the exact bytes).
  for (const job of copyJobs) writeProjectBytes(job.outAbs, readFileSync(job.srcAbs));

  // One Python spawn for the whole encode batch (spec-file pattern like slice). Uses
  // the config-only bridge interpreter, so a missing venv/PIL is a loud named error.
  if (encodeJobs.length) {
    const workDir = mkdtempSync(join(tmpdir(), "canvas-export-"));
    try {
      const specPath = join(workDir, "export_spec.json");
      const reportPath = join(workDir, "export_report.json");
      const spec = {
        schema: "ai_studio.canvas.export_images_spec.v1",
        report: reportPath,
        jobs: encodeJobs,
      };
      writeFileSync(specPath, `${JSON.stringify(spec, null, 2)}\n`);
      await runToolPython(root, ["ai_studio/assets/canvas/tools/export_images.py", "--spec", specPath]);
    } finally {
      rmSync(workDir, { recursive: true, force: true });
    }
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
    params: { elementIds: ids, rows: overrideRows ? "override" : "per-element" },
    result_summary: { item_count: items.length, encoded: encodeJobs.length, copied: copyJobs.length, folder },
  };
  updateProject(root, projectId, { tool_runs: capToolRuns(root, projectId, [...(getProject(root, projectId).tool_runs || []), run]) });
  return { folder, stamp, items, manifest, run };
}

// ---- zipExport (bundle a finished run into one .zip) -------------------------

// Bundle a finished export run's image files into ONE STORE-mode zip — the page's
// "several outputs -> one archive" save-dialog delivery (Figma behavior) and the CLI
// --zip flag, so both clients archive identically (tool parity). The run already
// materialized its files under the confined <project>/export/<stamp>/; this reads that
// run's manifest.json to learn the produced file names, reads each file, and hands them
// to the pure zip writer. STORE mode = no compression (PNG/JPG/WebP are already
// compressed). Loud on a bad/unknown stamp, a corrupt manifest, or a file gone missing
// (never a silent empty archive). Makes no project mutation.
export function zipExport(root, { projectId, stamp } = {}) {
  if (!projectId) throw new Error("zipExport requires projectId");
  if (!stamp) throw new Error("zipExport requires stamp");
  const manifestPath = resolveProjectPath(root, projectId, "export", stamp, "manifest.json");
  if (!existsSync(manifestPath)) throw new Error(`export run not found: ${stamp}`);
  let manifest;
  try {
    manifest = JSON.parse(readFileSync(manifestPath, "utf8").replace(/^﻿/, ""));
  } catch (error) {
    throw new Error(`export manifest is not valid JSON (${stamp}): ${error.message}`);
  }
  // Collect the run's output image files: a single-screen render carries manifest.file;
  // element/project runs list them under manifest.items[].file. De-duplicate, preserving
  // order, and archive nothing else (specs/reports/manifest stay out of the bundle).
  const files = [];
  if (manifest.file) files.push(manifest.file);
  for (const item of manifest.items || []) if (item && item.file) files.push(item.file);
  const seen = new Set();
  const entries = [];
  for (const file of files) {
    if (seen.has(file)) continue;
    seen.add(file);
    const abs = resolveProjectPath(root, projectId, "export", stamp, file); // confines each segment
    if (!existsSync(abs)) throw new Error(`export file missing for zip: ${file}`);
    entries.push({ name: file, data: readFileSync(abs) });
  }
  if (!entries.length) throw new Error(`export run ${stamp} has no files to zip`);
  return { bytes: zipStore(entries), files: entries.map((entry) => entry.name) };
}

// ---- renderGroup (screen compositing) ----------------------------------------
//
// Composite a group's VISIBLE member elements (element.visible !== false), in
// element array order (z-order), clipped to the group bounds, into ONE PNG at
// the requested scale over a transparent (or solid) background. The pixel work
// is done by our own Python tool (tools/render_group.py, PIL) because there is
// no dependency-free pure-Node compositor. This tool is OURS, so ops hands the full
// render spec to render_group.py through the shared bridge (runToolPython → warm
// worker), the same path detect/slice/export use; renderGroup makes no undoable
// geometry change, so like exportElements it is NOT journaled — it only records a
// render_group tool_runs entry.

function hexColor(value) {
  const text = String(value || "").trim();
  return /^#[0-9a-fA-F]{6}$/.test(text) ? text.toLowerCase() : null;
}

// Build the recursive z-ordered paint tree for a scope. Each VISIBLE child is either
// an element paint node (absolute box) or a group node (its background fill + its own
// recursively-built children painted inside the parent band). Hidden subtrees are
// pruned (isNodeHidden cascade). `leaves` accumulates every painted image element ref
// for the manifest + member count. `clip` rides along per group: render_group.py composites
// a clip:true subgroup's subtree onto its OWN box-sized layer (cropping overflow) before
// pasting it into the parent; a clip:false subgroup paints into the same layer at absolute
// offsets (overflow preserved).
function buildRenderNodes(root, projectId, project, scopeId, leaves, fonts) {
  const nodes = [];
  for (const child of orderedChildren(project, scopeId)) {
    if (isNodeHidden(project, child.ref)) continue;
    if (child.kind === "group") {
      const group = child.ref;
      const groupBg = group.background && group.background.type === "color" ? hexColor(group.background.color) : null;
      nodes.push({
        kind: "group",
        clip: group.clip === true,
        x: Number(group.x) || 0,
        y: Number(group.y) || 0,
        w: Number(group.w) || 0,
        h: Number(group.h) || 0,
        background: groupBg,
        children: buildRenderNodes(root, projectId, project, group.id, leaves, fonts),
      });
    } else {
      const element = child.ref;
      if (element.type === "text") {
        // A text node carries the ABSOLUTE font file (render_group.py loads the same
        // .ttf the page @font-faces) plus the split lines + style; the painter
        // re-measures each line for auto-width alignment, so no width is baked here.
        const style = element.style || defaultTextStyle();
        const entry = resolveFontEntry(fonts, {
          family: style.fontFamily,
          weight: style.fontWeight,
          style: style.fontStyle,
        });
        const stroke = style.stroke && Number(style.stroke.width) > 0
          ? { width: Number(style.stroke.width), color: style.stroke.color || "#000000" }
          : null;
        const shadow = style.shadow
          ? { dx: Number(style.shadow.dx) || 0, dy: Number(style.shadow.dy) || 0, color: style.shadow.color || "#000000" }
          : null;
        leaves.push(element);
        nodes.push({
          kind: "text",
          x: Number(element.x) || 0,
          y: Number(element.y) || 0,
          fontFile: resolveFontFileAbs(root, entry),
          fontSize: Number(style.fontSize) || 24,
          lineHeight: Number(style.lineHeight) || 1.2,
          align: style.align || "left",
          color: style.color || "#111111",
          lines: splitTextLines(element.content),
          stroke,
          shadow,
        });
        continue;
      }
      if (element.type !== "image" || !element.src) continue;
      leaves.push(element);
      nodes.push({
        kind: "element",
        src: resolveProjectFile(root, projectId, element.src),
        x: Number(element.x) || 0,
        y: Number(element.y) || 0,
        w: Number(element.w) || 0,
        h: Number(element.h) || 0,
      });
    }
  }
  return nodes;
}

// Composite one group's visible SUBTREE into a screen PNG at the given absolute
// output/spec/report paths (spawns render_group.py once). Shared by renderGroup
// (single screen, own folder) and exportProject (every visible top-level screen into
// one folder). Runs render_group.py through the shared bridge warm worker (runToolPython),
// the same config-only interpreter every canvas Python tool now uses.
async function compositeGroup(root, projectId, project, group, { scale, background, outputAbs, specAbs, reportAbs } = {}) {
  const renderScale = finite(scale) && Number(scale) > 0 ? Number(scale) : 1;
  // Precedence: an explicit render-time background arg OVERRIDES group.background;
  // else the group's own stored background; else transparent. render_group.py fills
  // this hex as the bottom layer, so the group background composites behind children.
  const explicit = background === undefined || background === null || background === "" ? null : hexColor(background);
  if (background && explicit === null) throw new Error(`background must be #rrggbb, got ${JSON.stringify(background)}`);
  const groupBg = group.background && group.background.type === "color" ? hexColor(group.background.color) : null;
  const bg = explicit || groupBg || null;

  // Recursive paint tree of the group's VISIBLE subtree, in COMPUTED z-order per scope
  // (nested subgroups included; each subgroup's background composites inside the parent
  // band). `members` is every painted image element (leaf) for the manifest + count.
  const members = [];
  // Load the fonts manifest once per composite so text nodes resolve to absolute .ttf
  // paths (a no-op file read when the group has no text).
  const fonts = readFontsManifest(root);
  const children = buildRenderNodes(root, projectId, project, group.id, members, fonts);
  const spec = {
    schema: "ai_studio.canvas.render_group_spec.v1",
    scale: renderScale,
    background: bg,
    group: { x: Number(group.x) || 0, y: Number(group.y) || 0, w: Number(group.w) || 0, h: Number(group.h) || 0 },
    output: outputAbs,
    report: reportAbs,
    children,
  };
  writeProjectBytes(specAbs, `${JSON.stringify(spec, null, 2)}\n`);
  await runToolPython(root, ["ai_studio/assets/canvas/tools/render_group.py", "--spec", specAbs]);

  let report = {};
  try {
    report = JSON.parse(readFileSync(reportAbs, "utf8"));
  } catch {
    // The PNG is the real product; a missing/foreign report is non-fatal.
  }
  const width = report.width || Math.max(1, Math.round((Number(group.w) || 0) * renderScale));
  const height = report.height || Math.max(1, Math.round((Number(group.h) || 0) * renderScale));
  return { renderScale, bg, members, width, height };
}

export async function renderGroup(root, { projectId, groupId, scale, background } = {}) {
  if (!projectId) throw new Error("renderGroup requires projectId");
  if (!groupId) throw new Error("renderGroup requires groupId");
  const project = getProject(root, projectId);
  const group = groupsOf(project).find((item) => item.id === groupId);
  if (!group) throw new Error(`group not found: ${groupId}`);

  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const fileName = `screen_${slug(group.name || group.id)}.png`;
  const folder = resolveProjectPath(root, projectId, "export", stamp);
  const outputAbs = resolveProjectPath(root, projectId, "export", stamp, fileName);

  const composited = await compositeGroup(root, projectId, project, group, {
    scale,
    background,
    outputAbs,
    specAbs: resolveProjectPath(root, projectId, "export", stamp, "render_spec.json"),
    reportAbs: resolveProjectPath(root, projectId, "export", stamp, "render_report.json"),
  });

  const manifest = {
    schema: "ai_studio.canvas.export.v1",
    kind: "screen",
    project: project.id,
    at: new Date().toISOString(),
    group: { id: group.id, name: group.name || group.id },
    scale: composited.renderScale,
    background: composited.bg,
    file: fileName,
    width: composited.width,
    height: composited.height,
    items: composited.members.map((element) => ({ elementId: element.id, name: element.name || element.id, src: element.src })),
  };
  writeProjectBytes(
    resolveProjectPath(root, projectId, "export", stamp, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
  );

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "render_group",
    at: new Date().toISOString(),
    params: { groupId, scale: composited.renderScale, background: composited.bg },
    result_summary: { file: fileName, folder, member_count: composited.members.length, width: manifest.width, height: manifest.height },
  };
  updateProject(root, projectId, { tool_runs: capToolRuns(root, projectId, [...(getProject(root, projectId).tool_runs || []), run]) });
  return { folder, file: fileName, path: outputAbs, manifest, run, members: composited.members.length };
}

// ---- exportProject (no selection -> export every screen) ---------------------

// Project-level export used when nothing is selected: render every visible TOP-LEVEL
// group (screen) at its own default 1x png into ONE <project>/export/<utc-stamp>/
// folder plus a combined manifest. A nested group is a component INSIDE its root
// screen (composited by compositeGroup's recursion), never a separate screen, so only
// parentId-less groups are exported here. Like the other export ops it makes no project
// mutation, so it is NOT journaled; it records one export_project tool_runs entry.
export async function exportProject(root, { projectId } = {}) {
  if (!projectId) throw new Error("exportProject requires projectId");
  const project = getProject(root, projectId);
  const visibleGroups = groupsOf(project).filter(
    (group) => group.parentId == null && group.visible !== false,
  );
  if (!visibleGroups.length) throw new Error("no visible screens to export (project export renders every visible top-level group)");

  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const folder = resolveProjectPath(root, projectId, "export", stamp);
  const usedFiles = new Set();
  const screens = [];
  for (const group of visibleGroups) {
    const base = `screen_${slug(group.name || group.id)}`;
    let fileName = `${base}.png`;
    let counter = 2;
    while (usedFiles.has(fileName)) {
      fileName = `${base}_${String(counter).padStart(2, "0")}.png`;
      counter += 1;
    }
    usedFiles.add(fileName);
    const stem = fileName.replace(/\.png$/, "");
    const composited = await compositeGroup(root, projectId, project, group, {
      scale: 1,
      background: null,
      outputAbs: resolveProjectPath(root, projectId, "export", stamp, fileName),
      specAbs: resolveProjectPath(root, projectId, "export", stamp, `${stem}.spec.json`),
      reportAbs: resolveProjectPath(root, projectId, "export", stamp, `${stem}.report.json`),
    });
    screens.push({
      groupId: group.id,
      name: group.name || group.id,
      file: fileName,
      w: composited.width,
      h: composited.height,
      members: composited.members.length,
    });
  }

  const manifest = {
    schema: "ai_studio.canvas.export.v1",
    kind: "project",
    project: project.id,
    at: new Date().toISOString(),
    items: screens,
  };
  writeProjectBytes(
    resolveProjectPath(root, projectId, "export", stamp, "manifest.json"),
    `${JSON.stringify(manifest, null, 2)}\n`,
  );

  const run = {
    id: `run_${randomUUID().slice(0, 8)}`,
    op: "export_project",
    at: new Date().toISOString(),
    params: { screenCount: screens.length },
    result_summary: { screen_count: screens.length, folder },
  };
  updateProject(root, projectId, { tool_runs: capToolRuns(root, projectId, [...(getProject(root, projectId).tool_runs || []), run]) });
  return { folder, stamp, screens, manifest, run };
}
