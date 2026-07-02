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
import { execFile } from "node:child_process";
import { randomUUID } from "node:crypto";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { extname, join } from "node:path";
import { performance } from "node:perf_hooks";
import { canvasHistoryDepth } from "../../core_harness/tool_lib/studio_config.mjs";
// The export encoder (tools/export_images.py) is a NEW tool, so it uses the T0218
// bridge's config-only Python resolution: interpreter from studio.config pythonPath
// ONLY, no candidate probing, and a loud error naming the setup command when the
// venv or a dependency (PIL) is missing. detect/slice/render keep the module's own
// legacy runPython discovery until the T0218 canvas seam flips them over.
import { runPython as runExportPython } from "../tools/image/_bridge/bridge.mjs";
import { detectImageRegions } from "../tools/image/regions/api.mjs";
import { uploadImageSource } from "../tools/image/sources/api.mjs";
import {
  addImage as storeAddImage,
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
  projectExists,
  readElementBytes,
  readErrors,
  readJournal,
  readSnapshot,
  removeElement as storeRemoveElement,
  resolveProjectFile,
  resolveProjectPath,
  rewriteJournal,
  updateProject,
  writeProjectBytes,
  writeSnapshot,
} from "./store.mjs";

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
  const project = commitMutation(root, projectId, {
    op: "addImage",
    args_summary: { name: result.element.name, elementId: result.element.id, w: result.element.w, h: result.element.h },
    before,
    after: result.project,
    startedAt,
  });
  return { project, element: result.element };
}

