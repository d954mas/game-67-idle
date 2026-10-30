import { execFile } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, realpathSync, writeFileSync } from "node:fs";
import { basename, dirname, join, normalize, relative, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { listIndexedPacks, queryIndexedAssets, refreshAssetIndex, reindexAssetRecord, resolveIndexedAssetManifest, resolveIndexedModel } from "../catalog/ops.mjs";
import { updatePackAssetRecord } from "../manifests/ops.mjs";
import { refreshPreviewCache } from "../previews/ops.mjs";
import { listRegisteredLibraries, listRegisteredTemplates, resolveRegisteredSourcePath } from "../sources/ops.mjs";
import { listGameMounts } from "../../workspace/games.mjs";

const here = fileURLToPath(new URL(".", import.meta.url));
const builder = join(here, "build_review.mjs");
const galleryRootName = "ai-studio-asset-viewer";
const galleryMetaName = ".asset-viewer-source.json";
const viewerCache = new Map();
const viewerInflight = new Map();
const preloadedRoots = new Set();
const viewerCacheTtlMs = 5 * 60 * 1000;

function safeSlug(value) {
  return String(value || "asset-viewer").replace(/[^a-zA-Z0-9_.-]+/g, "_").replace(/^_+|_+$/g, "") || "asset-viewer";
}

export function safeResolve(base, relativePath) {
  const resolvedBase = resolve(base);
  if (/^[A-Za-z]:[/\\]/.test(relativePath) && !/^[A-Za-z]:[/\\]/.test(resolvedBase)) return null;
  const full = resolve(resolvedBase, normalize(relativePath));
  if (full !== resolvedBase && !full.startsWith(resolvedBase + sep)) return null;
  return full;
}

function servedPathInside(base, relativePath) {
  const full = safeResolve(base, relativePath);
  if (!full) return null;

  let physicalBase;
  try {
    physicalBase = realpathSync.native(resolve(base));
  } catch {
    return null;
  }

  // A missing file still needs its lexical path so the static handler can
  // report 404, but an existing ancestor must remain inside the real root.
  let ancestor = full;
  while (!existsSync(ancestor)) {
    const parent = dirname(ancestor);
    if (parent === ancestor) return full;
    ancestor = parent;
  }

  try {
    const physicalAncestor = realpathSync.native(ancestor);
    if (physicalAncestor !== physicalBase && !physicalAncestor.startsWith(physicalBase + sep)) return null;
  } catch {
    return null;
  }
  return full;
}

function galleryBase(root) {
  return join(root, "tmp", galleryRootName);
}

function sourceAvailable(path) {
  return Boolean(path && existsSync(path));
}

function registeredGameSources(root, options = {}) {
  return listGameMounts(root, { includePrivate: options.includePrivate === true })
    .filter((game) => game.enabledStores.includes("assets"))
    .map((game) => {
    const assetsPath = safeResolve(root, game.assetRoot);
    return {
      id: game.storeId,
      type: "game",
      label: game.publicAlias || game.title || game.gameId,
      description: game.visibility === "public" ? "Registered game-local assets folder." : "Private game-local assets folder.",
      path: assetsPath || "",
      available: sourceAvailable(assetsPath),
      visibility: game.visibility,
    };
    });
}

function registeredLibrarySources(root) {
  return listRegisteredLibraries(root)
    .filter((library) => library.status !== "disabled")
    .map((library) => {
      const assetsPath = resolveRegisteredSourcePath(root, library.assets);
      return {
        id: library.id,
        type: "library",
        label: library.title || library.id,
        description: "Shared reusable asset storage.",
        path: assetsPath || "",
        available: sourceAvailable(assetsPath),
      };
    });
}

function registeredTemplateSources(root) {
  return listRegisteredTemplates(root).map((template) => {
    const assetsPath = safeResolve(root, template.assets);
    return {
      id: template.id,
      type: "template",
      label: template.title || template.id,
      description: "Registered template asset folder.",
      path: assetsPath || "",
      available: sourceAvailable(assetsPath),
    };
  });
}

export async function listAssetViewerSources(root, options = {}) {
  const sources = registeredLibrarySources(root);
  sources.push(...registeredTemplateSources(root));
  sources.push(...registeredGameSources(root, options));
  return {
    sources,
  };
}

function prefixRelativeUrl(value, galleryUrl) {
  if (!value || /^(https?:)?\/\//i.test(value) || value.startsWith("/") || value.startsWith("data:")) return value;
  return `${galleryUrl}${value}`;
}

function prefixViewerMedia(viewer, galleryUrl) {
  for (const asset of viewer.assets || []) {
    asset.thumb = prefixRelativeUrl(asset.thumb, galleryUrl);
    asset.model = prefixRelativeUrl(asset.model, galleryUrl);
    asset.audio = prefixRelativeUrl(asset.audio, galleryUrl);
  }
  for (const pack of viewer.packs || []) {
    pack.coverImg = prefixRelativeUrl(pack.coverImg, galleryUrl);
    pack.covers = (pack.covers || []).map((cover) => prefixRelativeUrl(cover, galleryUrl));
  }
  return viewer;
}

function cacheKeyForSource(source) {
  return `${source.id}|${source.path}|${source.type}`;
}

function readCache(key) {
  const cached = viewerCache.get(key);
  if (!cached) return null;
  if (Date.now() - cached.createdAt > viewerCacheTtlMs) {
    viewerCache.delete(key);
    return null;
  }
  return cached.payload;
}

function writeCache(key, payload) {
  viewerCache.set(key, { createdAt: Date.now(), payload });
  return payload;
}

function writeGalleryMeta(root, source) {
  const slug = safeSlug(source.id);
  const outDir = join(galleryBase(root), slug);
  mkdirSync(outDir, { recursive: true });
  const meta = {
    sourceId: source.id,
    sourceType: source.type,
    sourceLabel: source.label,
    sourcePath: source.path,
    generatedAt: new Date().toISOString(),
    libraryRoot: source.path,
  };
  writeFileSync(join(outDir, galleryMetaName), JSON.stringify(meta, null, 2), "utf8");
  return { outDir, galleryUrl: `/asset_viewer/gallery/${slug}/`, meta };
}

function readGalleryMeta(metaPath) {
  try {
    return JSON.parse(readFileSync(metaPath, "utf8"));
  } catch {
    return null;
  }
}

function readAssetFilters(url) {
  const filters = new Map();
  for (const [key, value] of url.searchParams.entries()) {
    if (!key.startsWith("filter.")) continue;
    const name = key.slice("filter.".length);
    if (!filters.has(name)) filters.set(name, []);
    filters.get(name).push(value);
  }
  return filters;
}

function readJsonBody(req) {
  return new Promise((resolveBody, rejectBody) => {
    let data = "";
    req.setEncoding("utf8");
    req.on("data", (chunk) => {
      data += chunk;
      if (data.length > 1024 * 1024) {
        rejectBody(new Error("request body too large"));
        req.destroy();
      }
    });
    req.on("end", () => {
      try {
        resolveBody(data ? JSON.parse(data) : {});
      } catch {
        rejectBody(new Error("invalid JSON body"));
      }
    });
    req.on("error", rejectBody);
  });
}

function runBuilder(args, cwd) {
  return new Promise((resolveRun, rejectRun) => {
    execFile(process.execPath, [builder, ...args], { cwd }, (error, stdout, stderr) => {
      if (error) {
        rejectRun(new Error((stderr || stdout || error.message).trim()));
        return;
      }
      resolveRun(stdout);
    });
  });
}

function boolValue(value) {
  return value === true || value === "true" || value === "1" || value === "yes";
}

function gameSourceFromMount(root, mount) {
  const assetsPath = safeResolve(root, mount.assetRoot);
  return {
    id: mount.storeId,
    type: "game",
    label: mount.publicAlias || mount.title || mount.gameId,
    description: mount.visibility === "public" ? "Registered game-local assets folder." : "Private game-local assets folder.",
    path: assetsPath || "",
    available: sourceAvailable(assetsPath),
    visibility: mount.visibility,
  };
}

function resolveGameSource(root, gameId, body = {}) {
  const id = String(gameId || "").trim();
  if (!id) throw new Error("missing game asset source id");
  const mounts = listGameMounts(root, {
    activeGameId: id,
    includePrivate: boolValue(body.includePrivate) || boolValue(body["include-private"]),
  });
  const mount = mounts.find((item) => item.gameId === id);
  if (!mount) throw new Error(`unknown game asset source: ${id}`);
  return gameSourceFromMount(root, mount);
}

function pathInside(base, candidate) {
  const resolvedBase = resolve(base);
  const resolvedCandidate = resolve(candidate);
  return resolvedCandidate === resolvedBase || resolvedCandidate.startsWith(resolvedBase + sep);
}

function isPrivateMountedAssetPath(root, abs) {
  let mounts = [];
  try {
    mounts = listGameMounts(root, { includePrivate: true, skipPreflight: true });
  } catch {
    return false;
  }
  return mounts
    .filter((mount) => mount.visibility !== "public")
    .some((mount) => pathInside(resolve(root, mount.root), abs) || pathInside(resolve(root, mount.assetRoot), abs));
}

export function selectSource(sources, body, root) {
  const byId = new Map(sources.map((source) => [source.id, source]));
  if (body.sourceId && byId.has(body.sourceId)) return byId.get(body.sourceId);
  if (body.game) return resolveGameSource(root, body.game, body);
  if (body.sourceId && String(body.sourceId).startsWith("game:")) {
    return resolveGameSource(root, String(body.sourceId).slice("game:".length), body);
  }

  if (body.type === "game" && body.path) {
    const abs = safeResolve(root, body.path);
    if (!abs) throw new Error("game assets path must stay inside the repository");
    if (isPrivateMountedAssetPath(root, abs)) {
      throw new Error("private game assets must be selected by game id, not by raw path");
    }
    return {
      id: `game:${safeSlug(body.path)}`,
      type: "game",
      label: basename(abs) === "assets" ? basename(resolve(abs, "..")) : basename(abs),
      description: "Custom game-local assets folder.",
      path: abs,
      available: sourceAvailable(abs),
    };
  }

  throw new Error("unknown asset source");
}

function sourceListOptions(body = {}) {
  return {
    includePrivate: boolValue(body.includePrivate) || boolValue(body["include-private"]),
  };
}

export async function buildAssetViewerGallery(root, body) {
  const { sources } = await listAssetViewerSources(root, sourceListOptions(body));
  const source = selectSource(sources, body, root);
  if (!source.available) throw new Error(`asset source is not available: ${source.path}`);

  const slug = safeSlug(source.id);
  const outDir = join(galleryBase(root), slug);
  mkdirSync(outDir, { recursive: true });

  const args = ["--out", outDir];
  const meta = {
    sourceId: source.id,
    sourceType: source.type,
    sourceLabel: source.label,
    sourcePath: source.path,
    generatedAt: new Date().toISOString(),
  };

  if (source.type === "library") {
    args.push("--mode", "library", "--library", source.path, "--ref");
    meta.libraryRoot = source.path;
  } else {
    args.push("--mode", "scan", "--path", source.path, "--repo", root, "--game", source.label);
  }

  const stdout = await runBuilder(args, root);
  writeFileSync(join(outDir, galleryMetaName), JSON.stringify(meta, null, 2), "utf8");

  let result = {};
  try {
    result = JSON.parse(stdout);
  } catch {
    result = { html: join(outDir, "index.html") };
  }

  return {
    ...result,
    source,
    url: `/asset_viewer/gallery/${slug}/`,
  };
}

export async function openAssetViewerSource(root, body) {
  const { sources } = await listAssetViewerSources(root, sourceListOptions(body));
  const source = selectSource(sources, body, root);
  if (!source.available) throw new Error(`asset source is not available: ${source.path}`);

  const key = cacheKeyForSource(source);
  const cached = readCache(key);
  if (cached) return cached;
  const inflight = viewerInflight.get(key);
  if (inflight) return inflight;

  const promise = buildOpenAssetViewerPayload(root, source, key);
  viewerInflight.set(key, promise);
  try {
    return await promise;
  } finally {
    viewerInflight.delete(key);
  }
}

export async function reindexAssetViewerSource(root, body) {
  const { sources } = await listAssetViewerSources(root, sourceListOptions(body));
  const source = selectSource(sources, body, root);
  if (!source.available) throw new Error(`asset source is not available: ${source.path}`);
  viewerCache.delete(cacheKeyForSource(source));

  const result = await refreshAssetIndex(root, source);
  return {
    source,
    refresh: {
      mode: "index",
      assetCount: result.assetCount,
      packCount: result.packCount,
      unchanged: Boolean(result.unchanged),
    },
  };
}

export const refreshAssetViewerSource = reindexAssetViewerSource;

export async function refreshAssetViewerPreviews(root, body) {
  const { sources } = await listAssetViewerSources(root, sourceListOptions(body));
  const source = selectSource(sources, body, root);
  if (!source.available) throw new Error(`asset source is not available: ${source.path}`);
  await refreshAssetIndex(root, source);
  const result = await refreshPreviewCache(root, source, { force: Boolean(body.force) });
  viewerCache.delete(cacheKeyForSource(source));
  return { source, previews: result };
}

async function buildOpenAssetViewerPayload(root, source, key) {
  const { galleryUrl } = writeGalleryMeta(root, source);
  const packs = await listIndexedPacks(root, source);
  const viewer = prefixViewerMedia({ assets: [], packs, opts: { title: source.label } }, galleryUrl);
  viewer.opts = {
    ...(viewer.opts || {}),
    title: source.label,
    sourceId: source.id,
    sourceType: source.type,
    sourcePath: source.path,
    assetsLazy: true,
  };

  return writeCache(key, {
    source,
    viewer,
    galleryUrl,
  });
}

export function preloadAssetViewerDefaults(root) {
  if (preloadedRoots.has(root)) return;
  preloadedRoots.add(root);
  setTimeout(() => {
    openAssetViewerSource(root, { sourceId: "global-library" }).catch(() => {
      // Preload is an optimization only. The foreground request reports errors.
    });
  }, 0);
}

function filtersObject(filters) {
  const out = {};
  for (const [key, values] of filters.entries()) out[key] = values;
  return out;
}

function writeJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(data, null, 2));
}

