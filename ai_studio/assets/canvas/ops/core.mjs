// Canvas core operation domain. Public API is ../ops.mjs.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { performance } from "node:perf_hooks";
import { canvasHistoryDepth } from "../config.mjs";
import { FONTS_DIR_REPO_PATH, FONTS_MANIFEST_REPO_PATH, defaultNoteStyle, defaultTextStyle, mergeNoteStyle, mergeTextStyle } from "../fonts.mjs";
import { appendArchive, appendError, appendJournalLine, deleteSnapshot, ensureThinJournal, getProject, listProjects, nextJournalSeq, readJournal, readSnapshot, resolveProjectFile, resolveProjectPath, rewriteJournal, updateProject, withProjectLock, writeSnapshot } from "../store.mjs";

export function ms(value) {
  return Math.round(value * 1000) / 1000;
}

// Round to int and clamp to [0, bound] (far edge inclusive) — the polygon vertex rule
// (mirrors the Python slicer's max(0, min(image_dim, x))).
export function clampRound(value, bound) {
  return Math.max(0, Math.min(bound, Math.round(value)));
}

// Axis-aligned bounding box [x, y, w, h] of an integer polygon, kept inside source
// bounds (ports the legacy rectFromPolygon: floor min / ceil max, min dimension 1). A
// polygonal region stores this as its rect so shape and bbox never diverge.
export function polygonBBox(points, boundsW, boundsH) {
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

export function mimeForExt(fileName) {
  const ext = String(fileName || "").toLowerCase().split(".").pop();
  return (
    { png: "image/png", jpg: "image/jpeg", jpeg: "image/jpeg", gif: "image/gif", webp: "image/webp" }[ext] ||
    "image/png"
  );
}

export function slug(value) {
  const cleaned = String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .slice(0, 60);
  return cleaned || "element";
}

export function finite(value) {
  return value !== undefined && value !== null && Number.isFinite(Number(value));
}

export function groupsOf(project) {
  return Array.isArray(project.groups) ? project.groups : [];
}

export function hexColor(value) {
  const text = String(value || "").trim();
  return /^#[0-9a-fA-F]{6}$/.test(text) ? text.toLowerCase() : null;
}

// ---- text fonts (node side of the shared fonts.mjs contract) -----------------

// Read the bundled fonts.json manifest from disk (node-only; the page fetches the
// same file over HTTP). A loud error names the manifest path when it is missing or
// corrupt — text can't be validated without it.
export function readFontsManifest(root) {
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
export function resolveFontFileAbs(root, entry) {
  return join(root, FONTS_DIR_REPO_PATH, entry.file);
}

// Sanitize a patch that may carry text `content`/`style` for a TEXT element: validate
// + normalize the style against the manifest (loud on unknown family/weight), coerce
// content to a string, and pass every other field (x/y/w/h/name/visible) through
// untouched. A no-op fast path when the patch has neither, so image patches never load
// the manifest. Throws if content/style target a non-text element.
export function sanitizeTextPatch(root, project, elementId, patch = {}) {
  if (patch.style === undefined && patch.content === undefined && patch.background === undefined) return patch;
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const isText = element.type === "text";
  const isNote = element.type === "note"; // T0268: content/style also apply to a note (its own font subset)
  const clean = { ...patch };
  if (patch.content !== undefined || patch.style !== undefined) {
    if (!isText && !isNote) {
      throw new Error(`element ${elementId} is not a text element (content/style only apply to type:"text"/"note")`);
    }
  }
  if (patch.content !== undefined) clean.content = String(patch.content);
  if (patch.style !== undefined) {
    const manifest = readFontsManifest(root);
    clean.style = isNote
      ? mergeNoteStyle(element.style || defaultNoteStyle(), patch.style, manifest)
      : mergeTextStyle(element.style || defaultTextStyle(), patch.style, manifest);
  }
  // Background is NOTE-only (T0268): a loud error on an image or a text element, mirroring
  // the content/style type gate above. Validated/normalized to null or {type:"color", color}.
  if (patch.background !== undefined) {
    if (!isNote) {
      throw new Error(`element ${elementId} is not a note element (background only applies to type:"note")`);
    }
    clean.background = normalizeNoteBackground(patch.background);
  }
  return clean;
}

// Validate + normalize an optional NOTE background (additive field, T0268). Accepts null
// (no fill) or {type:"color", color:"#rrggbb"}; anything else throws a loud error (no
// silent fallback). Reuses the group-background shape/validator via the shared hexColor
// gate. Returns null or the normalized {type:"color", color} object.
export function normalizeNoteBackground(background) {
  if (background === null) return null;
  if (typeof background !== "object" || Array.isArray(background)) {
    throw new Error(`note background must be null or {type:"color", color:"#rrggbb"}, got ${JSON.stringify(background)}`);
  }
  if (background.type !== "color") {
    throw new Error(`note background type must be "color", got ${JSON.stringify(background.type)}`);
  }
  const color = hexColor(background.color);
  if (!color) throw new Error(`note background color must be #rrggbb, got ${JSON.stringify(background.color)}`);
  return { type: "color", color };
}

// Degrees, finite; normalized to [0,360) so 450 and -90 store identically to 90/270 —
// the one canonical rotation value both renderers key off (see README "Rotation & flip").
// Throws on a non-finite value (no silent NaN/Infinity write).
export function normalizeRotation(value) {
  const num = Number(value);
  if (!Number.isFinite(num)) throw new Error(`rotation must be a finite number of degrees, got ${JSON.stringify(value)}`);
  const wrapped = num % 360;
  return wrapped < 0 ? wrapped + 360 : wrapped;
}

// A flip flag must be a real boolean (mirrors normalizeGroupClip below) — no silent
// string coercion; the CLI/page convert their own string flags before calling.
export function normalizeFlipFlag(value, field) {
  if (typeof value !== "boolean") {
    throw new Error(`${field} must be a boolean (true|false), got ${JSON.stringify(value)}`);
  }
  return value;
}

// Sanitize a patch that may carry `rotation`/`flipH`/`flipV` (T0232 increment 3a —
// additive transform schema on `patchElement`/`patchElements`, see README "Rotation &
// flip"). `rotation` is validated + normalized to [0,360) for EITHER element type ("text
// rotates the box" too); `flipH`/`flipV` are IMAGE-ONLY booleans (R7 — mirroring pixels
// makes no sense for a text box), a loud error on a text element. A no-op fast path when
// the patch carries none of the three fields, so an untouched patch never pays for the
// element lookup (mirrors sanitizeTextPatch above).
export function sanitizeTransformPatch(project, elementId, patch = {}) {
  if (patch.rotation === undefined && patch.flipH === undefined && patch.flipV === undefined) return patch;
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  const clean = { ...patch };
  if (patch.rotation !== undefined) clean.rotation = normalizeRotation(patch.rotation);
  if ((patch.flipH !== undefined || patch.flipV !== undefined) && element.type !== "image") {
    throw new Error(
      `element ${elementId} is a ${element.type} element — flip is image-only (flipH/flipV don't apply to type:"${element.type}")`,
    );
  }
  if (patch.flipH !== undefined) clean.flipH = normalizeFlipFlag(patch.flipH, "flipH");
  if (patch.flipV !== undefined) clean.flipV = normalizeFlipFlag(patch.flipV, "flipV");
  return clean;
}

// Validate a patch's optional `opacity` (T0260 Track A prerequisite): a finite number in
// [0,1] (loud otherwise). Returns undefined when the patch carries no opacity (the fast
// path — an untouched patch never pays for this). Unlike x/y/w/h etc, opacity is NOT a
// store-handled field (store.applyElementFields owns geometry/name/visible/text/transform
// only), so patchElement applies the stored-only-when-!=1 write itself — mirroring the
// slice9.scale / rotation:0 "absent = default (1)" convention so an opaque element stays
// byte-identical to a pre-T0260 save.
export function normalizeOpacityPatch(patch) {
  if (patch.opacity === undefined) return undefined;
  const num = Number(patch.opacity);
  if (!Number.isFinite(num) || num < 0 || num > 1) {
    throw new Error(`opacity must be a finite number in [0,1], got ${JSON.stringify(patch.opacity)}`);
  }
  return num;
}

// ---- image filters (non-destructive brightness/saturation/contrast/tint) ------
//
// `element.filters` is an ADDITIVE object (absent = no filters, same "absent means the
// default" convention as `rotation`/`flipH`/`background`) with up to four optional keys:
// `brightness`/`saturation`/`contrast` (finite numbers in [0,2], default 1, stored only
// when != 1) and `tint` ({color:"#rrggbb", strength:[0,1]}, stored only when strength > 0).
// See README "Image filters" for the shared canonical math (both renderers implement the
// SAME per-pixel formulas — PIL is the source of rendered truth, the canvas approximates
// via the browser's spec'd CSS filters, same stance as **Rotation & flip**).
export const FILTER_DEFAULTS = { brightness: 1, saturation: 1, contrast: 1 };

// Normalize one filters object: validates every present key loudly (non-finite/out-of-
// range/bad hex all throw BEFORE any write), then drops any key that lands at its default
// so the stored shape never carries redundant defaults. `null`/`{}` (or an object that
// normalizes to all-defaults) collapses to `null` — the whole-object "clear" signal
// store.mjs's applyElementFields understands (mirrors `rotation:0` clearing to absent).
export function normalizeFilters(input) {
  if (input === null) return null;
  if (typeof input !== "object" || Array.isArray(input)) {
    throw new Error(`filters must be an object, got ${JSON.stringify(input)}`);
  }
  const out = {};
  for (const key of ["brightness", "saturation", "contrast"]) {
    if (input[key] === undefined) continue;
    const num = Number(input[key]);
    if (!Number.isFinite(num) || num < 0 || num > 2) {
      throw new Error(`filters.${key} must be a finite number in [0,2], got ${JSON.stringify(input[key])}`);
    }
    if (num !== FILTER_DEFAULTS[key]) out[key] = num;
  }
  if (input.tint !== undefined && input.tint !== null) {
    if (typeof input.tint !== "object" || Array.isArray(input.tint)) {
      throw new Error(`filters.tint must be an object {color, strength}, got ${JSON.stringify(input.tint)}`);
    }
    const color = hexColor(input.tint.color);
    if (!color) throw new Error(`filters.tint.color must be #rrggbb, got ${JSON.stringify(input.tint.color)}`);
    const strength = Number(input.tint.strength);
    if (!Number.isFinite(strength) || strength < 0 || strength > 1) {
      throw new Error(`filters.tint.strength must be a finite number in [0,1], got ${JSON.stringify(input.tint.strength)}`);
    }
    // Validated above regardless (loud even at strength 0); stored only when it actually
    // does something — mirrors the brightness/saturation/contrast "!= default" rule.
    if (strength > 0) out.tint = { color, strength };
  }
  return Object.keys(out).length ? out : null;
}

// Sanitize a patch that may carry `filters` — image-only (mirrors the flipH/flipV guard:
// a loud error on a text/note element), whole-object REPLACE (not a merge: patching
// `filters:{contrast:1.3}` resets brightness/saturation/tint to their defaults too, same
// as a `style` patch replacing a text element's whole style object). A no-op fast path
// when the patch carries no `filters` key, so an untouched patch never pays for the
// element lookup (mirrors sanitizeTransformPatch above).
export function sanitizeFiltersPatch(project, elementId, patch = {}) {
  if (patch.filters === undefined) return patch;
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  if (element.type !== "image") {
    throw new Error(
      `element ${elementId} is a ${element.type} element — filters are image-only (filters don't apply to type:"${element.type}")`,
    );
  }
  return { ...patch, filters: normalizeFilters(patch.filters) };
}

// ---- journal core ------------------------------------------------------------

export function snapshotOf(project) {
  return {
    // Project-level metadata is carried with elements/groups, so patchProject is
    // fully undoable too.
    title: project.title,
    ownership: project.ownership ? JSON.parse(JSON.stringify(project.ownership)) : null,
    archived: project.archived === true,
    elements: JSON.parse(JSON.stringify(project.elements || [])),
    // Groups are metadata like elements, so the same before/after snapshot makes
    // every group + visibility mutation fully undoable with no file changes.
    groups: JSON.parse(JSON.stringify(project.groups || [])),
    tool_runs: JSON.parse(JSON.stringify(project.tool_runs || [])),
  };
}

// A mutation line (has a sidecar snapshot); tolerates a legacy inline fat line too,
// so undo/redo/history stay correct even if migration has not run yet.
export function isMutation(line) {
  return !!line && (line.has_snapshot === true || line.undo_patch !== undefined || line.state !== undefined);
}

// The {undo_patch, state} snapshot for one entry: inline (legacy fat line) or the
// sidecar file (thin line). Returns {} if neither is present.
export function snapshotForEntry(root, projectId, entry) {
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
export let opsActor = "user";
export function setOpsActor(actor) {
  if (actor !== "user" && actor !== "agent") throw new Error(`unknown ops actor: ${JSON.stringify(actor)}`);
  opsActor = actor;
}

// T0254 Tier 1 #1: `before` was read at the START of an op — for the ops that do slow
// external work (codex/agy) before their final write, that could be minutes ago. If
// another mutation landed on this project in the meantime, anything computed from
// `before` was built on a state this op never actually saw: writing it anyway would
// either lose that other mutation (lost update) or record a parent pointer the
// journal chain never actually passed through. Refuse loudly instead (mirrors
// checkExpectHead's shape, including the stable HEAD_CONFLICT code so api.mjs maps it
// to 409, not prose-matched) — a stale-before refusal is a correct, safe v1 answer;
// the remedy is simply to retry the whole gesture, which reads a fresh `before`.
// `current` is whatever the caller just re-read as the ACTUAL live project — pass it
// straight through (no extra disk read here) so this can run BEFORE a locked op's
// first write, not just before its final commit (see generateFromRecipe and friends,
// which call this immediately after their own re-read, before touching the store —
// otherwise a refusal at commitMutation-time would still leave an earlier, unjournaled
// updateProject write sitting on disk, which is exactly the silent corruption this
// whole check exists to prevent). For ops whose ENTIRE read-modify-commit runs inside
// withProjectLock (the common case, wrapped at the API/CLI layer), this never trips:
// nothing else can have advanced the head in between.
export function refuseIfHeadMoved(op, before, current) {
  const startedHead = Number(before.history_seq) || 0;
  const actualHead = Number(current.history_seq) || 0;
  if (actualHead !== startedHead) {
    const error = new Error(
      `canvas project ${current.id} changed underneath op "${op}" (started at head ${startedHead}, now ${actualHead}) — retry the operation`,
    );
    error.code = "HEAD_CONFLICT";
    throw error;
  }
}

export function commitMutation(root, projectId, { op, args_summary, before, after, startedAt }) {
  ensureThinJournal(root, projectId); // one-time migration of a legacy fat journal
  // Cheap insurance for every op (see refuseIfHeadMoved's doc above): for the common
  // wrapped-at-the-client-layer case this never trips, since nothing could have
  // advanced the head since `before` was read under the same lock.
  refuseIfHeadMoved(op, before, getProject(root, projectId));
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
export function compactJournal(root, projectId) {
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

export { getProject, listProjects, resolveProjectFile, resolveProjectPath, updateProject, withProjectLock };
