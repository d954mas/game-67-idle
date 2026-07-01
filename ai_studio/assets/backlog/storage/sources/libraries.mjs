import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join, relative, resolve } from "node:path";

const defaultRegistry = {
  schema: "ai_studio.assets.libraries.v1",
  libraries: [],
};

function registryPath(root) {
  return join(root, "ai_studio", "assets", "backlog", "storage", "sources", "libraries.json");
}

function normalizeSourcePath(value) {
  const text = String(value || "").trim().replace(/\\/g, "/").replace(/\/+$/, "");
  if (/^[a-zA-Z]:\//.test(text) || text.startsWith("//")) return text;
  return text.replace(/^\.?\//, "");
}

function readRegistry(root) {
  const path = registryPath(root);
  if (!existsSync(path)) return { ...defaultRegistry, libraries: [...defaultRegistry.libraries] };
  const parsed = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
  return {
    schema: parsed.schema || defaultRegistry.schema,
    libraries: Array.isArray(parsed.libraries) ? parsed.libraries : [],
  };
}

function writeRegistry(root, registry) {
  const path = registryPath(root);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(registry, null, 2)}\n`, "utf8");
}

export function resolveRegisteredSourcePath(root, sourcePath) {
  const normalized = normalizeSourcePath(sourcePath);
  if (!normalized) return "";
  if (isAbsolute(normalized) || /^[a-zA-Z]:\//.test(normalized) || normalized.startsWith("//")) return normalized;
  return resolve(root, normalized);
}

export function listRegisteredLibraries(root) {
  const registry = readRegistry(root);
  return registry.libraries
    .filter((library) => library && library.id && library.assets)
    .map((library) => ({
      id: String(library.id),
      title: String(library.title || library.id),
      assets: normalizeSourcePath(library.assets),
      status: String(library.status || "active"),
    }))
    .sort((a, b) => a.id.localeCompare(b.id));
}

export function defaultLibrarySourceRoot(root = process.cwd()) {
  const libraries = listRegisteredLibraries(root);
  const library = libraries.find((item) => item.id === "global-library" && item.status !== "disabled")
    || libraries.find((item) => item.status !== "disabled");
  return library ? resolveRegisteredSourcePath(root, library.assets) : "";
}

export function registerLibraryAssetSource(root, { id, title = "", assets = "", status = "active" }) {
  const libraryId = String(id || "").trim();
  if (!/^[a-z][a-z0-9-]*$/.test(libraryId)) {
    throw new Error("library id must be lowercase kebab-case");
  }

  const normalizedAssets = normalizeSourcePath(assets);
  if (!normalizedAssets) throw new Error("library assets path is required");
  const registry = readRegistry(root);
  const next = {
    id: libraryId,
    title: String(title || libraryId),
    assets: normalizedAssets,
    status,
  };

  const existing = registry.libraries.findIndex((library) => library && library.id === libraryId);
  if (existing >= 0) registry.libraries[existing] = next;
  else registry.libraries.push(next);
  registry.libraries.sort((a, b) => String(a.id).localeCompare(String(b.id)));
  writeRegistry(root, registry);
  return next;
}

export function libraryRegistryPath(root) {
  return relative(root, registryPath(root)).replace(/\\/g, "/");
}
