import { DatabaseSync } from "node:sqlite";
import { existsSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { basename, dirname, extname, join, relative, resolve, sep } from "node:path";
import { scanLibrary, scanPacks } from "../okf_catalog/find_assets.mjs";

const schemaVersion = 1;
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

function indexDir(root) {
  return join(root, "tmp", "ai_studio", "assets", "asset_index");
}

function previewCacheDir(root, source) {
  return join(root, "tmp", "ai_studio", "assets", "previews", safeSlug(source.id));
}

export function assetPreviewCachePath(root, source, assetId) {
  const dir = join(previewCacheDir(root, source), safeSlug(assetId));
  for (const ext of [".webp", ".png", ".jpg", ".jpeg", ".gif"]) {
    const candidate = join(dir, "preview" + ext);
    if (existsSync(candidate)) return candidate;
  }
  return "";
}

export function assetIndexPath(root, source) {
  return join(indexDir(root), `${safeSlug(source.id)}.sqlite`);
}

function openIndex(root, source) {
  const dbPath = assetIndexPath(root, source);
  mkdirSync(dirname(dbPath), { recursive: true });
  return new DatabaseSync(dbPath);
}

function initSchema(db) {
  db.exec(`
    PRAGMA journal_mode = WAL;
    PRAGMA synchronous = NORMAL;
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
    CREATE INDEX IF NOT EXISTS idx_assets_source_kind ON assets(source_id, kind);
    CREATE INDEX IF NOT EXISTS idx_assets_source_pack ON assets(source_id, pack);
    CREATE INDEX IF NOT EXISTS idx_assets_source_origin ON assets(source_id, origin);
    CREATE INDEX IF NOT EXISTS idx_assets_source_license ON assets(source_id, license);
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
    CREATE INDEX IF NOT EXISTS idx_packs_source ON packs(source_id);
    CREATE TABLE IF NOT EXISTS asset_terms (
      source_id TEXT NOT NULL,
      asset_id TEXT NOT NULL,
      key TEXT NOT NULL,
      value TEXT NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_asset_terms_lookup ON asset_terms(source_id, key, value);
    CREATE INDEX IF NOT EXISTS idx_asset_terms_asset ON asset_terms(source_id, asset_id);
    CREATE VIRTUAL TABLE IF NOT EXISTS asset_search_fts USING fts5(
      source_id UNINDEXED,
      asset_id UNINDEXED,
      text
    );
  `);
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
  push("pack", record.pack);
  push("genre", record.genre);
  push("style", record.style);
  push("tags", record.tags);
  return terms;
}

function scanFolderRecords(root, source) {
  const dirCache = new Map();
  return walkSync(source.path)
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
    clauses.push("a.pack = ?");
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

function packRowToCard(db, row, root, sourceRoot, sourceId) {
  const coverRows = db.prepare(`
    SELECT preview_path FROM assets
    WHERE source_id = ? AND pack = ? AND preview_path != ''
    ORDER BY random_key ASC
    LIMIT 4
  `).all(sourceId, row.id);
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
    covers: coverRows.map((cover) => mediaRef(root, sourceRoot, cover.preview_path)).filter(Boolean),
  };
}

export async function rebuildAssetIndex(root, source) {
  const db = openIndex(root, source);
  initSchema(db);
  const now = new Date().toISOString();
  const records = source.mode === "library" ? await scanLibrary(source.path) : scanFolderRecords(root, source);
  const packs = source.mode === "library" ? await scanPacks(source.path) : [];

  db.exec("BEGIN");
  try {
    db.prepare("DELETE FROM assets WHERE source_id = ?").run(source.id);
    db.prepare("DELETE FROM packs WHERE source_id = ?").run(source.id);
    db.prepare("DELETE FROM asset_terms WHERE source_id = ?").run(source.id);
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
    const insertFts = db.prepare("INSERT INTO asset_search_fts (source_id, asset_id, text) VALUES (?, ?, ?)");
    for (const record of records) {
      if (!record.asset_id) continue;
      const modelPath = record.modelPath || modelPathIn(record.filesDir);
      const stats = catalogStats(record.catalogPath);
      const sourceName = record.sourceName || recordSource(record);
      const previewPath = record.preview || assetPreviewCachePath(root, source, record.asset_id);
      const previewStatus = previewPath ? "clean" : "missing";
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
      for (const [key, value] of recordTerms(record)) insertTerm.run(source.id, record.asset_id, key, value);
      insertFts.run(source.id, record.asset_id, [
        record.title,
        record.asset_id,
        record.description,
        record.pack,
        record.kind,
        record.license,
        record.origin,
        sourceName,
        (record.tags || []).join(" "),
        (record.genre || []).join(" "),
        (record.style || []).join(" "),
      ].join(" "));
    }

    const countByPack = new Map();
    for (const record of records) {
      if (!record.pack) continue;
      countByPack.set(record.pack, (countByPack.get(record.pack) || 0) + 1);
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
    db.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)").run("rebuiltAt", now);
    db.prepare("INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)").run("assetCount", String(records.length));
    db.exec("COMMIT");
  } catch (error) {
    db.exec("ROLLBACK");
    throw error;
  } finally {
    db.close();
  }

  return { sourceId: source.id, rebuiltAt: now, assetCount: records.length, packCount: packs.length, path: assetIndexPath(root, source) };
}

export async function ensureAssetIndex(root, source) {
  const dbPath = assetIndexPath(root, source);
  if (!existsSync(dbPath)) return rebuildAssetIndex(root, source);
  const db = openIndex(root, source);
  initSchema(db);
  let closed = false;
  try {
    const schema = db.prepare("SELECT value FROM meta WHERE key = ?").get("schema")?.value;
    const key = db.prepare("SELECT value FROM meta WHERE key = ?").get("sourceKey")?.value;
    const count = db.prepare("SELECT COUNT(*) AS count FROM assets WHERE source_id = ?").get(source.id)?.count || 0;
    if (schema !== String(schemaVersion) || key !== sourceKey(source) || !count) {
      db.close();
      closed = true;
      return rebuildAssetIndex(root, source);
    }
    const rebuiltAt = db.prepare("SELECT value FROM meta WHERE key = ?").get("rebuiltAt")?.value || "";
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
    return rows.map((row) => packRowToCard(db, row, root, source.path, source.id));
  } finally {
    db.close();
  }
}

export async function queryIndexedAssets(root, source, options = {}) {
  await ensureAssetIndex(root, source);
  const db = openIndex(root, source);
  initSchema(db);
  const offset = Math.max(0, Number.parseInt(String(options.offset || 0), 10) || 0);
  const limit = Math.min(500, Math.max(24, Number.parseInt(String(options.limit || 240), 10) || 240));
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
    const assets = rows.map((row) => rowToAsset(row, root, source.path));

    const facets = {};
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
