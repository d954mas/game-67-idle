// Neutral Studio config loader. Domain modules own key interpretation,
// environment overrides, defaults, and path validation.
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";

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
