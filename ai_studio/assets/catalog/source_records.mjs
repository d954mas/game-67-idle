import { createHash } from "node:crypto";
import { existsSync, readFileSync, statSync } from "node:fs";
import { basename, dirname, extname, join, resolve } from "node:path";
import { hasPackManifestSource, scanPackManifestSource } from "../manifests/ops.mjs";
import { assetPreviewCachePath } from "../previews/status.mjs";
import {
  assetKind,
  detectOrigin,
  findLicense,
  glbExt,
  imageExt,
  kindForExt,
  relPosix,
  uniqueList,
  vendorNames,
  walkSync,
} from "./shared.mjs";

export function metadataStats(path) {
  if (!path || !existsSync(path)) return { mtimeMs: 0, size: 0 };
  const stat = statSync(path);
  return { mtimeMs: Math.round(stat.mtimeMs), size: stat.size };
}

export function recordSource(record) {
  return (record.asset_id || "").split("__")[0] || "";
}

export function recordPacks(record) {
  return uniqueList(record.pack, record.packs, record.member_of, record.bundles);
}

export function recordTerms(record) {
  const terms = [];
  const push = (key, values) => {
    const list = Array.isArray(values) ? values : values ? [values] : [];
    for (const value of list.filter(Boolean)) terms.push([key, String(value)]);
  };
  push("kind", record.kind);
  push("origin", record.origin);
  push("license", record.license);
  push("source", record.sourceName || recordSource(record));
  push("pack", recordPacks(record));
  push("genre", record.genre);
  push("style", record.style);
  push("tags", record.tags);
  return terms;
}

export function addFacetCounts(counts, terms) {
  for (const [key, value] of terms) {
    if (!key || !value) continue;
    const id = `${key}\0${value}`;
    const current = counts.get(id);
    if (current) current.count += 1;
    else counts.set(id, { key, value, count: 1 });
  }
}

function isCoveredPath(path, covered) {
  if (!covered) return false;
  const full = resolve(path);
  if (covered.files.has(full)) return true;
  let current = full;
  while (true) {
    if (covered.dirs.has(current)) return true;
    const parent = dirname(current);
    if (parent === current) return false;
    current = parent;
  }
}

function scanFolderRecords(root, source, covered = null) {
  const dirCache = new Map();
  const shouldSkipDir = (path) => isLifecycleAuditDir(path) || Boolean(covered?.dirs?.has(resolve(path)));
  return listSourceAssetFiles(root, source, shouldSkipDir)
    .filter((file) => !isCoveredPath(file.path, covered))
    .filter((file) => !isAcceptedIncomingFile(source, file, covered))
    .map((path) => {
      const ext = path.ext;
      const rel = path.sourceRel;
      const kind = path.kind;
      const abs = path.path;
      const dir = dirname(abs);
      if (!dirCache.has(dir)) dirCache.set(dir, walkSync(dir));
      const dirFiles = dirCache.get(dir);
      const licenseFile = findLicense(dirFiles);
      const origin = detectOrigin(rel);
      const sourceName = vendorNames.find((vendor) => rel.toLowerCase().includes(vendor)) || "unknown";
      const id = rel.replace(/[\\/]/g, "__");
      const cachedPreview = assetPreviewCachePath(root, source, id);
      return {
        asset_id: id,
        title: basename(abs),
        description: "",
        kind,
        origin,
        license: licenseFile ? "see license" : "unknown",
        pack: "",
        source_id: "",
        tags: [],
        genre: [],
        style: [],
        resource: rel,
        filesDir: glbExt.has(ext) ? dir : "",
        preview: cachedPreview || (imageExt.has(ext) ? abs : ""),
        modelPath: glbExt.has(ext) ? abs : "",
        metadataPath: abs,
        sourceName,
      };
    })
    .filter(Boolean);
}

function isLifecycleAuditDir(path) {
  return /(^|[\\/])(_accepted|_rejected)([\\/]|$)/i.test(path);
}

function listSourceAssetFiles(root, source, shouldSkipDir = null) {
  return [source.path].flatMap((scanRoot) => walkSync(scanRoot, shouldSkipDir))
    .filter((path) => !/[\\/](_accepted|_rejected|catalog|previews|licenses|\.git|node_modules|tmp)[\\/]/i.test(`/${relPosix(source.path, path)}`))
    .map((path) => {
      const ext = extname(path).toLowerCase();
      const repoRel = relPosix(root, path);
      const sourceRel = relPosix(source.path, path);
      const kind = assetKind(ext, sourceRel);
      return kind ? { path, repoRel, sourceRel, ext, kind } : null;
    })
    .filter(Boolean);
}

