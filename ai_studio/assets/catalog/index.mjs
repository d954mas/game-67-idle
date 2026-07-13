import { existsSync } from "node:fs";
import { extname, join } from "node:path";
import { decideLicense } from "../licenses/ops.mjs";
import { assetPreviewCachePath, assetPreviewCacheStatus } from "../previews/status.mjs";
import {
  buildSourceSnapshot,
  diffSourceSnapshots,
  readSourceSnapshot,
  sourceSnapshotSignature,
  writeSourceSnapshot,
} from "./snapshots/snapshots.mjs";
import {
  addFacetCounts,
  mergeRegisteredWithDiscoveredFiles,
  metadataStats,
  readRegisteredSourceData,
  recordPacks,
  recordSource,
  recordTerms,
} from "./source_records.mjs";
import {
  assetIndexPath,
  createSecondaryIndexes,
  dropSecondaryIndexes,
  initSchema,
  openIndex,
  readIndexCounts,
  readMetaValue,
  resetIndexFiles,
  schemaVersion,
} from "./store.mjs";
import { hashKey, imageExt, jsonList, modelPathIn, sourceKey } from "./shared.mjs";

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
  const sourceData = await readRegisteredSourceData(root, source);
  mark("scan");
  const records = mergeRegisteredWithDiscoveredFiles(root, source, sourceData.records || []);
  const packs = sourceData.packs || [];
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
        source_id, id, kind, title, description, pack, origin, license,
        license_url, license_kind, attribution_required, notice_required,
        credit_text, source_page, author_vendor, source,
        source_asset_id, resource_path, files_dir, preview_path, model_path,
        metadata_path, metadata_mtime_ms, metadata_size, tags_json, genre_json,
        style_json, random_key, indexed_at, preview_status
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    `);
    const insertTerm = db.prepare("INSERT INTO asset_terms (source_id, asset_id, key, value) VALUES (?, ?, ?, ?)");
    const facetCounts = new Map();
    const insertPackMembership = db.prepare("INSERT OR IGNORE INTO asset_pack_memberships (source_id, asset_id, pack, is_primary) VALUES (?, ?, ?, ?)");
    const insertFts = db.prepare("INSERT INTO asset_search_fts (source_id, asset_id, text) VALUES (?, ?, ?)");
    for (const record of records) {
      if (!record.asset_id) continue;
      const licenseDecision = decideLicense(record);
      const modelPath = record.modelPath || modelPathIn(record.filesDir);
      const stats = metadataStats(record.metadataPath);
      const sourceName = record.sourceName || recordSource(record);
      const cachedPreview = record.preview ? "" : assetPreviewCachePath(root, source, record.asset_id);
      const previewPath = record.preview || cachedPreview;
      const generatedSourcePath = modelPath || record.metadataPath || "";
      const recordExt = extname(record.metadataPath || record.resource || "").toLowerCase();
      const previewStatus = record.preview && record.preview !== cachedPreview
        ? "clean"
        : previewPath === cachedPreview
          ? assetPreviewCacheStatus(root, source, record.asset_id, generatedSourcePath)
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
        record.license_url || licenseDecision.licenseUrl || "",
        record.license_kind || licenseDecision.licenseKind || "",
        record.attribution_required || (licenseDecision.attributionRequired ? "true" : "false"),
        record.notice_required || (licenseDecision.noticeRequired ? "true" : "false"),
        record.credit_text || record.credit || record.attribution || "",
        record.source_page || "",
        record.author_vendor || record.author || "",
        sourceName,
        record.source_id || "",
        record.resource || "",
        record.filesDir || "",
        previewPath,
        modelPath,
        record.metadataPath || "",
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
    for (const facet of facetCounts.values()) insertFacetCount.run(source.id, facet.key, facet.value, facet.count);
    const countByPack = new Map();
    for (const record of records) {
      for (const pack of recordPacks(record)) countByPack.set(pack, (countByPack.get(pack) || 0) + 1);
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
    if (schema === String(schemaVersion)
      && key === sourceKey(source)
      && counts.assetCount
      && previousSignature === JSON.stringify(currentSignature)) {
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
