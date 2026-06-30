import { DatabaseSync } from "node:sqlite";
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, statSync } from "node:fs";
import { basename, dirname, extname, join, relative, resolve, sep } from "node:path";
import { scanLibraryWithPacks } from "../okf_catalog/find_assets.mjs";
import { hasPackManifestSource, scanPackManifestSource } from "../pack_manifest/pack_manifest.mjs";
import {
  buildSourceSnapshot,
  diffSourceSnapshots,
  readSourceSnapshot,
  sourceSnapshotSignature,
  writeSourceSnapshot,
} from "../source_snapshots/source_snapshots.mjs";

const schemaVersion = 3;
const previewCacheVersion = 1;
const facetKeys = ["kind", "origin", "license", "source", "pack", "genre", "style", "tags"];
const primaryExt = {
  image: [".png", ".jpg", ".jpeg", ".webp", ".gif"],
  model: [".obj", ".glb", ".gltf", ".fbx"],
  font: [".ttf", ".otf", ".woff", ".woff2"],
  audio: [".wav", ".mp3", ".ogg"],
};
const glbExt = new Set([".glb", ".gltf"]);
const imageExt = new Set(primaryExt.image);
const vendorNames = ["kenney", "quaternius", "polyhaven", "poly-haven", "ambientcg", "poly-pizza", "opengameart"];
const uiPath = /[\\/](ui|icons?|hud|gui|sprites?|buttons?)[\\/]/i;

function safeSlug(value) {
  return String(value || "assets").replace(/[^a-zA-Z0-9_.-]+/g, "_").replace(/^_+|_+$/g, "") || "assets";
}

function relPosix(root, abs) {
  return relative(root, abs).replace(/\\/g, "/");
}

function walkSync(dir, shouldSkipDir = null) {
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
    }
    else out.push(path);
  }
  return out;
}

function kindForExt(ext) {
  for (const [kind, exts] of Object.entries(primaryExt)) {
    if (exts.includes(ext)) return kind;
  }
  return null;
}

function assetKind(ext, relPath) {
  const broad = kindForExt(ext);
  if (broad === "image") return uiPath.test(relPath) ? "ui" : "texture";
  return broad;
}

function detectOrigin(relPath) {
  const lower = relPath.toLowerCase();
  if (/[\\/](generated|imagegen|ai[-_]?gen|gen)[\\/]/.test(lower)) return "ai";
  if (/[\\/]source[\\/]/.test(lower) || vendorNames.some((vendor) => lower.includes(vendor))) return "sourced";
  return "unknown";
}

function findLicense(dirFiles) {
  return dirFiles.find((file) => /license|licence/i.test(basename(file))) || "";
}

function jsonList(value) {
  return JSON.stringify(Array.isArray(value) ? value.filter(Boolean) : value ? [value] : []);
}

function parseJsonList(value) {
  if (!value) return [];
  try {
    const parsed = JSON.parse(value);
    return Array.isArray(parsed) ? parsed.filter(Boolean) : [];
  } catch {
    return [];
  }
}

function list(value) {
  return Array.isArray(value) ? value.filter(Boolean) : value ? [value] : [];
}

function uniqueList(...values) {
  return [...new Set(values.flatMap(list).map(String).filter(Boolean))];
}

