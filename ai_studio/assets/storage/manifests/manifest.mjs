import { existsSync } from "node:fs";
import { readFile, readdir } from "node:fs/promises";
import { basename, dirname, extname, join, relative, resolve, sep } from "node:path";

const modelExt = new Set([".glb", ".gltf"]);

function list(value) {
  return Array.isArray(value) ? value.filter(Boolean) : value ? [value] : [];
}

function uniqueList(...values) {
  return [...new Set(values.flatMap(list).map(String).filter(Boolean))];
}

function relPosix(root, abs) {
  return relative(root, abs).replace(/\\/g, "/");
}

function safeResolve(base, relativePath = "") {
  const resolvedBase = resolve(base);
  const full = resolve(resolvedBase, relativePath);
  if (full !== resolvedBase && !full.startsWith(resolvedBase + sep)) {
    throw new Error(`manifest path escapes pack directory: ${relativePath}`);
  }
  return full;
}

async function readJson(path) {
  return JSON.parse((await readFile(path, "utf8")).replace(/^\uFEFF/, ""));
}

async function readJsonl(path) {
  const text = await readFile(path, "utf8");
  const rows = [];
  for (const [index, raw] of text.split(/\r?\n/).entries()) {
    const line = raw.replace(/^\uFEFF/, "").trim();
    if (!line || line.startsWith("#")) continue;
    try {
      rows.push(JSON.parse(line));
    } catch (error) {
      throw new Error(`${path}:${index + 1}: invalid JSONL asset row: ${error.message}`);
    }
  }
  return rows;
}

async function listPackDirs(sourceRoot) {
  const roots = [join(sourceRoot, "packs"), join(sourceRoot, "restricted", "packs")];
  const dirs = [];
  for (const packsRoot of roots) {
    if (!existsSync(packsRoot)) continue;
    const entries = await readdir(packsRoot, { withFileTypes: true });
    dirs.push(...entries.filter((entry) => entry.isDirectory()).map((entry) => join(packsRoot, entry.name)));
  }
  return dirs.sort();
}

function normalizePack(sourceRoot, packDir, pack) {
  const packId = pack.pack || basename(packDir);
  const cover = pack.cover ? safeResolve(packDir, pack.cover) : "";
  return {
    pack: packId,
    title: pack.title || packId,
    source: pack.source || "",
    kind: pack.kind || "",
    license: pack.license || "",
    license_url: pack.license_url || "",
    license_kind: pack.license_kind || "",
    attribution_required: pack.attribution_required || "",
    notice_required: pack.notice_required || "",
    credit_text: pack.credit_text || "",
    origin: pack.origin || "unknown",
    count: Number(pack.count) || 0,
    genre: list(pack.genre),
    style: list(pack.style),
    tags: list(pack.tags),
    cover: pack.cover || "",
    cover_img: cover && existsSync(cover) ? cover : "",
    description: pack.description || "",
    body: pack.body || "",
    metadataDir: packDir,
    resource: relPosix(sourceRoot, packDir),
  };
}

function resolveAssetPath(sourceRoot, packDir, value = "", sourceRelative = false) {
  if (!value) return "";
  return safeResolve(sourceRelative ? sourceRoot : packDir, value);
}

function normalizeAsset(sourceRoot, packDir, pack, asset) {
  const packId = pack.pack || basename(packDir);
  const assetId = asset.asset_id || asset.id;
  if (!assetId) throw new Error(`${join(packDir, "assets.jsonl")}: asset row is missing asset_id`);
  const resourcePath = asset.source_resource || asset.resource || asset.path || "";
  const previewPath = asset.source_preview || asset.preview || "";
  const resourceSourceRelative = Boolean(asset.source_resource);
  const previewSourceRelative = Boolean(asset.source_preview);
  const modelPath = asset.model || (modelExt.has(extname(resourcePath).toLowerCase()) ? resourcePath : "");
  const resolvedResource = resolveAssetPath(sourceRoot, packDir, resourcePath, resourceSourceRelative);
  const resolvedPreview = resolveAssetPath(sourceRoot, packDir, previewPath, previewSourceRelative);
  const resolvedModel = asset.source_model
    ? resolveAssetPath(sourceRoot, packDir, asset.source_model, true)
    : modelPath
      ? resolveAssetPath(sourceRoot, packDir, modelPath, resourceSourceRelative)
      : "";
  const filesDir = resolvedResource ? dirname(resolvedResource) : "";
  return {
    asset_id: assetId,
    title: asset.title || assetId,
    description: asset.description || "",
    kind: asset.kind || pack.kind || "",
    status: asset.status || "accepted",
    license: asset.license || pack.license || "",
    license_url: asset.license_url || pack.license_url || "",
    license_kind: asset.license_kind || pack.license_kind || "",
    attribution_required: asset.attribution_required || pack.attribution_required || "",
    notice_required: asset.notice_required || pack.notice_required || "",
    credit_text: asset.credit_text || pack.credit_text || "",
    origin: asset.origin || pack.origin || "unknown",
    publish: asset.publish || pack.publish || "",
    commercial_use: asset.commercial_use || pack.commercial_use || "",
    modification_allowed: asset.modification_allowed || pack.modification_allowed || "",
    redistribution_allowed: asset.redistribution_allowed || pack.redistribution_allowed || "",
    sha256: asset.sha256 || "",
    bytes: asset.bytes || "",
    pack: packId,
    packs: uniqueList(packId, asset.packs, asset.member_of, asset.bundles),
    source_id: asset.source_id || assetId,
    author: asset.author || asset.author_vendor || pack.author || pack.author_vendor || "",
    author_vendor: asset.author_vendor || asset.author || pack.author_vendor || pack.author || "",
    source_page: asset.source_page || asset.source_page_url || pack.source_page || pack.source_page_url || "",
    tags: uniqueList(pack.tags, asset.tags),
    genre: uniqueList(pack.genre, asset.genre),
    style: uniqueList(pack.style, asset.style),
    resource: resolvedResource ? relPosix(sourceRoot, resolvedResource) : "",
    filesDir,
    modelPath: resolvedModel,
    preview: resolvedPreview,
    metadataPath: join(packDir, "assets.jsonl"),
    sourceName: pack.source || "",
  };
}

export async function scanPackManifestSource(sourceRoot) {
  const records = [];
  const packs = [];
  for (const packDir of await listPackDirs(sourceRoot)) {
    const packPath = join(packDir, "pack.json");
    const assetsPath = join(packDir, "assets.jsonl");
    if (!existsSync(packPath) || !existsSync(assetsPath)) continue;
    const packJson = await readJson(packPath);
    const pack = normalizePack(sourceRoot, packDir, packJson);
    const assets = await readJsonl(assetsPath);
    pack.count = assets.length;
    packs.push(pack);
    for (const asset of assets) records.push(normalizeAsset(sourceRoot, packDir, pack, asset));
  }
  return { records, packs };
}

export function hasPackManifestSource(sourceRoot) {
  return existsSync(join(sourceRoot, "packs")) || existsSync(join(sourceRoot, "restricted", "packs"));
}
