import { tmpdir } from "node:os";
import { isAbsolute, join, resolve } from "node:path";

import { STUDIO_CONFIG_SCHEMA, loadStudioConfig } from "../../config.mjs";

function looksAbsolute(value) {
  return isAbsolute(value) || /^[a-zA-Z]:[\\/]/.test(value);
}

export function canvasProjectsRoot(root) {
  const fromEnv = String(process.env.CANVAS_PROJECTS_ROOT || "").trim();
  if (fromEnv) return resolve(fromEnv);
  const raw = String(loadStudioConfig(root).canvasProjectsRoot || "").trim();
  if (!raw) {
    throw new Error(`studio config is missing canvasProjectsRoot (schema ${STUDIO_CONFIG_SCHEMA})`);
  }
  return looksAbsolute(raw) ? resolve(raw) : resolve(root, raw);
}

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
    configured = undefined;
  }
  const raw = String(configured || "").trim();
  if (!raw) return resolve(root, "tmp", "canvas_cache");
  return looksAbsolute(raw) ? resolve(raw) : resolve(root, raw);
}