function hashKey(value) {
  let hash = 2166136261;
  for (const ch of String(value || "")) {
    hash ^= ch.charCodeAt(0);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

function sourceKey(source) {
  return `${source.id}|${source.path}|${source.mode}`;
}

function libraryScanOptionsFromSnapshot(source, snapshot) {
  if (source.mode !== "library" || !snapshot?.files) return {};
  return {
    catalogFiles: snapshot.files
      .filter((file) => file.scope === "catalog")
      .map((file) => join(source.path, "catalog", file.rel)),
    assetFiles: snapshot.files
      .filter((file) => file.scope === "files")
      .map((file) => join(source.path, "files", file.rel)),
  };
}

export async function summarizeIndexedPreviewStatus(root, source) {
  await ensureAssetIndex(root, source);
  const db = openIndex(root, source);
  initSchema(db);
  try {
    const rows = db.prepare(`
      SELECT kind, preview_status AS status, COUNT(*) AS count
      FROM assets
      WHERE source_id = ?
      GROUP BY kind, preview_status
    `).all(source.id);
    const summary = {
      sourceId: source.id,
      total: 0,
      clean: 0,
      missing: 0,
      stale: 0,
      byKind: {},
    };
    for (const row of rows) {
      const status = row.status || "missing";
      const kind = row.kind || "asset";
      const count = Number(row.count) || 0;
      summary.total += count;
      summary[status] = (summary[status] || 0) + count;
      if (!summary.byKind[kind]) summary.byKind[kind] = { clean: 0, missing: 0, stale: 0, total: 0 };
      summary.byKind[kind][status] = (summary.byKind[kind][status] || 0) + count;
      summary.byKind[kind].total += count;
    }
    return summary;
  } finally {
    db.close();
  }
}

function indexDir(root) {
  return join(root, "tmp", "ai_studio", "assets", "asset_index");
}

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

export function assetIndexPath(root, source) {
  return join(indexDir(root), `${safeSlug(source.id)}.sqlite`);
}

function openIndex(root, source) {
  const dbPath = assetIndexPath(root, source);
  mkdirSync(dirname(dbPath), { recursive: true });
  return new DatabaseSync(dbPath);
}

function resetIndexFiles(root, source) {
  const dbPath = assetIndexPath(root, source);
  let removed = true;
  for (const path of [dbPath, `${dbPath}-wal`, `${dbPath}-shm`]) {
    try {
      rmSync(path, { force: true });
    } catch (error) {
      if (error && ["EBUSY", "EPERM", "EACCES"].includes(error.code)) {
        removed = false;
        continue;
      }
      throw error;
    }
  }
  return removed;
}

function initSchema(db, { indexes = true } = {}) {
  db.exec(`
    PRAGMA journal_mode = WAL;
    PRAGMA synchronous = NORMAL;
    PRAGMA busy_timeout = 5000;
    CREATE TABLE IF NOT EXISTS meta (
      key TEXT PRIMARY KEY,
      value TEXT NOT NULL
    );
    CREATE TABLE IF NOT EXISTS assets (
      source_id TEXT NOT NULL,
      id TEXT NOT NULL,
      kind TEXT,
      title TEXT,
      description TEXT,
      pack TEXT,
      origin TEXT,
      license TEXT,
      source TEXT,
      source_asset_id TEXT,
      resource_path TEXT,
      files_dir TEXT,
      preview_path TEXT,
      model_path TEXT,
      catalog_path TEXT,
      catalog_mtime_ms INTEGER,
      catalog_size INTEGER,
      tags_json TEXT,
      genre_json TEXT,
      style_json TEXT,
      random_key INTEGER,
      indexed_at TEXT,
      preview_status TEXT,
      PRIMARY KEY (source_id, id)
    );
    CREATE TABLE IF NOT EXISTS packs (
      source_id TEXT NOT NULL,
      id TEXT NOT NULL,
      title TEXT,
      source TEXT,
      kind TEXT,
      origin TEXT,
      license TEXT,
      license_url TEXT,
      count INTEGER,
      cover TEXT,
      cover_img TEXT,
      description TEXT,
      body TEXT,
      tags_json TEXT,
      genre_json TEXT,
      style_json TEXT,
      PRIMARY KEY (source_id, id)
    );
    CREATE TABLE IF NOT EXISTS asset_terms (
      source_id TEXT NOT NULL,
      asset_id TEXT NOT NULL,
      key TEXT NOT NULL,
      value TEXT NOT NULL
    );
    CREATE TABLE IF NOT EXISTS asset_facet_counts (
      source_id TEXT NOT NULL,
      key TEXT NOT NULL,
      value TEXT NOT NULL,
      count INTEGER NOT NULL,
      PRIMARY KEY (source_id, key, value)
    );
    CREATE TABLE IF NOT EXISTS asset_pack_memberships (
      source_id TEXT NOT NULL,
      asset_id TEXT NOT NULL,
      pack TEXT NOT NULL,
      is_primary INTEGER NOT NULL DEFAULT 0,
      PRIMARY KEY (source_id, asset_id, pack)
    );
    CREATE VIRTUAL TABLE IF NOT EXISTS asset_search_fts USING fts5(
      source_id UNINDEXED,
      asset_id UNINDEXED,
      text
    );
  `);
  if (indexes) createSecondaryIndexes(db);
}

function dropSecondaryIndexes(db) {
  db.exec(`
    DROP INDEX IF EXISTS idx_assets_source_kind;
    DROP INDEX IF EXISTS idx_assets_source_pack;
    DROP INDEX IF EXISTS idx_assets_source_origin;
    DROP INDEX IF EXISTS idx_assets_source_license;
    DROP INDEX IF EXISTS idx_packs_source;
    DROP INDEX IF EXISTS idx_asset_terms_lookup;
    DROP INDEX IF EXISTS idx_asset_terms_asset;
    DROP INDEX IF EXISTS idx_asset_facet_counts_lookup;
    DROP INDEX IF EXISTS idx_asset_pack_memberships_pack;
    DROP INDEX IF EXISTS idx_asset_pack_memberships_asset;
  `);
}

function createSecondaryIndexes(db) {
  db.exec(`
    CREATE INDEX IF NOT EXISTS idx_assets_source_kind ON assets(source_id, kind);
    CREATE INDEX IF NOT EXISTS idx_assets_source_pack ON assets(source_id, pack);
    CREATE INDEX IF NOT EXISTS idx_assets_source_origin ON assets(source_id, origin);
    CREATE INDEX IF NOT EXISTS idx_assets_source_license ON assets(source_id, license);
    CREATE INDEX IF NOT EXISTS idx_packs_source ON packs(source_id);
    CREATE INDEX IF NOT EXISTS idx_asset_terms_lookup ON asset_terms(source_id, key, value);
    CREATE INDEX IF NOT EXISTS idx_asset_terms_asset ON asset_terms(source_id, asset_id);
    CREATE INDEX IF NOT EXISTS idx_asset_facet_counts_lookup ON asset_facet_counts(source_id, key);
    CREATE INDEX IF NOT EXISTS idx_asset_pack_memberships_pack ON asset_pack_memberships(source_id, pack, asset_id);
    CREATE INDEX IF NOT EXISTS idx_asset_pack_memberships_asset ON asset_pack_memberships(source_id, asset_id);
  `);
}

function readMetaValue(db, key) {
  return db.prepare("SELECT value FROM meta WHERE key = ?").get(key)?.value || "";
}

function readIndexCounts(db, source) {
  return {
    assetCount: db.prepare("SELECT COUNT(*) AS count FROM assets WHERE source_id = ?").get(source.id)?.count || 0,
    packCount: db.prepare("SELECT COUNT(*) AS count FROM packs WHERE source_id = ?").get(source.id)?.count || 0,
  };
}

function modelPathIn(filesDir) {
  if (!filesDir || !existsSync(filesDir)) return "";
  const file = readdirSync(filesDir).find((name) => /\.(glb|gltf)$/i.test(name));
  return file ? join(filesDir, file) : "";
}

function catalogStats(path) {
  if (!path || !existsSync(path)) return { mtimeMs: 0, size: 0 };
  const stat = statSync(path);
  return { mtimeMs: Math.round(stat.mtimeMs), size: stat.size };
}

function recordSource(record) {
  return (record.asset_id || "").split("__")[0] || "";
}

function recordPacks(record) {
  return uniqueList(record.pack, record.packs, record.member_of, record.bundles);
}

function recordTerms(record) {
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

function addFacetCounts(counts, terms) {
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
  const scanRoots = source.mode === "library" ? [join(source.path, "files")] : [source.path];
  const shouldSkipDir = covered ? (path) => covered.dirs.has(resolve(path)) : null;
  return scanRoots.flatMap((scanRoot) => walkSync(scanRoot, shouldSkipDir))
    .filter((path) => !/[\\/](catalog|previews|licenses|\.git|node_modules|tmp)[\\/]/i.test(path))
    .filter((path) => !isCoveredPath(path, covered))
    .map((path) => {
      const ext = extname(path).toLowerCase();
      const rel = relPosix(root, path);
      const kind = assetKind(ext, rel);
      if (!kind) return null;

      const dir = dirname(path);
      if (!dirCache.has(dir)) dirCache.set(dir, walkSync(dir));
      const dirFiles = dirCache.get(dir);
      const licenseFile = findLicense(dirFiles);
      const origin = detectOrigin(rel);
      const sourceName = vendorNames.find((vendor) => rel.toLowerCase().includes(vendor)) || "unknown";
      const id = rel.replace(/[\\/]/g, "__");
      const cachedPreview = assetPreviewCachePath(root, source, id);
      return {
        asset_id: id,
        title: basename(path),
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
        preview: cachedPreview || (imageExt.has(ext) ? path : ""),
        modelPath: glbExt.has(ext) ? path : "",
        catalogPath: path,
        sourceName,
      };
    })
    .filter(Boolean);
}

function registeredCoveredPaths(root, source, registeredRecords) {
  const files = new Set();
  const dirs = new Set();
  const addFile = (path) => {
    if (!path) return;
    files.add(resolve(path));
  };
  const addDir = (path) => {
    if (!path) return;
    dirs.add(resolve(path));
  };
  const addResource = (base, value) => {
    if (!value) return;
    const full = resolve(base, value);
    if (kindForExt(extname(value).toLowerCase())) addFile(full);
    else addDir(full);
  };
  for (const record of registeredRecords) {
    addFile(record.modelPath);
    addFile(record.preview);
    addFile(record.catalogPath);
    addResource(source.path, record.resource);
    addResource(root, record.resource);
  }
  return { files, dirs };
}

function isCoveredDiscoveredRecord(record, covered) {
  return isCoveredPath(record.catalogPath || "", covered);
}

function mergeRegisteredWithDiscoveredFiles(root, source, registeredRecords) {
  const covered = registeredCoveredPaths(root, source, registeredRecords);
  const unregistered = scanFolderRecords(root, source, covered)
    .map((record) => ({
      ...record,
      origin: "unregistered",
      license: "unknown",
      sourceName: "unregistered",
      tags: [...new Set([...(record.tags || []), "unregistered"])],
    }));
  return [...registeredRecords, ...unregistered];
}

function ftsQuery(value) {
  const terms = String(value || "")
    .toLowerCase()
    .match(/[\p{L}\p{N}_-]+/gu);
  if (!terms || !terms.length) return "";
  return terms.map((term) => `"${term.replace(/"/g, '""')}"*`).join(" AND ");
}

function filterParams(filters = {}) {
  const out = [];
  for (const [key, value] of Object.entries(filters)) {
    const values = Array.isArray(value) ? value : value ? [value] : [];
    for (const item of values.filter(Boolean)) out.push([key, String(item)]);
  }
  return out;
}

function buildWhere({ sourceId, pack = "", query = "", filters = {} } = {}) {
  const clauses = ["a.source_id = ?"];
  const params = [sourceId];
  if (pack) {
    clauses.push(`EXISTS (
      SELECT 1 FROM asset_pack_memberships apm
      WHERE apm.source_id = a.source_id
        AND apm.asset_id = a.id
        AND apm.pack = ?
    )`);
    params.push(pack);
  }

  const fts = ftsQuery(query);
  if (fts) {
    clauses.push(`a.id IN (
      SELECT asset_id FROM asset_search_fts
      WHERE source_id = ? AND asset_search_fts MATCH ?
    )`);
    params.push(sourceId, fts);
  }

  for (const [key, value] of filterParams(filters)) {
    clauses.push(`EXISTS (
      SELECT 1 FROM asset_terms t
      WHERE t.source_id = a.source_id
        AND t.asset_id = a.id
        AND t.key = ?
        AND t.value = ?
    )`);
    params.push(key, value);
  }

  return { where: clauses.join(" AND "), params };
}

function mediaRef(root, sourceRoot, abs) {
  if (!abs) return "";
  const sourceBase = resolve(sourceRoot);
  const repoBase = resolve(root);
  const full = resolve(abs);
  if (full === sourceBase || full.startsWith(sourceBase + sep)) return `lib/${relPosix(sourceBase, full)}`;
  if (full === repoBase || full.startsWith(repoBase + sep)) return `repo/${relPosix(repoBase, full)}`;
  return "";
}

function rowToAsset(row, root, sourceRoot) {
  return {
    id: row.id,
    name: String(row.title || row.id).replace(/\.(glb|gltf|png|ttf)$/i, ""),
    kind: row.kind || "asset",
    origin: row.origin || "",
    pack: row.pack || "",
    primaryPack: row.pack || "",
    packs: [],
    source: row.source || "",
    license: row.license || "",
    tags: parseJsonList(row.tags_json),
    sourceId: row.source_asset_id || "",
    genre: parseJsonList(row.genre_json),
    style: parseJsonList(row.style_json),
    thumb: mediaRef(root, sourceRoot, row.preview_path),
    model: "",
    previewStatus: row.preview_status || "missing",
  };
}

function attachPackMemberships(db, assets, sourceId, activePack = "") {
  if (!assets.length) return assets;
  const ids = assets.map((asset) => asset.id).filter(Boolean);
  if (!ids.length) return assets;
  const placeholders = ids.map(() => "?").join(", ");
  const rows = db.prepare(`
    SELECT asset_id, pack FROM asset_pack_memberships
    WHERE source_id = ? AND asset_id IN (${placeholders})
    ORDER BY asset_id, is_primary DESC, pack COLLATE NOCASE
  `).all(sourceId, ...ids);
  const memberships = new Map();
  for (const row of rows) {
    if (!memberships.has(row.asset_id)) memberships.set(row.asset_id, []);
    memberships.get(row.asset_id).push(row.pack);
  }
  for (const asset of assets) {
    const packs = memberships.get(asset.id) || [];
    asset.packs = packs;
    asset.primaryPack = asset.pack || packs[0] || "";
    if (activePack && packs.includes(activePack)) asset.pack = activePack;
  }
  return assets;
}

function assetFacetValues(asset, key) {
  if (key === "pack") return uniqueList(asset.pack, asset.packs);
  if (key === "tags") return list(asset.tags);
  if (key === "genre") return list(asset.genre);
  if (key === "style") return list(asset.style);
  return list(asset[key]);
}

function facetsFromAssets(assets) {
  const facets = {};
  for (const key of facetKeys) {
    const counts = new Map();
    for (const asset of assets) {
      for (const value of assetFacetValues(asset, key)) {
        counts.set(value, (counts.get(value) || 0) + 1);
      }
    }
    facets[key] = [...counts.entries()]
      .sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]))
      .slice(0, 80)
      .map(([value, count]) => ({ value, count }));
  }
  return facets;
}