export function createAssetViewerApi(root) {
  preloadAssetViewerDefaults(root);
  return async function handleAssetViewerApi(req, res, url) {
    try {
      if (req.method === "GET" && url.pathname === "/api/asset-viewer/sources") {
        writeJson(res, 200, await listAssetViewerSources(root, {
          includePrivate: boolValue(url.searchParams.get("include-private")) || boolValue(url.searchParams.get("includePrivate")),
        }));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-viewer/build") {
        const body = await readJsonBody(req);
        writeJson(res, 200, await buildAssetViewerGallery(root, body));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-viewer/open") {
        const body = await readJsonBody(req);
        writeJson(res, 200, await openAssetViewerSource(root, body));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-viewer/reindex") {
        const body = await readJsonBody(req);
        writeJson(res, 200, await reindexAssetViewerSource(root, body));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-viewer/refresh") {
        const body = await readJsonBody(req);
        writeJson(res, 200, await refreshAssetViewerSource(root, body));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-viewer/previews/refresh") {
        const body = await readJsonBody(req);
        writeJson(res, 200, await refreshAssetViewerPreviews(root, body));
        return true;
      }
      if (req.method === "GET" && url.pathname === "/api/asset-viewer/model") {
        writeJson(res, 200, await resolveAssetViewerModel(root, url));
        return true;
      }
      if (req.method === "GET" && url.pathname === "/api/asset-viewer/assets") {
        writeJson(res, 200, await resolveAssetViewerAssets(root, url));
        return true;
      }
      if (req.method === "GET" && url.pathname === "/api/asset-viewer/tag-proposals") {
        writeJson(res, 200, readTagProposals(root, url.searchParams.get("pack") || ""));
        return true;
      }
      if (req.method === "POST" && url.pathname === "/api/asset-viewer/asset-review") {
        assertSameOriginJson(req);
        const body = await readJsonBody(req);
        writeJson(res, 200, await saveAssetReview(root, body));
        return true;
      }
      return false;
    } catch (error) {
      writeJson(res, 400, { error: error.message });
      return true;
    }
  };
}