function registeredCoveredPaths(root, source, registeredRecords) {
  const files = new Set();
  const dirs = new Set();
  const hashes = new Set();
  const incomingHashesByBasename = incomingIntakeHashesByBasename(source);
  const addFile = (path) => path && files.add(resolve(path));
  const addDir = (path) => path && dirs.add(resolve(path));
  const addResource = (base, value) => {
    if (!value) return;
    const full = resolve(base, value);
    if (kindForExt(extname(value).toLowerCase())) addFile(full);
    else addDir(full);
  };
  for (const record of registeredRecords) {
    addFile(record.modelPath);
    addFile(record.preview);
    addFile(record.metadataPath);
    addResource(source.path, record.resource);
    addResource(root, record.resource);
    if (record.sha256) hashes.add(String(record.sha256).toLowerCase());
    else addMatchingResourceHash(root, source, record, incomingHashesByBasename, hashes);
  }
  return { files, dirs, hashes };
}

function sha256FileSync(path) {
  return createHash("sha256").update(readFileSync(path)).digest("hex");
}

function addHashByName(map, name, hash) {
  if (!name || !hash) return;
  const key = basename(name).toLowerCase();
  if (!map.has(key)) map.set(key, new Set());
  map.get(key).add(String(hash).toLowerCase());
}

function incomingIntakeHashesByBasename(source) {
  const root = join(source.path, "_incoming");
  const out = new Map();
  if (!existsSync(root)) return out;
  for (const path of walkSync(root).filter((file) => basename(file).toLowerCase() === "intake.json")) {
    try {
      const intake = JSON.parse(readFileSync(path, "utf8"));
      addHashByName(out, intake.path || intake.input || "", intake.sha256);
      for (const item of intake.files || []) addHashByName(out, item.path, item.sha256);
    } catch {
      // Broken intake metadata should not block indexing unrelated assets.
    }
  }
  return out;
}

function addMatchingResourceHash(root, source, record, incomingHashesByBasename, hashes) {
  if (!incomingHashesByBasename.size) return;
  const candidates = uniqueList(
    record.modelPath,
    record.metadataPath,
    record.resource ? resolve(source.path, record.resource) : "",
    record.resource ? resolve(root, record.resource) : "",
  ).filter((path) => path && existsSync(path) && kindForExt(extname(path).toLowerCase()));
  for (const path of candidates) {
    const expected = incomingHashesByBasename.get(basename(path).toLowerCase());
    if (!expected?.size) continue;
    const hash = sha256FileSync(path);
    if (expected.has(hash)) hashes.add(hash);
  }
}

function readIncomingIntake(source, file) {
  const parts = file.sourceRel.split("/");
  if (parts[0] !== "_incoming" || parts.length < 4) return null;
  const candidateDir = join(source.path, parts[0], parts[1], parts[2]);
  const intakePath = join(candidateDir, "intake.json");
  if (!existsSync(intakePath)) return null;
  try {
    return { candidateRel: parts.slice(3).join("/"), intake: JSON.parse(readFileSync(intakePath, "utf8")) };
  } catch {
    return null;
  }
}

function incomingIntakeHashes(source, file) {
  const data = readIncomingIntake(source, file);
  if (!data) return [];
  const hashes = [];
  if (data.intake.sha256) hashes.push(String(data.intake.sha256));
  for (const item of data.intake.files || []) {
    if (!item || !item.sha256) continue;
    if (!item.path || item.path === data.candidateRel || basename(item.path) === basename(file.path)) hashes.push(String(item.sha256));
  }
  return hashes.map((hash) => hash.toLowerCase());
}

function isAcceptedIncomingFile(source, file, covered) {
  if (!covered?.hashes?.size || !file.sourceRel.startsWith("_incoming/")) return false;
  return incomingIntakeHashes(source, file).some((hash) => covered.hashes.has(hash));
}

export async function readRegisteredSourceData(root, source) {
  if (hasPackManifestSource(source.path)) return scanPackManifestSource(source.path);
  return { records: [], packs: [] };
}

export function mergeRegisteredWithDiscoveredFiles(root, source, registeredRecords) {
  const covered = registeredCoveredPaths(root, source, registeredRecords);
  const unregistered = scanFolderRecords(root, source, covered).map((record) => ({
    ...record,
    origin: "unregistered",
    license: "unknown",
    sourceName: "unregistered",
    tags: [...new Set([...(record.tags || []), "unregistered"])],
  }));
  return [...registeredRecords, ...unregistered];
}
