import { copyFileSync, existsSync, mkdirSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, extname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { assetPreviewCachePath, rebuildAssetIndex } from "../asset_index/asset_index.mjs";
import { scanLibrary } from "../okf_catalog/find_assets.mjs";

const here = dirname(fileURLToPath(import.meta.url));
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

function assetIdFor(root, path) {
  const rel = path.startsWith(root) ? path.slice(root.length).replace(/^[\\/]+/, "") : path;
  return rel.replace(/[\\/]/g, "__");
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

async function collectLibraryPreviewTargets(root, source, force) {
  const images = [];
  const models = [];
  const records = await scanLibrary(source.path);
  for (const record of records) {
    if (!record.asset_id) continue;
    if (!force && (record.preview || assetPreviewCachePath(root, source, record.asset_id))) continue;
    if (record.kind === "model") {
      const model = glbIn(record.filesDir);
      if (model) models.push({ assetId: record.asset_id, path: model });
    }
  }
  return { images, models };
}

function collectFolderPreviewTargets(sourceRoot, root, source, force) {
  const images = [];
  const models = [];
  for (const path of walkSync(sourceRoot)) {
    const ext = extname(path).toLowerCase();
    if (!imageExt.has(ext) && !modelExt.has(ext)) continue;
    const assetId = assetIdFor(root, path);
    if (!force && assetPreviewCachePath(root, source, assetId)) continue;
    if (imageExt.has(ext)) images.push({ assetId, path, ext });
    if (modelExt.has(ext)) models.push({ assetId, path });
  }
  return { images, models };
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
  const { images, models } = source.mode === "library"
    ? await collectLibraryPreviewTargets(root, source, Boolean(options.force))
    : collectFolderPreviewTargets(source.path, root, source, Boolean(options.force));
  const copied = copyImagePreviews(root, source, images);
  const modelResult = renderModelPreviews(root, source, models, options);
  const index = await rebuildAssetIndex(root, source);
  return {
    sourceId: source.id,
    copiedImages: copied,
    renderedModels: modelResult.rendered,
    skippedModels: modelResult.skipped,
    warning: modelResult.reason,
    index,
  };
}
