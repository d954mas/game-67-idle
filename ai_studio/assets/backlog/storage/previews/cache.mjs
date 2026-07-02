import { copyFileSync, existsSync, mkdirSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, extname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import {
  assetPreviewCacheStatus,
  assetPreviewMetaPath,
  ensureAssetIndex,
  rebuildAssetIndex,
  summarizeIndexedPreviewStatus,
} from "../index/index.mjs";
import { hasPackManifestSource, scanPackManifestSource } from "../manifests/manifest.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const previewCacheVersion = 1;
const imageExt = new Set([".png", ".jpg", ".jpeg", ".webp", ".gif"]);
const modelExt = new Set([".glb", ".gltf"]);
const blenders = [
  "C:\\Program Files\\Blender Foundation\\Blender 4.3\\blender.exe",
  "C:\\Program Files\\Blender Foundation\\Blender 4.2\\blender.exe",
  "C:\\Program Files\\Blender Foundation\\Blender 3.2\\blender.exe",
];

function safeSlug(value) {
  return String(value || "asset").replace(/[^a-zA-Z0-9_.-]+/g, "_").replace(/^_+|_+$/g, "") || "asset";
}

function previewDir(root, source, assetId) {
  return join(root, "tmp", "ai_studio", "assets", "previews", safeSlug(source.id), safeSlug(assetId));
}

function sourceStats(path) {
  const stat = statSync(path);
  return {
    sourcePath: resolve(path).replace(/\\/g, "/"),
    sourceMtimeMs: Math.round(stat.mtimeMs),
    sourceSize: stat.size,
  };
}

function writePreviewMeta(root, source, assetId, sourcePath, kind, previewFile, size) {
  const meta = {
    version: previewCacheVersion,
    kind,
    previewFile,
    previewSize: size,
    generatedAt: new Date().toISOString(),
    ...sourceStats(sourcePath),
  };
  writeFileSync(assetPreviewMetaPath(root, source, assetId), JSON.stringify(meta, null, 2), "utf8");
}

function walkSync(dir) {
  const out = [];
  let entries = [];
  try {
    entries = readdirSync(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const entry of entries) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walkSync(path));
    else out.push(path);
  }
  return out;
}

// Folder-scanned assets are identified by their path RELATIVE TO THE SOURCE
// ROOT, exactly as the index does in scanFolderRecords (`path.sourceRel`), so
// the rendered/copied preview cache is written under the same id the index
// later looks it up by (assetPreviewCachePath). Deriving it from a repo-relative
// slice broke on Windows when the roots differed in separator direction
// (path.startsWith failed and the whole absolute path leaked into the id),
// orphaning every folder-source model preview. path.relative normalizes both
// sides, so this stays correct regardless of separator direction.
function assetIdFor(sourceRoot, path) {
  return relative(sourceRoot, path).replace(/[\\/]/g, "__");
}

function extensionTarget(ext) {
  if (ext === ".jpg" || ext === ".jpeg") return ".jpg";
  if (ext === ".gif") return ".gif";
  if (ext === ".webp") return ".webp";
  return ".png";
}

function findBlender(customPath = "") {
  if (customPath && existsSync(customPath)) return customPath;
  return blenders.find((path) => existsSync(path)) || "";
}

function glbIn(dir) {
  if (!dir || !existsSync(dir)) return "";
  const file = readdirSync(dir).find((name) => modelExt.has(extname(name).toLowerCase()));
  return file ? join(dir, file) : "";
}

async function collectManifestPreviewTargets(root, source, force) {
  const images = [];
  const models = [];
  const stats = { skippedImages: 0, skippedModels: 0, staleImages: 0, staleModels: 0 };
  const { records } = await scanPackManifestSource(source.path);
  for (const record of records) {
    if (!record.asset_id) continue;
    if (record.kind === "model") {
      if (record.preview && !force) {
        stats.skippedModels += 1;
        continue;
      }
      const model = record.modelPath || glbIn(record.filesDir);
      if (!model) continue;
      const status = assetPreviewCacheStatus(root, source, record.asset_id, model);
      if (!force && status === "clean") {
        stats.skippedModels += 1;
        continue;
      }
      if (status === "stale") stats.staleModels += 1;
      models.push({ assetId: record.asset_id, path: model });
    }
  }
  return { images, models, ...stats };
}

function collectFolderPreviewTargets(sourceRoot, root, source, force) {
  const images = [];
  const models = [];
  const stats = { skippedImages: 0, skippedModels: 0, staleImages: 0, staleModels: 0 };
  for (const path of walkSync(sourceRoot)) {
    const ext = extname(path).toLowerCase();
    if (!imageExt.has(ext) && !modelExt.has(ext)) continue;
    const assetId = assetIdFor(sourceRoot, path);
    const status = assetPreviewCacheStatus(root, source, assetId, path);
    if (!force && status === "clean") {
      if (imageExt.has(ext)) stats.skippedImages += 1;
      if (modelExt.has(ext)) stats.skippedModels += 1;
      continue;
    }
    if (status === "stale") {
      if (imageExt.has(ext)) stats.staleImages += 1;
      if (modelExt.has(ext)) stats.staleModels += 1;
    }
    if (imageExt.has(ext)) images.push({ assetId, path, ext });
    if (modelExt.has(ext)) models.push({ assetId, path });
  }
  return { images, models, ...stats };
}

