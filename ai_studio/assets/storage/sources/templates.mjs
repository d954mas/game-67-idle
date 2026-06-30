import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";

const defaultRegistry = {
  schema: "ai_studio.assets.templates.v1",
  templates: [{
    id: "template",
    title: "Template",
    folder: "template",
    assets: "template/assets",
    status: "active",
  }],
};

function registryPath(root) {
  return join(root, "ai_studio", "assets", "storage", "sources", "templates.json");
}

function normalizeRelPath(value) {
  return String(value || "").replace(/\\/g, "/").replace(/^\.?\//, "").replace(/\/+$/, "");
}

function readRegistry(root) {
  const path = registryPath(root);
  if (!existsSync(path)) return { ...defaultRegistry, templates: [...defaultRegistry.templates] };
  const parsed = JSON.parse(readFileSync(path, "utf8"));
  return {
    schema: parsed.schema || defaultRegistry.schema,
    templates: Array.isArray(parsed.templates) ? parsed.templates : [],
  };
}

function writeRegistry(root, registry) {
  const path = registryPath(root);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(registry, null, 2)}\n`, "utf8");
}

export function listRegisteredTemplates(root) {
  const registry = readRegistry(root);
  return registry.templates
    .filter((template) => template && template.id && template.assets)
    .map((template) => ({
      id: String(template.id),
      title: String(template.title || template.id),
      folder: normalizeRelPath(template.folder || template.id),
      assets: normalizeRelPath(template.assets),
      status: String(template.status || "active"),
    }))
    .sort((a, b) => a.id.localeCompare(b.id));
}

export function registerTemplateAssetSource(root, { id, title = "", folder = "", assets = "", status = "active" }) {
  const templateId = String(id || "").trim();
  if (!/^[a-z][a-z0-9-]*$/.test(templateId)) {
    throw new Error("template id must be lowercase kebab-case");
  }

  const relFolder = normalizeRelPath(folder || templateId);
  const relAssets = normalizeRelPath(assets || `${relFolder}/assets`);
  const registry = readRegistry(root);
  const next = {
    id: templateId,
    title: String(title || templateId),
    folder: relFolder,
    assets: relAssets,
    status,
  };

  const existing = registry.templates.findIndex((template) => template && template.id === templateId);
  if (existing >= 0) registry.templates[existing] = next;
  else registry.templates.push(next);
  registry.templates.sort((a, b) => String(a.id).localeCompare(String(b.id)));
  writeRegistry(root, registry);
  return next;
}

export function templateRegistryPath(root) {
  return relative(root, registryPath(root)).replace(/\\/g, "/");
}