function canUsePrecomputedFacets({ pack = "", query = "", filters = {}, offset = 0 } = {}) {
  return !pack && !query && offset === 0 && filterParams(filters).length === 0;
}

function readPrecomputedFacets(db, sourceId) {
  const facets = {};
  for (const key of facetKeys) {
    facets[key] = db.prepare(`
      SELECT value, count
      FROM asset_facet_counts
      WHERE source_id = ? AND key = ?
      ORDER BY count DESC, value COLLATE NOCASE
      LIMIT 80
    `).all(sourceId, key).map((row) => ({ value: row.value, count: row.count }));
  }
  return facets;
}

function packRowToCard(row, root, sourceRoot, coversByPack = new Map()) {
  const covers = coversByPack.get(row.id) || [];
  return {
    pack: row.id,
    title: row.title || row.id,
    source: row.source || "",
    kind: row.kind || "",
    license: row.license || "",
    license_url: row.license_url || "",
    origin: row.origin || "",
    count: Number(row.count) || 0,
    genre: parseJsonList(row.genre_json),
    style: parseJsonList(row.style_json),
    tags: parseJsonList(row.tags_json),
    cover: row.cover || "",
    description: row.description || "",
    body: row.body || "",
    coverImg: mediaRef(root, sourceRoot, row.cover_img),
    covers,
  };
}

