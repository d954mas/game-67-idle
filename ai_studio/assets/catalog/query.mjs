import { resolve, sep } from "node:path";
import { ensureAssetIndex } from "./index.mjs";
import { initSchema, openIndex } from "./store.mjs";
import { facetKeys, list, parseJsonList, relPosix, uniqueList } from "./shared.mjs";

function ftsQuery(value) {
  const terms = String(value || "").toLowerCase().match(/[\p{L}\p{N}_]+/gu);
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
    license_url: row.license_url || "",
    licenseKind: row.license_kind || "",
    attributionRequired: row.attribution_required || "",
    noticeRequired: row.notice_required || "",
    creditText: row.credit_text || "",
    sourcePage: row.source_page || "",
    authorVendor: row.author_vendor || "",
    tags: parseJsonList(row.tags_json),
    sourceId: row.source_asset_id || "",
    genre: parseJsonList(row.genre_json),
    style: parseJsonList(row.style_json),
    relpath: row.resource_path || "",
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
      for (const value of assetFacetValues(asset, key)) counts.set(value, (counts.get(value) || 0) + 1);
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
    const summary = { sourceId: source.id, total: 0, clean: 0, missing: 0, stale: 0, byKind: {} };
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
    return { sourceId: source.id, pack: options.pack || "", offset, limit, total, facets, assets };
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
    return { id: row.id, model: mediaRef(root, source.path, row.model_path) };
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