export function patchElement(root, projectId, elementId, patch = {}) {
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const result = storePatchElement(root, projectId, elementId, patch);
  const project = commitMutation(root, projectId, {
    op: "patchElement",
    args_summary: { elementId, patch },
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

// Move an element to a target index AMONG ITS SIBLINGS (same parent scope: the
// root-level ungrouped elements, or its group's members). Element array order IS
// the paint/z-order everywhere — the canvas paints elements() in array order and a
// group's render/export filters its members preserving that order — so this permutes
// only the sibling subsequence within the flat-array slots those siblings already
// occupy, leaving every other element's absolute position untouched. `index` is
// 0-based among siblings (0 = back / painted first, siblings.length-1 = front /
// painted last) and is clamped into range. One journal entry; undo restores the
// exact previous order. Groups (screens) keep their own array order — element
// z-order is the must-have here; reordering screens would be its own op.
export function reorderElement(root, { projectId, elementId, index } = {}) {
  if (!projectId) throw new Error("reorderElement requires projectId");
  if (!elementId) throw new Error("reorderElement requires elementId");
  if (!Number.isFinite(Number(index))) throw new Error("reorderElement requires a numeric index");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const list = Array.isArray(before.elements) ? before.elements : [];
  const moved = list.find((item) => item.id === elementId);
  if (!moved) throw new Error(`element not found: ${elementId}`);
  const scope = moved.groupId || null;
  const sameScope = (item) => (item.groupId || null) === scope;

  // Siblings in current paint order + the flat-array slots they occupy.
  const siblings = list.filter(sameScope);
  const slots = [];
  list.forEach((item, i) => {
    if (sameScope(item)) slots.push(i);
  });
  const from = siblings.findIndex((item) => item.id === elementId);
  const target = Math.max(0, Math.min(siblings.length - 1, Math.round(Number(index))));
  if (from === target) return { project: before, element: moved, index: target }; // no-op

  const nextSiblings = siblings.slice();
  nextSiblings.splice(from, 1);
  nextSiblings.splice(target, 0, moved);
  // Pour the reordered siblings back into their original slots; non-siblings stay put.
  const nextElements = list.slice();
  slots.forEach((slotIndex, i) => {
    nextElements[slotIndex] = nextSiblings[i];
  });

  const after = updateProject(root, projectId, { elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "reorderElement",
    args_summary: { elementId, index: target, scope },
    before,
    after,
    startedAt,
  });
  const element = (project.elements || []).find((item) => item.id === elementId);
  return { project, element, index: target };
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
// [{scale, suffix, format, quality?, resample}] — the Figma Export section persisted
// on the layer. setExportSettings validates + normalizes the rows and journals them
// like any metadata mutation (undo/redo restore the previous rows). The scale math
// (resolveExportScale) also runs at export time; validated here so an unknown
// scale/format/resample is a clear error the moment it is set, from either client.

const EXPORT_FORMATS = new Set(["png", "jpg", "webp"]);
const EXPORT_RESAMPLE = new Set(["lanczos", "nearest"]);
// Suffix is appended to the export filename (e.g. "@2x"); keep it filename-safe (no
// separators/traversal) so it can never escape the confined export folder.
const EXPORT_SUFFIX_RE = /^[A-Za-z0-9@._ -]*$/;
const MAX_EXPORT_DIM = 16384;
const DEFAULT_EXPORT_ROW = { scale: "1x", suffix: "", format: "png", resample: "lanczos" };

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

// Validate + normalize export rows to {scale, suffix, format, resample, quality?}.
// quality (1-100) is kept only for the lossy formats (jpg/webp), defaulting to 90;
// a png row never carries a quality. Throws on any invalid field.
function cleanExportRows(rows) {
  if (!Array.isArray(rows)) throw new Error("export rows must be an array");
  return rows.map((row, index) => {
    if (!row || typeof row !== "object") throw new Error(`export row ${index} is not an object`);
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
    const suffix = String(row.suffix == null ? "" : row.suffix).trim();
    if (!EXPORT_SUFFIX_RE.test(suffix) || suffix.includes("..")) {
      throw new Error(`export row ${index} suffix has unsafe characters: ${JSON.stringify(row.suffix)}`);
    }
    const clean = { scale, suffix, format, resample };
    if (format === "jpg" || format === "webp") {
      const raw = row.quality === undefined || row.quality === null || row.quality === "" ? 90 : Number(row.quality);
      if (!Number.isFinite(raw)) throw new Error(`export row ${index} quality must be a number 1-100`);
      clean.quality = Math.max(1, Math.min(100, Math.round(raw)));
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
  const clean = cleanExportRows(rows);
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
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  const groupId = `grp_${randomUUID().slice(0, 8)}`;
  const cleanName = String(name || "").trim() || "Group";

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
    startedAt,
  });
  return { project, group: (project.groups || []).find((item) => item.id === groupId) };
}

// Patch a group's name/bounds/visibility. When x or y change, translate ALL
// member elements by the same delta so the whole screen moves as one; resize
// (w/h) never moves members. One journal entry restores everything on undo.
export function patchGroup(root, { projectId, groupId, name, x, y, w, h, visible } = {}) {
  if (!projectId) throw new Error("patchGroup requires projectId");
  if (!groupId) throw new Error("patchGroup requires groupId");
  const startedAt = performance.now();
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
  const nextElements = (before.elements || []).map((element) =>
    idSet.has(element.id) ? { ...element, groupId: target } : element,
  );
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

// Remove a group. Members keep their positions and have groupId cleared. One
// journal entry.
// Deleting a group deletes its MEMBER ELEMENTS with it (lead 2026-07-02: a group
// is a container — dissolving one without deleting content is Ungroup, i.e.
// assignToGroup(null)). One journal entry; undo restores the group and every
// member. Member image files stay in files/ (non-destructive storage).
export function deleteGroup(root, { projectId, groupId } = {}) {
  if (!projectId) throw new Error("deleteGroup requires projectId");
  if (!groupId) throw new Error("deleteGroup requires groupId");
  const startedAt = performance.now();
  const before = getProject(root, projectId);
  findGroup(before, groupId);
  const nextGroups = groupsOf(before).filter((group) => group.id !== groupId);
  const removedElements = (before.elements || []).filter((element) => element.groupId === groupId);
  const nextElements = (before.elements || []).filter((element) => element.groupId !== groupId);
  const after = updateProject(root, projectId, { groups: nextGroups, elements: nextElements });
  const project = commitMutation(root, projectId, {
    op: "deleteGroup",
    args_summary: { groupId, deletedElements: removedElements.map((element) => element.id) },
    before,
    after,
    startedAt,
  });
  return { project, removed: groupId, removedElements: removedElements.map((element) => element.id) };
}

// ---- undo / redo / history ---------------------------------------------------

export function undoOp(root, { projectId } = {}) {
  if (!projectId) throw new Error("undoOp requires projectId");
  const startedAt = performance.now();
  ensureThinJournal(root, projectId); // migrating open is a mutating open
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
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

export function redoOp(root, { projectId } = {}) {
  if (!projectId) throw new Error("redoOp requires projectId");
  const startedAt = performance.now();
  ensureThinJournal(root, projectId);
  const project = getProject(root, projectId);
  const head = Number(project.history_seq) || 0;
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
export function readHistory(root, { projectId } = {}) {
  if (!projectId) throw new Error("readHistory requires projectId");
  const project = getProject(root, projectId);
  const journal = readJournal(root, projectId);
  const head = Number(project.history_seq) || 0;
  const canUndo = head > 0 && journal.some((item) => Number(item.seq) === head && isMutation(item));
  const canRedo = journal.some((item) => isMutation(item) && (Number(item.parent) || 0) === head);
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
    await runPython(root, ["ai_studio/assets/canvas/tools/crop_regions.py", "--spec", specPath]);
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

// The rows an element exports with: its persisted export settings, an explicit
// override applied to every element (CLI ad-hoc / one-off), or the default single
// 1x-png row when the layer has no settings — matching Figma's implicit 1x.
function rowsForElement(element, overrideRows) {
  if (overrideRows) return overrideRows;
  if (Array.isArray(element.export) && element.export.length) return cleanExportRows(element.export);
  return [DEFAULT_EXPORT_ROW];
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

    for (const row of elementRows) {
      const { width, height } = resolveExportScale(row.scale, sourceW, sourceH);
      const base = slug(element.name || element.id);
      let file = `${base}${row.suffix}.${formatExt(row.format)}`;
      let counter = 2;
      while (used.has(file)) {
        file = `${base}${row.suffix}_${String(counter).padStart(2, "0")}.${formatExt(row.format)}`;
        counter += 1;
      }
      used.add(file);
      const outAbs = resolveProjectPath(root, projectId, "export", stamp, file);
      const needsResize = width !== sourceW || height !== sourceH;
      const pureCopy = !needsResize && row.format === "png" && srcFmt === "png";

      const item = {
        elementId: element.id,
        name: element.name || element.id,
        file,
        src: element.src,
        scale: row.scale,
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
      await runExportPython(root, ["ai_studio/assets/canvas/tools/export_images.py", "--spec", specPath]);
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

// ---- renderGroup (screen compositing) ----------------------------------------
//
// Composite a group's VISIBLE member elements (element.visible !== false), in
// element array order (z-order), clipped to the group bounds, into ONE PNG at
// the requested scale over a transparent (or solid) background. The pixel work
// is done by our own Python tool (tools/render_group.py, PIL) because there is
// no dependency-free pure-Node compositor. This tool is OURS, so ops spawns it
// directly with the same robust Python discovery the image tools bridge uses; the
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

// Composite one group's visible members into a screen PNG at the given absolute
// output/spec/report paths (spawns render_group.py once). Shared by renderGroup
// (single screen, own folder) and exportProject (every visible screen into one
// folder). Uses the module's legacy runPython discovery like the other bridged
// canvas tools; the T0218 canvas seam flips these to the config-only interpreter.
async function compositeGroup(root, projectId, project, group, { scale, background, outputAbs, specAbs, reportAbs } = {}) {
  const renderScale = finite(scale) && Number(scale) > 0 ? Number(scale) : 1;
  const bg = background === undefined || background === null || background === "" ? null : hexColor(background);
  if (background && bg === null) throw new Error(`background must be #rrggbb, got ${JSON.stringify(background)}`);

  // Visible member elements, in element array order (z-order).
  const members = (project.elements || []).filter(
    (element) => element.groupId === group.id && element.visible !== false && element.type === "image" && element.src,
  );
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

// Project-level export used when nothing is selected: render EVERY visible group
// (screen) at its own default 1x png into ONE <project>/export/<utc-stamp>/ folder
// plus a combined manifest. Reuses the renderGroup compositor (compositeGroup) per
// visible screen. Like the other export ops it makes no project mutation, so it is
// NOT journaled; it records one export_project tool_runs entry.
export async function exportProject(root, { projectId } = {}) {
  if (!projectId) throw new Error("exportProject requires projectId");
  const project = getProject(root, projectId);
  const visibleGroups = groupsOf(project).filter((group) => group.visible !== false);
  if (!visibleGroups.length) throw new Error("no visible screens to export (project export renders every visible group)");

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