export async function resolveAssetViewerAssets(root, url) {
  const sourceId = url.searchParams.get("sourceId") || "global-library";
  const pack = url.searchParams.get("pack") || "";
  const query = url.searchParams.get("q") || "";
  const sort = url.searchParams.get("sort") || "name";
  const offset = Math.max(0, Number.parseInt(url.searchParams.get("offset") || "0", 10) || 0);
  const limit = Math.min(500, Math.max(24, Number.parseInt(url.searchParams.get("limit") || "240", 10) || 240));
  const filters = readAssetFilters(url);

  const { sources } = await listAssetViewerSources(root, {
    includePrivate: boolValue(url.searchParams.get("include-private")) || boolValue(url.searchParams.get("includePrivate")),
  });
  const source = selectSource(sources, { sourceId }, root);

  writeGalleryMeta(root, source);
  const galleryUrl = `/asset_viewer/gallery/${safeSlug(source.id)}/`;
  const payload = await queryIndexedAssets(root, source, {
    pack,
    query,
    sort,
    offset,
    limit,
    filters: filtersObject(filters),
  });
  payload.assets = prefixViewerMedia({ assets: payload.assets, packs: [] }, galleryUrl).assets;
  return payload;
}

// Writes reach the Pack Manifest, so a page on another origin must not be able to
// trigger them: a JSON content type forces a CORS preflight the shell never grants.
function assertSameOriginJson(req) {
  if (!String(req.headers["content-type"] || "").toLowerCase().startsWith("application/json")) {
    throw new Error("asset review requires a JSON request");
  }
  const origin = req.headers.origin;
  if (origin && origin !== `http://${req.headers.host}`) throw new Error("asset review is same-origin only");
}