function readPackCovers(db, rows, root, sourceRoot, sourceId) {
  const packIds = rows.map((row) => row.id).filter(Boolean);
  if (!packIds.length) return new Map();
  const placeholders = packIds.map(() => "?").join(", ");
  const coverRows = db.prepare(`
    SELECT pack, preview_path FROM (
      SELECT
        apm.pack AS pack,
        a.preview_path AS preview_path,
        ROW_NUMBER() OVER (PARTITION BY apm.pack ORDER BY a.random_key ASC) AS rn
      FROM asset_pack_memberships apm
      JOIN assets a
        ON a.source_id = apm.source_id
       AND a.id = apm.asset_id
      WHERE a.source_id = ?
        AND apm.pack IN (${placeholders})
        AND a.preview_path != ''
    )
    WHERE rn <= 4
    ORDER BY pack COLLATE NOCASE, rn
  `).all(sourceId, ...packIds);
  const coversByPack = new Map();
  for (const row of coverRows) {
    if (!coversByPack.has(row.pack)) coversByPack.set(row.pack, []);
    coversByPack.get(row.pack).push(mediaRef(root, sourceRoot, row.preview_path));
  }
  return coversByPack;
}

export async function rebuildAssetIndex(root, source, options = {}) {
  const profile = process.env.AI_STUDIO_ASSET_INDEX_PROFILE === "1";
  const timings = [];
  let lastTiming = Date.now();
  const mark = (label) => {
    if (!profile) return;
    const now = Date.now();
    timings.push({ label, ms: now - lastTiming });
    lastTiming = now;
  };
  const removedIndexFiles = resetIndexFiles(root, source);
  const db = openIndex(root, source);
  initSchema(db, { indexes: false });
  const now = new Date().toISOString();
  const snapshot = options.snapshot || buildSourceSnapshot(root, source);
  const signature = sourceSnapshotSignature(snapshot);
  mark("snapshot");
  const libraryData = source.mode === "library"
    ? await scanLibraryWithPacks(source.path, libraryScanOptionsFromSnapshot(source, snapshot))
    : { records: [], packs: [] };
  mark("scan");
  const manifestData = source.mode !== "library" && hasPackManifestSource(source.path)
    ? await scanPackManifestSource(source.path)
    : null;
  const records = source.mode === "library"
    ? mergeRegisteredWithDiscoveredFiles(root, source, libraryData.records)
    : manifestData
      ? mergeRegisteredWithDiscoveredFiles(root, source, manifestData.records)
      : mergeRegisteredWithDiscoveredFiles(root, source, []);
  const packs = source.mode === "library" ? libraryData.packs : manifestData ? manifestData.packs : [];
  mark("merge");

  db.exec("BEGIN");
  try {
    if (!removedIndexFiles) dropSecondaryIndexes(db);
    db.prepare("DELETE FROM assets WHERE source_id = ?").run(source.id);
    db.prepare("DELETE FROM packs WHERE source_id = ?").run(source.id);
    db.prepare("DELETE FROM asset_terms WHERE source_id = ?").run(source.id);
    db.prepare("DELETE FROM asset_facet_counts WHERE source_id = ?").run(source.id);
    db.prepare("DELETE FROM asset_pack_memberships WHERE source_id = ?").run(source.id);
    db.prepare("DELETE FROM asset_search_fts WHERE source_id = ?").run(source.id);

    const insertAsset = db.prepare(`
      INSERT INTO assets (
        source_id, id, kind, title, description, pack, origin, license, source,
        source_asset_id, resource_path, files_dir, preview_path, model_path,
        catalog_path, catalog_mtime_ms, catalog_size, tags_json, genre_json,
        style_json, random_key, indexed_at, preview_status
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `);
    const insertTerm = db.prepare("INSERT INTO asset_terms (source_id, asset_id, key, value) VALUES (?, ?, ?, ?)");
    const facetCounts = new Map();
    const insertPackMembership = db.prepare("INSERT OR IGNORE INTO asset_pack_memberships (source_id, asset_id, pack, is_primary) VALUES (?, ?, ?, ?)");
    const insertFts = db.prepare("INSERT INTO asset_search_fts (source_id, asset_id, text) VALUES (?, ?, ?)");
    for (const record of records) {
      if (!record.asset_id) continue;
      const modelPath = record.modelPath || modelPathIn(record.filesDir);
      const stats = catalogStats(record.catalogPath);
      const sourceName = record.sourceName || recordSource(record);
      const cachedPreview = source.mode === "library" && record.preview
        ? ""
        : assetPreviewCachePath(root, source, record.asset_id);
      const previewPath = record.preview || cachedPreview;
      const generatedSourcePath = modelPath || record.catalogPath || "";
      const recordExt = extname(record.catalogPath || record.resource || "").toLowerCase();
      const previewStatus = record.preview && record.preview !== cachedPreview
        ? (source.mode === "library" ? "clean" : "missing")
        : previewPath === cachedPreview
          ? assetPreviewCacheStatus(root, source, record.asset_id, generatedSourcePath)
          : previewPath && source.mode === "library"
            ? "clean"
            : previewPath && !imageExt.has(recordExt)
              ? "clean"
            : "missing";
      insertAsset.run(
        source.id,
        record.asset_id,
        record.kind || "",
        record.title || record.asset_id,
        record.description || "",
        record.pack || "",
        record.origin || "",
        record.license || "",
        sourceName,
        record.source_id || "",
        record.resource || "",
        record.filesDir || "",
        previewPath,
        modelPath,
        record.catalogPath || "",
        stats.mtimeMs,
        stats.size,
        jsonList(record.tags),
        jsonList(record.genre),
        jsonList(record.style),
        hashKey(record.asset_id),
        now,
        previewStatus,
      );
      const terms = recordTerms(record);
      addFacetCounts(facetCounts, terms);
      for (const [key, value] of terms) insertTerm.run(source.id, record.asset_id, key, value);
      for (const pack of recordPacks(record)) insertPackMembership.run(source.id, record.asset_id, pack, pack === record.pack ? 1 : 0);
      insertFts.run(source.id, record.asset_id, [
        record.title,
        record.asset_id,
        record.description,
        record.pack,
        recordPacks(record).join(" "),
        record.kind,
        record.license,
        record.origin,
        sourceName,
        (record.tags || []).join(" "),
        (record.genre || []).join(" "),
        (record.style || []).join(" "),
      ].join(" "));
    }

    const insertFacetCount = db.prepare("INSERT INTO asset_facet_counts (source_id, key, value, count) VALUES (?, ?, ?, ?)");
    for (const facet of facetCounts.values()) {
      insertFacetCount.run(source.id, facet.key, facet.value, facet.count);
    }

    const countByPack = new Map();
    for (const record of records) {
      for (const pack of recordPacks(record)) {
        countByPack.set(pack, (countByPack.get(pack) || 0) + 1);
      }
    }

    const insertPack = db.prepare(`
      INSERT INTO packs (
        source_id, id, title, source, kind, origin, license, license_url, count,
        cover, cover_img, description, body, tags_json, genre_json, style_json
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `);
    for (const pack of packs) {
      if (!pack.pack) continue;
      let coverImg = "";
      if (/\.(png|jpg|jpeg|webp)$/i.test(pack.cover || "")) {
        const candidate = join(source.path, pack.cover);
        if (existsSync(candidate)) coverImg = candidate;
      }
      if (!coverImg) {
        const candidate = join(source.path, "previews", pack.pack, "cover.png");
        if (existsSync(candidate)) coverImg = candidate;
      }
      insertPack.run(
        source.id,
        pack.pack,
        pack.title || pack.pack,
        pack.source || "",
        pack.kind || "",
        pack.origin || "",
        pack.license || "",
        pack.license_url || "",
        countByPack.get(pack.pack) || Number(pack.count) || 0,
        pack.cover || "",
        coverImg,
        pack.description || "",
        pack.body || "",
        jsonList(pack.tags),
        jsonList(pack.genre),
        jsonList(pack.style),
      );
    }

    db.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)").run("schema", String(schemaVersion));
    db.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)").run("sourceKey", sourceKey(source));
    db.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)").run("sourceSignature", JSON.stringify(signature));
    db.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)").run("rebuiltAt", now);
    db.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)").run("assetCount", String(records.length));
    createSecondaryIndexes(db);
    db.exec("COMMIT");
    mark("sqlite");
  } catch (error) {
    db.exec("ROLLBACK");
    throw error;
  } finally {
    db.close();
  }
  writeSourceSnapshot(root, source, snapshot);
  if (profile) console.error(`[asset-index profile] ${JSON.stringify(timings)}`);

  return {
    sourceId: source.id,
    rebuiltAt: now,
    assetCount: records.length,
    packCount: packs.length,
    path: assetIndexPath(root, source),
    snapshotDiff: options.snapshotDiff || null,
  };
}

