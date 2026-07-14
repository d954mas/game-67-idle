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

function validateSchema(config, path, { required = false } = {}) {
  if (!config) return;
  const schema = config.schema;
  if (schema === undefined && !required) return;
  if (schema !== STUDIO_CONFIG_SCHEMA) {
    throw new Error(
      `unsupported studio config schema at ${path}: expected ${STUDIO_CONFIG_SCHEMA}, got ${JSON.stringify(schema)}`,
    );
  }
}

export function loadStudioConfig(root) {
  const mainPath = mainConfigPath(root);
  const localPath = localConfigPath(root);
  const main = readJsonIfExists(mainPath);
  const local = readJsonIfExists(localPath);
  if (!main && !local) {
    throw new Error(
      `missing studio config: create ai_studio/studio.config.json (schema ${STUDIO_CONFIG_SCHEMA})`,
    );
  }
  validateSchema(main, mainPath, { required: true });
  validateSchema(local, localPath);
  const { schema: _localSchema, ...localValues } = local || {};
  return { schema: STUDIO_CONFIG_SCHEMA, ...(main || {}), ...localValues };
}