function copyImagePreviews(root, source, images) {
  let copied = 0;
  for (const image of images) {
    const dir = previewDir(root, source, image.assetId);
    mkdirSync(dir, { recursive: true });
    const ext = extensionTarget(image.ext);
    for (const old of readdirSync(dir, { withFileTypes: true }).filter((entry) => entry.isFile()).map((entry) => entry.name)) {
      if (/^preview\.(png|jpg|jpeg|webp|gif)$/i.test(old) && old !== "preview" + ext) rmSync(join(dir, old), { force: true });
    }
    copyFileSync(image.path, join(dir, "preview" + ext));
    writePreviewMeta(root, source, image.assetId, image.path, "image-copy", "preview" + ext, 0);
    copied += 1;
  }
  return copied;
}

function renderModelPreviews(root, source, models, { blender = "", size = 512 } = {}) {
  if (!models.length) return { rendered: 0, skipped: 0, reason: "" };
  const blenderPath = findBlender(blender);
  if (!blenderPath) return { rendered: 0, skipped: models.length, reason: "blender.exe not found" };

  const workDir = join(root, "tmp", "ai_studio", "assets", "preview_work", safeSlug(source.id));
  const outDir = join(workDir, "out");
  const manifest = join(workDir, "manifest.txt");
  mkdirSync(workDir, { recursive: true });
  rmSync(outDir, { recursive: true, force: true });
  mkdirSync(outDir, { recursive: true });
  writeFileSync(manifest, models.map((model) => model.path.replace(/\\/g, "/") + "::" + safeSlug(model.assetId)).join("\n"), "utf8");

  const args = ["--background", "--python", join(here, "render_thumbs.py"), "--", outDir, String(size), "--webp", "@" + manifest];
  const result = spawnSync(blenderPath, args, { stdio: ["ignore", "pipe", "pipe"], encoding: "utf8" });

  let rendered = 0;
  for (const model of models) {
    const renderedPath = join(outDir, safeSlug(model.assetId) + ".webp");
    if (!existsSync(renderedPath)) continue;
    const dir = previewDir(root, source, model.assetId);
    mkdirSync(dir, { recursive: true });
    copyFileSync(renderedPath, join(dir, "preview.webp"));
    writePreviewMeta(root, source, model.assetId, model.path, "model-render", "preview.webp", size);
    rendered += 1;
  }
  const blenderOutput = [result.stdout, result.stderr].filter(Boolean).join("\n").trim();
  const reason = result.status === 0
    ? (rendered < models.length ? `Blender rendered ${rendered}/${models.length} model previews; models without previews may be empty or unsupported.` : "")
    : blenderOutput || `blender exited with ${result.status}`;
  return {
    rendered,
    skipped: models.length - rendered,
    reason,
  };
}

export async function refreshPreviewCache(root, source, options = {}) {
  if (!source.available) throw new Error(`asset source is not available: ${source.path}`);
  const force = Boolean(options.force);
  const currentIndex = await ensureAssetIndex(root, source);
  let previewSummary = null;
  if (!force) {
    previewSummary = await summarizeIndexedPreviewStatus(root, source);
    if (!previewSummary.missing && !previewSummary.stale) {
      const cachedImages = (previewSummary.byKind.texture?.clean || 0) + (previewSummary.byKind.ui?.clean || 0);
      return {
        sourceId: source.id,
        copiedImages: 0,
        renderedModels: 0,
        skippedImages: 0,
        skippedModels: 0,
        cachedImages,
        cachedModels: previewSummary.byKind.model?.clean || 0,
        staleImages: 0,
        staleModels: 0,
        warning: "",
        index: currentIndex,
        previewSummary,
      };
    }
  }
  const collected = hasPackManifestSource(source.path)
    ? await collectManifestPreviewTargets(root, source, force)
    : collectFolderPreviewTargets(source.path, root, source, force);
  const { images, models } = collected;
  const copied = copyImagePreviews(root, source, images);
  const modelResult = renderModelPreviews(root, source, models, options);
  const changedPreviews = copied > 0 || modelResult.rendered > 0;
  const foundSourcePreviews = hasPackManifestSource(source.path) && collected.skippedModels > 0;
  const stalePreviewIndex = foundSourcePreviews && previewSummary && (previewSummary.missing || previewSummary.stale);
  const index = changedPreviews || stalePreviewIndex ? await rebuildAssetIndex(root, source) : currentIndex;
  return {
    sourceId: source.id,
    copiedImages: copied,
    renderedModels: modelResult.rendered,
    skippedImages: collected.skippedImages,
    skippedModels: modelResult.skipped,
    cachedImages: collected.skippedImages,
    cachedModels: collected.skippedModels,
    staleImages: collected.staleImages,
    staleModels: collected.staleModels,
    warning: modelResult.reason,
    index,
  };
}