export async function refreshAssetIndex(root, source) {
  const dbPath = assetIndexPath(root, source);
  if (!existsSync(dbPath)) return rebuildAssetIndex(root, source);

  const db = openIndex(root, source);
  initSchema(db);
  let closed = false;
  try {
    const schema = readMetaValue(db, "schema");
    const key = readMetaValue(db, "sourceKey");
    const previousSignature = readMetaValue(db, "sourceSignature");
    const rebuiltAt = readMetaValue(db, "rebuiltAt");
    const counts = readIndexCounts(db, source);
    const previousSnapshot = readSourceSnapshot(root, source);
    const currentSnapshot = buildSourceSnapshot(root, source);
    const currentSignature = sourceSnapshotSignature(currentSnapshot);
    if (
      schema === String(schemaVersion)
      && key === sourceKey(source)
      && counts.assetCount
      && previousSignature === JSON.stringify(currentSignature)
    ) {
      if (!previousSnapshot) writeSourceSnapshot(root, source, currentSnapshot);
      return {
        sourceId: source.id,
        rebuiltAt,
        assetCount: counts.assetCount,
        packCount: counts.packCount,
        path: dbPath,
        unchanged: true,
      };
    }
    db.close();
    closed = true;
    return rebuildAssetIndex(root, source, {
      snapshot: currentSnapshot,
      snapshotDiff: diffSourceSnapshots(previousSnapshot, currentSnapshot),
    });
  } finally {
    if (!closed) db.close();
  }
}

