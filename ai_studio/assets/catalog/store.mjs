import { DatabaseSync } from "node:sqlite";
import { mkdirSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { safeSlug } from "./shared.mjs";

export const schemaVersion = 8;

function indexDir(root) {
  return join(root, "tmp", "ai_studio", "assets", "index");
}

export function assetIndexPath(root, source) {
  return join(indexDir(root), `${safeSlug(source.id)}.sqlite`);
}

export function openIndex(root, source) {
  const dbPath = assetIndexPath(root, source);
  mkdirSync(dirname(dbPath), { recursive: true });
  return new DatabaseSync(dbPath);
}

export function resetIndexFiles(root, source) {
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

export function initSchema(db, { indexes = true } = {}) {
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
      license_url TEXT,
      license_kind TEXT,
      attribution_required TEXT,
      notice_required TEXT,
      credit_text TEXT,
      source_page TEXT,
      author_vendor TEXT,
      source TEXT,
      source_asset_id TEXT,
      resource_path TEXT,
      files_dir TEXT,
      preview_path TEXT,
      model_path TEXT,
      metadata_path TEXT,
      metadata_mtime_ms INTEGER,
      metadata_size INTEGER,
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

export function dropSecondaryIndexes(db) {
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

export function createSecondaryIndexes(db) {
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

export function readMetaValue(db, key) {
  return db.prepare("SELECT value FROM meta WHERE key = ?").get(key)?.value || "";
}

export function readIndexCounts(db, source) {
  return {
    assetCount: db.prepare("SELECT COUNT(*) AS count FROM assets WHERE source_id = ?").get(source.id)?.count || 0,
    packCount: db.prepare("SELECT COUNT(*) AS count FROM packs WHERE source_id = ?").get(source.id)?.count || 0,
  };
}
