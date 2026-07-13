import { existsSync, readFileSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { safeSlug } from "../catalog/shared.mjs";

const previewCacheVersion = 1;

function previewCacheDir(root, source) {
  return join(root, "tmp", "ai_studio", "assets", "previews", safeSlug(source.id));
}

function previewItemDir(root, source, assetId) {
  return join(previewCacheDir(root, source), safeSlug(assetId));
}

export function assetPreviewCachePath(root, source, assetId) {
  const dir = previewItemDir(root, source, assetId);
  for (const ext of [".webp", ".png", ".jpg", ".jpeg", ".gif"]) {
    const candidate = join(dir, "preview" + ext);
    if (existsSync(candidate)) return candidate;
  }
  return "";
}

export function assetPreviewMetaPath(root, source, assetId) {
  return join(previewItemDir(root, source, assetId), "preview.json");
}

function previewSourceStats(path) {
  if (!path || !existsSync(path)) return null;
  const stat = statSync(path);
  return {
    sourcePath: resolve(path).replace(/\\/g, "/"),
    sourceMtimeMs: Math.round(stat.mtimeMs),
    sourceSize: stat.size,
  };
}

function readPreviewMeta(root, source, assetId) {
  const path = assetPreviewMetaPath(root, source, assetId);
  if (!existsSync(path)) return null;
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch {
    return null;
  }
}

export function assetPreviewCacheStatus(root, source, assetId, sourcePath = "") {
  if (!assetPreviewCachePath(root, source, assetId)) return "missing";
  const meta = readPreviewMeta(root, source, assetId);
  if (!meta || meta.version !== previewCacheVersion) return "stale";
  const stats = previewSourceStats(sourcePath);
  if (!stats) return "clean";
  return meta.sourcePath === stats.sourcePath
    && meta.sourceMtimeMs === stats.sourceMtimeMs
    && meta.sourceSize === stats.sourceSize
    ? "clean"
    : "stale";
}
