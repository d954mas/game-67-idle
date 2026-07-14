import { tmpdir } from "node:os";
import { isAbsolute, join, resolve } from "node:path";

import { STUDIO_CONFIG_SCHEMA, loadStudioConfig } from "../../config.mjs";

function looksAbsolute(value) {
  return isAbsolute(value) || /^[a-zA-Z]:[\\/]/.test(value);
}

function loadOptionalCanvasConfig(root) {
  try {
    return loadStudioConfig(root);
  } catch (error) {
    if (String(error?.message || "").startsWith("missing studio config:")) return {};
    throw error;
  }
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
  const configured = loadOptionalCanvasConfig(root).canvasHistoryDepth;
  const parsed = Number(configured);
  return Number.isFinite(parsed) ? parsed : DEFAULT_CANVAS_HISTORY_DEPTH;
}

export function canvasLocalCacheRoot(root) {
  const fromEnv = String(process.env.CANVAS_LOCAL_CACHE_ROOT || "").trim();
  if (fromEnv) return resolve(fromEnv);
  if (String(process.env.CANVAS_PROJECTS_ROOT || "").trim()) {
    return join(tmpdir(), "ai_studio_canvas_cache");
  }
  const configured = loadOptionalCanvasConfig(root).canvasLocalCacheRoot;
  const raw = String(configured || "").trim();
  if (!raw) return resolve(root, "tmp", "canvas_cache");
  return looksAbsolute(raw) ? resolve(raw) : resolve(root, raw);
}
