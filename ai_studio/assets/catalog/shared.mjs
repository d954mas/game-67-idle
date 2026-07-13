import { readdirSync } from "node:fs";
import { basename, extname, join, relative } from "node:path";

export const facetKeys = ["kind", "origin", "license", "source", "pack", "genre", "style", "tags"];
export const primaryExt = {
  image: [".png", ".jpg", ".jpeg", ".webp", ".gif"],
  model: [".obj", ".glb", ".gltf", ".fbx"],
  font: [".ttf", ".otf", ".woff", ".woff2"],
  audio: [".wav", ".mp3", ".ogg"],
};
export const glbExt = new Set([".glb", ".gltf"]);
export const imageExt = new Set(primaryExt.image);
export const vendorNames = ["kenney", "quaternius", "polyhaven", "poly-haven", "ambientcg", "poly-pizza", "opengameart"];
const uiPath = /[\\/](ui|icons?|hud|gui|sprites?|buttons?)[\\/]/i;

export function safeSlug(value) {
  return String(value || "assets").replace(/[^a-zA-Z0-9_.-]+/g, "_").replace(/^_+|_+$/g, "") || "assets";
}

export function relPosix(root, abs) {
  return relative(root, abs).replace(/\\/g, "/");
}

export function walkSync(dir, shouldSkipDir = null) {
  const out = [];
  let entries = [];
  try {
    entries = readdirSync(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const entry of entries) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) {
      if (shouldSkipDir && shouldSkipDir(path)) continue;
      out.push(...walkSync(path, shouldSkipDir));
    } else {
      out.push(path);
    }
  }
  return out;
}

export function kindForExt(ext) {
  for (const [kind, exts] of Object.entries(primaryExt)) {
    if (exts.includes(ext)) return kind;
  }
  return null;
}

export function assetKind(ext, relPath) {
  const broad = kindForExt(ext);
  if (broad === "image") return uiPath.test(relPath) ? "ui" : "texture";
  return broad;
}

export function detectOrigin(relPath) {
  const lower = relPath.toLowerCase();
  if (/[\\/](generated|imagegen|ai[-_]?gen|gen)[\\/]/.test(lower)) return "ai";
  if (/[\\/]source[\\/]/.test(lower) || vendorNames.some((vendor) => lower.includes(vendor))) return "sourced";
  return "unknown";
}

export function findLicense(dirFiles) {
  return dirFiles.find((file) => /license|licence/i.test(basename(file))) || "";
}

export function jsonList(value) {
  return JSON.stringify(Array.isArray(value) ? value.filter(Boolean) : value ? [value] : []);
}

export function parseJsonList(value) {
  if (!value) return [];
  try {
    const parsed = JSON.parse(value);
    return Array.isArray(parsed) ? parsed.filter(Boolean) : [];
  } catch {
    return [];
  }
}

export function list(value) {
  return Array.isArray(value) ? value.filter(Boolean) : value ? [value] : [];
}

export function uniqueList(...values) {
  return [...new Set(values.flatMap(list).map(String).filter(Boolean))];
}

export function hashKey(value) {
  let hash = 2166136261;
  for (const ch of String(value || "")) {
    hash ^= ch.charCodeAt(0);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

export function sourceKey(source) {
  return `${source.id}|${source.path}|${source.type || "source"}`;
}

export function modelPathIn(filesDir) {
  if (!filesDir) return "";
  let names;
  try {
    names = readdirSync(filesDir);
  } catch {
    return "";
  }
  const file = names.find((name) => /\.(glb|gltf)$/i.test(name));
  return file ? join(filesDir, file) : "";
}

export function extensionKind(path) {
  return kindForExt(extname(path).toLowerCase());
}