function reviewTags(value) {
  if (!Array.isArray(value)) throw new Error("tags must be a list");
  const tags = [];
  for (const raw of value) {
    const tag = String(raw).trim().toLowerCase().replace(/\s+/g, "-");
    if (tag && !tags.includes(tag)) tags.push(tag);
  }
  return tags;
}

// Saves a lead's tag review: the row in assets.jsonl is the source of truth, then
// only that asset is re-read into the index so the viewer shows it at once.
export async function saveAssetReview(root, body) {
  const assetId = String(body.id || "");
  if (!assetId) throw new Error("missing asset id");
  const { sources } = await listAssetViewerSources(root, { includePrivate: boolValue(body.includePrivate) });
  const source = selectSource(sources, { sourceId: body.sourceId || "global-library" }, root);
  const { manifest } = await resolveIndexedAssetManifest(root, source, assetId);
  // The viewer shows pack tags merged in; they stay on the pack, not on the row.
  const packPath = join(dirname(manifest), "pack.json");
  const packTags = new Set(existsSync(packPath) ? JSON.parse(readFileSync(packPath, "utf8").replace(/^﻿/, "")).tags || [] : []);
  const patch = { tags: reviewTags(body.tags).filter((tag) => !packTags.has(tag)) };
  if (typeof body.title === "string" && body.title.trim()) patch.title = body.title.trim();
  await updatePackAssetRecord(manifest, assetId, patch);
  return reindexAssetRecord(root, source, assetId);
}

