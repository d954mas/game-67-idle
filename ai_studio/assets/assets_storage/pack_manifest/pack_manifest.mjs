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
  return JSON.parse(await readFile(path, "utf8"));
}

async function readJsonl(path) {
  const text = await readFile(path, "utf8");
  const rows = [];
  for (const [index, raw] of text.split(/\r?\n/).entries()) {
    const line = raw.trim();
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
  const packsRoot = join(sourceRoot, "packs");
  if (!existsSync(packsRoot)) return [];
  const entries = await readdir(packsRoot, { withFileTypes: true });
  return entries.filter((entry) => entry.isDirectory()).map((entry) => join(packsRoot, entry.name)).sort();
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
    origin: pack.origin || "unknown",
    count: Number(pack.count) || 0,
    genre: list(pack.genre),
    style: list(pack.style),
    tags: list(pack.tags),
    cover: pack.cover || "",
    cover_img: cover && existsSync(cover) ? cover : "",
    description: pack.description || "",
    body: pack.body || "",
    catalogDir: packDir,
    resource: relPosix(sourceRoot, packDir),
  };
}

function normalizeAsset(sourceRoot, packDir, pack, asset) {
  const packId = pack.pack || basename(packDir);
  const assetId = asset.asset_id || asset.id;
  if (!assetId) throw new Error(`${join(packDir, "assets.jsonl")}: asset row is missing asset_id`);
  const resourcePath = asset.resource || asset.path || "";
  const previewPath = asset.preview || "";
  const modelPath = asset.model || (modelExt.has(extname(resourcePath).toLowerCase()) ? resourcePath : "");
  const filesDir = resourcePath ? dirname(safeResolve(packDir, resourcePath)) : "";
  return {
    asset_id: assetId,
    title: asset.title || assetId,
    description: asset.description || "",
    kind: asset.kind || pack.kind || "",
    status: asset.status || "accepted",
    license: asset.license || pack.license || "",
    origin: asset.origin || pack.origin || "unknown",
    pack: packId,
    packs: uniqueList(packId, asset.packs, asset.member_of, asset.bundles),
    source_id: asset.source_id || assetId,
    author: asset.author || pack.author || "",
    tags: uniqueList(pack.tags, asset.tags),
    genre: uniqueList(pack.genre, asset.genre),
    style: uniqueList(pack.style, asset.style),
    resource: resourcePath ? relPosix(sourceRoot, safeResolve(packDir, resourcePath)) : "",
    filesDir,
    modelPath: modelPath ? safeResolve(packDir, modelPath) : "",
    preview: previewPath ? safeResolve(packDir, previewPath) : "",
    catalogPath: join(packDir, "assets.jsonl"),
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
  return existsSync(join(sourceRoot, "packs"));
}