export async function ensureAssetIndex(root, source) {
  const dbPath = assetIndexPath(root, source);
  if (!existsSync(dbPath)) return rebuildAssetIndex(root, source);
  const db = openIndex(root, source);
  initSchema(db);
  let closed = false;
  try {
    const schema = readMetaValue(db, "schema");
    const key = readMetaValue(db, "sourceKey");
    const count = db.prepare("SELECT COUNT(*) AS count FROM assets WHERE source_id = ?").get(source.id)?.count || 0;
    if (schema !== String(schemaVersion) || key !== sourceKey(source) || !count) {
      db.close();
      closed = true;
      return rebuildAssetIndex(root, source);
    }
    const rebuiltAt = readMetaValue(db, "rebuiltAt");
    return { sourceId: source.id, rebuiltAt, assetCount: count, path: dbPath };
  } finally {
    if (!closed) db.close();
  }
}

export async function listIndexedPacks(root, source) {
  await ensureAssetIndex(root, source);
  const db = openIndex(root, source);
  initSchema(db);
  try {
    const rows = db.prepare("SELECT * FROM packs WHERE source_id = ? ORDER BY title COLLATE NOCASE").all(source.id);
    const coversByPack = readPackCovers(db, rows, root, source.path, source.id);
    return rows.map((row) => packRowToCard(row, root, source.path, coversByPack));
  } finally {
    db.close();
  }
}

