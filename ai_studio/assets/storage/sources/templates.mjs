import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join, relative } from "node:path";

const defaultRegistry = {
  schema: "ai_studio.assets.templates.v1",
  templates: [{
    id: "template",
    title: "Template",
    folder: "templates/template",
    assets: "templates/template/assets",
    status: "active",
  }],
};

function registryPath(root) {
  return join(root, "templates", "templates.json");
}

function normalizeRelPath(value, label = "path") {
  const text = String(value || "").trim().replace(/\\/g, "/").replace(/^\.?\//, "").replace(/\/+$/, "");
  if (!text) return "";
  if (isAbsolute(text) || /^[a-zA-Z]:\//.test(text) || text.startsWith("//") || text.split("/").includes("..")) {
    throw new Error(`template ${label} must be repo-relative and stay inside the repository`);
  }
  return text;
}

function readRegistry(root) {
  const path = registryPath(root);
  if (!existsSync(path)) return { ...defaultRegistry, templates: [...defaultRegistry.templates] };
  const parsed = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
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
      folder: normalizeRelPath(template.folder || template.id, "folder"),
      assets: normalizeRelPath(template.assets, "assets"),
      status: String(template.status || "active"),
    }))
    .sort((a, b) => a.id.localeCompare(b.id));
}

export function registerTemplateAssetSource(root, { id, title = "", folder = "", assets = "", status = "active" }) {
  const templateId = String(id || "").trim();
  if (!/^[a-z][a-z0-9-]*$/.test(templateId)) {
    throw new Error("template id must be lowercase kebab-case");
  }

  const relFolder = normalizeRelPath(folder || `templates/${templateId}`, "folder");
  const relAssets = normalizeRelPath(assets || `${relFolder}/assets`, "assets");
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
