// Studio config reader: resolves studio-wide settings that live outside any one
// module. Reads the committed ai_studio/studio.config.json merged with an
// optional gitignored ai_studio/studio.config.local.json override. Node builtins
// only; no domain policy beyond reading + path resolution.
//
// Resolution for a setting value: local override > committed main > error.
// Nothing here creates directories on disk; callers create their own roots
// lazily (e.g. the canvas store only makes canvasProjectsRoot on first create).
import { existsSync, readFileSync } from "node:fs";
import { isAbsolute, resolve } from "node:path";

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