// Tag proposals are tool output under tmp/, one JSON per model run, named
// <pack>.json or <pack>.<model>.json. They are suggestions, never asset metadata.
export function readTagProposals(root, pack) {
  const out = {};
  if (!pack || pack !== safeSlug(pack)) return out;
  const dir = join(root, "tmp", "ai_studio", "assets", "audio_tags");
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir).sort()) {
    if (!(name === `${pack}.json` || (name.startsWith(`${pack}.`) && name.endsWith(".json")))) continue;
    let run;
    try {
      run = JSON.parse(readFileSync(join(dir, name), "utf8"));
    } catch {
      continue;
    }
    const model = run.model || name;
    for (const asset of run.assets || []) {
      if (!asset.asset_id) continue;
      (out[asset.asset_id] ||= []).push({
        model,
        summary: asset.summary || "",
        description: asset.description || "",
        tags: asset.tags || [],
      });
    }
  }
  return out;
}

export async function resolveAssetViewerModel(root, url) {
  const sourceId = url.searchParams.get("sourceId") || "global-library";
  const assetId = url.searchParams.get("id") || "";
  if (!assetId) throw new Error("missing model asset id");

  const { sources } = await listAssetViewerSources(root, {
    includePrivate: boolValue(url.searchParams.get("include-private")) || boolValue(url.searchParams.get("includePrivate")),
  });
  const source = selectSource(sources, { sourceId }, root);

  writeGalleryMeta(root, source);
  const result = await resolveIndexedModel(root, source, assetId);
  return {
    id: result.id,
    model: `/asset_viewer/gallery/${safeSlug(source.id)}/${result.model}`,
  };
}

export function resolveAssetViewerGalleryPath(root, pathname) {
  const prefix = "/asset_viewer/gallery/";
  if (!pathname.startsWith(prefix)) return null;

  const rest = pathname.slice(prefix.length);
  const [slug, ...parts] = rest.split("/").filter(Boolean);
  if (!slug) return null;

  const galleryRoot = servedPathInside(root, relative(root, galleryBase(root)));
  if (!galleryRoot) return null;
  const galleryDir = servedPathInside(galleryRoot, safeSlug(slug));
  if (!galleryDir) return null;
  const metaPath = join(galleryDir, galleryMetaName);
  const relativeParts = parts.length ? parts : ["index.html"];

  if (relativeParts[0] === "lib") {
    if (!existsSync(metaPath)) return null;
    const meta = readGalleryMeta(metaPath);
    if (!meta?.libraryRoot) return null;
    return servedPathInside(meta.libraryRoot, relativeParts.slice(1).join("/"));
  }
  if (relativeParts[0] === "repo") {
    return servedPathInside(root, relativeParts.slice(1).join("/"));
  }

  return servedPathInside(galleryDir, relativeParts.join("/"));
}
