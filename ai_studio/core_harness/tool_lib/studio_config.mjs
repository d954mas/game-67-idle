// Studio config reader: resolves studio-wide settings that live outside any one
// module. Reads the committed ai_studio/studio.config.json merged with an
// optional gitignored ai_studio/studio.config.local.json override. Node builtins
// only; no domain policy beyond reading + path resolution.
//
// Resolution for a setting value: local override > committed main > error.
// Nothing here creates directories on disk; callers create their own roots
// lazily (e.g. the canvas store only makes canvasProjectsRoot on first create).
import { existsSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { isAbsolute, join, resolve } from "node:path";

export const STUDIO_CONFIG_SCHEMA = "ai_studio.studio_config.v1";

function mainConfigPath(root) {
  return resolve(root, "ai_studio", "studio.config.json");
}

function localConfigPath(root) {
  return resolve(root, "ai_studio", "studio.config.local.json");
}

function readJsonIfExists(path) {
  if (!existsSync(path)) return null;
  try {
    return JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
  } catch (error) {
    throw new Error(`invalid studio config JSON at ${path}: ${error.message}`);
  }
}

// Merged studio config. The local override file (gitignored) wins field-by-field
// over the committed defaults. Throws a clear message if neither file exists.
export function loadStudioConfig(root) {
  const main = readJsonIfExists(mainConfigPath(root));
  const local = readJsonIfExists(localConfigPath(root));
  if (!main && !local) {
    throw new Error(
      `missing studio config: create ai_studio/studio.config.json (schema ${STUDIO_CONFIG_SCHEMA})`,
    );
  }
  return { schema: STUDIO_CONFIG_SCHEMA, ...(main || {}), ...(local || {}) };
}

function looksAbsolute(value) {
  return isAbsolute(value) || /^[a-zA-Z]:[\/]/.test(value);
}

// Absolute on-disk root that holds canvas projects. The CANVAS_PROJECTS_ROOT env
// var overrides everything so tests and one-off runs never touch the configured
// (often YandexDisk) location. Resolution otherwise defers to loadStudioConfig.
export function canvasProjectsRoot(root) {
  const fromEnv = String(process.env.CANVAS_PROJECTS_ROOT || "").trim();
  if (fromEnv) return resolve(fromEnv);
  const raw = String(loadStudioConfig(root).canvasProjectsRoot || "").trim();
  if (!raw) {
    throw new Error(`studio config is missing canvasProjectsRoot (schema ${STUDIO_CONFIG_SCHEMA})`);
  }
  return looksAbsolute(raw) ? resolve(raw) : resolve(root, raw);
}

// Maximum retained undo depth for canvas projects (the journal-compaction horizon).
// CANVAS_HISTORY_DEPTH env overrides for tests/one-off runs; otherwise the committed
// `canvasHistoryDepth` config value is used, defaulting to 200 when unset. A value
// <= 0 means "unlimited" (compaction disabled). Resolution never throws: a missing
// config is treated as the default so read-only history still works before any
// studio config exists.
export const DEFAULT_CANVAS_HISTORY_DEPTH = 200;

export function canvasHistoryDepth(root) {
  const fromEnv = String(process.env.CANVAS_HISTORY_DEPTH || "").trim();
  if (fromEnv) {
    const parsed = Number(fromEnv);
    return Number.isFinite(parsed) ? parsed : DEFAULT_CANVAS_HISTORY_DEPTH;
  }
  let configured;
  try {
    configured = loadStudioConfig(root).canvasHistoryDepth;
  } catch {
    return DEFAULT_CANVAS_HISTORY_DEPTH;
  }
  const parsed = Number(configured);
  return Number.isFinite(parsed) ? parsed : DEFAULT_CANVAS_HISTORY_DEPTH;
}

// Per-machine LOCAL cache root for the canvas history subsystem — the journal, sidecar
// snapshots, compaction archive, fat-journal backup, and cross-process lock (T0259). Kept
// OFF the (cloud-synced) canvasProjectsRoot so a per-gesture write no longer churns sync
// traffic; project.json and files/ stay in the synced folder, undo history deliberately
// does not. Resolution precedence:
//   1. CANVAS_LOCAL_CACHE_ROOT env — explicit override (a fast local disk in production;
//      tests that assert the exact layout).
//   2. When the projects root is itself redirected off its configured (synced) location via
//      CANVAS_PROJECTS_ROOT — the test/one-off signal this module already documents — the
//      cache follows into the OS temp area. A fake/nonexistent repo root then never litters
//      the real filesystem, and parallel suites stay isolated (keyed downstream in store.mjs
//      by a hash of the resolved projects root, so identical project ids never collide).
//   3. Committed studio config `canvasLocalCacheRoot` (local override wins), relative to root.
//   4. Default: <repoRoot>/tmp/canvas_cache — gitignored, local disk.
// A missing config never throws here (like canvasHistoryDepth). An UNUSABLE resolved root is
// NEVER silently swapped: it surfaces later as a loud mkdir/EACCES error from the store.
export function canvasLocalCacheRoot(root) {
  const fromEnv = String(process.env.CANVAS_LOCAL_CACHE_ROOT || "").trim();
  if (fromEnv) return resolve(fromEnv);
  if (String(process.env.CANVAS_PROJECTS_ROOT || "").trim()) {
    return join(tmpdir(), "ai_studio_canvas_cache");
  }
  let configured;
  try {
    configured = loadStudioConfig(root).canvasLocalCacheRoot;
  } catch {
    configured = undefined; // no studio config yet → fall through to the repo-local default
  }
  const raw = String(configured || "").trim();
  if (!raw) return resolve(root, "tmp", "canvas_cache");
  return looksAbsolute(raw) ? resolve(raw) : resolve(root, raw);
}

// Absolute on-disk root of the ISOLATED, wholesale-deletable video-generation
// experiment (T0257): the portable ComfyUI stack, the draft/final workflow
// JSONs, and the MatAnyone tool venv. Deliberately OUTSIDE the repo
// — its outputs and model weights are large and machine-local, and the whole
// thing can be deleted without touching git. The Track B video-animation
// pipeline (ai_studio/assets/tools/video/**) reads this to find the ComfyUI
// server and the profile workflows. Resolution mirrors
// canvasProjectsRoot: VIDEO_GEN_ROOT env overrides everything (tests/one-off
// runs); otherwise the committed `videoGenRoot` (local override wins). Unlike
// the canvas accessors this THROWS loudly when unset — a video stage cannot run
// without the experiment stack, and a silent default would be a fallback.
// NOTE (T0335): VIDEO_GEN_ROOT no longer covers CorridorKey — sandboxing or
// redirecting the CorridorKey install (incl. its ClipsForInference staging)
// is ONLY corridorKeyRoot / env CORRIDOR_KEY_ROOT below.
export function videoGenRoot(root) {
  const fromEnv = String(process.env.VIDEO_GEN_ROOT || "").trim();
  if (fromEnv) return resolve(fromEnv);
  const raw = String(loadStudioConfig(root).videoGenRoot || "").trim();
  if (!raw) {
    throw new Error(`studio config is missing videoGenRoot (schema ${STUDIO_CONFIG_SCHEMA})`);
  }
  return looksAbsolute(raw) ? resolve(raw) : resolve(root, raw);
}

// Absolute on-disk root of the PERMANENT CorridorKey install (upstream repo
// clone + its uv-managed venv + checkpoints, ~9GB). Split out of videoGenRoot
// in T0335: CorridorKey outlives the deletable video-gen experiment — it is the
// canvas's first-priority alpha method for glow/translucent art (lead-ratified,
// alpha bench 2026-07-07). Machine-local and never committed (CC-BY-NC-SA-4.0
// upstream, asset-processing carve-out; only wrappers live in the repo).
// CORRIDOR_KEY_ROOT env overrides (tests/one-off runs); otherwise the committed
// `corridorKeyRoot` (local override wins). THROWS loudly when unset — the
// CorridorKey stages cannot run without the install, and a silent default
// would be a fallback.
export function corridorKeyRoot(root) {
  const fromEnv = String(process.env.CORRIDOR_KEY_ROOT || "").trim();
  if (fromEnv) return resolve(fromEnv);
  const raw = String(loadStudioConfig(root).corridorKeyRoot || "").trim();
  if (!raw) {
    throw new Error(`studio config is missing corridorKeyRoot (schema ${STUDIO_CONFIG_SCHEMA})`);
  }
  return looksAbsolute(raw) ? resolve(raw) : resolve(root, raw);
}