export async function queryIndexedAssets(root, source, options = {}) {
  await ensureAssetIndex(root, source);
  const db = openIndex(root, source);
  initSchema(db);
  const offset = Math.max(0, Number.parseInt(String(options.offset || 0), 10) || 0);
  const limit = Math.min(500, Math.max(1, Number.parseInt(String(options.limit || 240), 10) || 240));
  const query = options.query ?? options.q ?? "";
  const { where, params } = buildWhere({ sourceId: source.id, ...options, query });
  const order = options.sort === "origin"
    ? "a.origin COLLATE NOCASE, a.title COLLATE NOCASE, a.id"
    : options.sort === "random"
      ? "a.random_key, a.id"
      : "a.title COLLATE NOCASE, a.id";

  try {
    const total = db.prepare(`SELECT COUNT(*) AS count FROM assets a WHERE ${where}`).get(...params)?.count || 0;
    const rows = db.prepare(`SELECT a.* FROM assets a WHERE ${where} ORDER BY ${order} LIMIT ? OFFSET ?`).all(...params, limit, offset);
    const assets = attachPackMemberships(db, rows.map((row) => rowToAsset(row, root, source.path)), source.id, options.pack || "");

    const facets = canUsePrecomputedFacets({ ...options, query, offset })
      ? readPrecomputedFacets(db, source.id)
      : total <= limit && offset === 0
        ? facetsFromAssets(assets)
        : {};
    if (!Object.keys(facets).length) {
      for (const key of facetKeys) {
        facets[key] = db.prepare(`
          SELECT t.value AS value, COUNT(*) AS count
          FROM asset_terms t
          WHERE t.source_id = ?
            AND t.key = ?
            AND t.asset_id IN (SELECT a.id FROM assets a WHERE ${where})
          GROUP BY t.value
          ORDER BY count DESC, value COLLATE NOCASE
          LIMIT 80
        `).all(source.id, key, ...params).map((row) => ({ value: row.value, count: row.count }));
      }
    }

    return {
      sourceId: source.id,
      pack: options.pack || "",
      offset,
      limit,
      total,
      facets,
      assets,
    };
  } finally {
    db.close();
  }
}

export async function resolveIndexedModel(root, source, assetId) {
  await ensureAssetIndex(root, source);
  const db = openIndex(root, source);
  initSchema(db);
  try {
    const row = db.prepare("SELECT id, model_path FROM assets WHERE source_id = ? AND id = ?").get(source.id, assetId);
    if (!row || !row.model_path) throw new Error(`model file is missing for: ${assetId}`);
    return {
      id: row.id,
      model: mediaRef(root, source.path, row.model_path),
    };
  } finally {
    db.close();
  }
}

export function safeResolveIndexPath(root, source, relativePath) {
  const base = resolve(source.path);
  const full = resolve(base, relativePath);
  if (full !== base && !full.startsWith(base + sep)) return null;
  return full;
}
