// Game Page ops: read-only aggregation of one game's studio-visible state.
// The page never mutates game data; every section links into the owning tool.
import { existsSync, readFileSync, readdirSync, realpathSync, statSync } from "node:fs";
import { gzipSync } from "node:zlib";
import { basename, extname, join, normalize, resolve, sep } from "node:path";

import { listGameMounts } from "../workspace/games.mjs";
import { listProjects } from "../taskboard/store.mjs";
import { entryDetail, parseNameHeader, parseNtpack } from "./ntpack.mjs";

// Layout contract: everything the page knows about a game folder, in one
// place. These are template conventions, not per-game knowledge — owners:
// build/release layout comes from the template's game tools, generated name
// headers from nt_builder, captures from runtime_automation's capture
// workflow. A game that diverges simply shows an empty section.
const GAME_LAYOUT = {
  designDir: "design",
  buildDir: "build",
  releaseArtifactsDir: "release/artifacts",
  generatedNamesDir: "src/generated",
  stateDir: "state",
  capturesDir: "tmp/captures",
};

// Front-of-list design docs; every other design/*.md follows alphabetically.
const DESIGN_DOC_ORDER = ["README.md", "gdd.md", "concept.md"];

function comparable(value) {
  return String(value || "").trim().toLowerCase();
}

export function listGames(root) {
  const warnings = [];
  const mounts = listGameMounts(root, { includePrivate: true, warnings });
  const games = mounts
    .map((mount) => ({
      id: mount.gameId,
      title: mount.title,
      visibility: mount.visibility,
      root: mount.root,
      storeId: mount.storeId,
    }))
    .sort((a, b) => a.title.localeCompare(b.title) || a.id.localeCompare(b.id));
  return { schema: "ai_studio.game_page.games.v1", games, warnings };
}

// The workspace scan parses every identity manifest; one page load fires
// dozens of /game-file requests, so mounts are cached for a short TTL.
const mountCache = { at: 0, root: "", mounts: null };
const MOUNT_CACHE_TTL_MS = 2000;

function cachedGameMounts(root) {
  const now = Date.now();
  if (mountCache.mounts && mountCache.root === root && now - mountCache.at < MOUNT_CACHE_TTL_MS) {
    return mountCache.mounts;
  }
  const mounts = listGameMounts(root, { includePrivate: true, warnings: [] });
  mountCache.at = now;
  mountCache.root = root;
  mountCache.mounts = mounts;
  return mounts;
}

export function resolveGameMount(root, gameId) {
  const wanted = comparable(gameId);
  if (!wanted) return null;
  const mounts = cachedGameMounts(root);
  return mounts.find((mount) =>
    [mount.gameId, mount.storageNamespace, mount.storeId, ...(mount.aliases || [])]
      .some((alias) => comparable(alias) === wanted)) || null;
}

function designDocs(gameRoot) {
  const docs = safeFiles(join(gameRoot, GAME_LAYOUT.designDir))
    .filter((entry) => entry.name.toLowerCase().endsWith(".md"))
    .map((entry) => ({
      rel: `${GAME_LAYOUT.designDir}/${entry.name}`,
      label: entry.name === "README.md" ? "Design home" : entry.name.replace(/\.md$/i, "").replace(/_/g, " "),
    }));
  const rank = (doc) => {
    const at = DESIGN_DOC_ORDER.indexOf(basename(doc.rel));
    return at < 0 ? DESIGN_DOC_ORDER.length : at;
  };
  return docs.sort((a, b) => rank(a) - rank(b) || a.rel.localeCompare(b.rel));
}

function projectRow(doc) {
  const fields = doc.fields || {};
  return {
    id: String(fields.id || ""),
    title: String(fields.title || ""),
    status: String(fields.status || ""),
    store: doc.storeId || "",
  };
}

// A game's project can live in the studio store (target points at the game
// root) or in the game's own private taskboard store.
function taskboardProjects(root, mount) {
  const rows = [];
  const wantedTargets = new Set([comparable(mount.root), comparable(mount.gameId)]);
  for (const doc of safeProjects(root, {})) {
    if (wantedTargets.has(comparable(doc.fields?.target))) rows.push(projectRow({ ...doc, storeId: "studio" }));
  }
  const gameItemsRoot = join(mount.root, ".ai_studio", "taskboard", "items");
  if (existsSync(join(root, gameItemsRoot))) {
    for (const doc of safeProjects(root, { itemsRoot: gameItemsRoot })) {
      rows.push(projectRow({ ...doc, storeId: mount.storeId }));
    }
  }
  return rows;
}

function safeProjects(root, options) {
  try {
    return listProjects(root, options);
  } catch {
    return [];
  }
}

// Shipping payload inside a config's bin/; CMake infrastructure stays out of
// the report by only listing configs that carry one of these. A file with no
// extension is kept too (Linux executables).
const BIN_EXTENSIONS = new Set([".exe", ".wasm", ".js", ".html", ".data"]);

// gz sizes are estimates for serving cost; keyed by path+mtime+size so a
// rebuilt artifact is re-measured and an unchanged one is free.
const gzSizeCache = new Map();

// A file can vanish between readdir and stat (active capture writes); a
// missing row is skipped, never a 500 for the whole section.
function fileRow(base, rel) {
  try {
    const stats = statSync(join(base, rel));
    return { rel, bytes: stats.size, mtimeMs: Math.round(stats.mtimeMs) };
  } catch {
    return null;
  }
}

function newestMtime(rows) {
  return rows.reduce((max, row) => Math.max(max, row.mtimeMs), 0);
}

function safeDirs(path) {
  try {
    return readdirSync(path, { withFileTypes: true })
      .filter((entry) => entry.isDirectory() && !entry.name.startsWith("."));
  } catch {
    return [];
  }
}

function safeFiles(path) {
  try {
    return readdirSync(path, { withFileTypes: true }).filter((entry) => entry.isFile());
  } catch {
    return [];
  }
}

function gzSize(path, row) {
  const key = `${path}\0${row.mtimeMs}\0${row.bytes}`;
  if (!gzSizeCache.has(key)) {
    try {
      gzSizeCache.set(key, gzipSync(readFileSync(path)).length);
    } catch {
      gzSizeCache.set(key, null);
    }
    if (gzSizeCache.size > 256) gzSizeCache.delete(gzSizeCache.keys().next().value);
  }
  return gzSizeCache.get(key);
}

// Packs live wherever the config's tooling put them (bin/assets and pack/ in
// the template); the same pack often exists in both places, so rows dedupe by
// basename+size and only unique payload is reported.
function configPacks(configRoot) {
  const rows = [];
  const seen = new Set();
  const walk = (rel, depth) => {
    for (const entry of safeFiles(join(configRoot, rel))) {
      if (extname(entry.name).toLowerCase() !== ".ntpack") continue;
      const row = fileRow(configRoot, rel ? `${rel}/${entry.name}` : entry.name);
      if (!row) continue;
      const key = `${entry.name}:${row.bytes}`;
      if (seen.has(key)) continue;
      seen.add(key);
      rows.push(row);
    }
    if (depth <= 0) return;
    for (const dir of safeDirs(join(configRoot, rel))) {
      walk(rel ? `${rel}/${dir.name}` : dir.name, depth - 1);
    }
  };
  walk("", 2);
  return rows;
}

function buildConfig(gameRoot, name) {
  const configRoot = join(gameRoot, GAME_LAYOUT.buildDir, name);
  const packs = configPacks(configRoot);
  const binFiles = safeFiles(join(configRoot, "bin"))
    .filter((entry) => {
      const extension = extname(entry.name).toLowerCase();
      return extension === "" || BIN_EXTENSIONS.has(extension);
    })
    .map((entry) => fileRow(configRoot, `bin/${entry.name}`))
    .filter(Boolean);
  if (!packs.length && !binFiles.length) return null;
  const web = binFiles.some((row) => row.rel.endsWith(".wasm"));
  if (web) {
    for (const row of binFiles) row.gzBytes = gzSize(join(configRoot, row.rel), row);
  }
  return {
    name,
    web,
    freshnessMs: newestMtime([...packs, ...binFiles]),
    packs,
    binFiles,
  };
}

function releaseArtifacts(gameRoot) {
  const artifactsRoot = join(gameRoot, GAME_LAYOUT.releaseArtifactsDir);
  const rows = [];
  for (const entry of safeFiles(artifactsRoot)) {
    if (!entry.name.endsWith(".manifest.json")) continue;
    try {
      const manifest = JSON.parse(readFileSync(join(artifactsRoot, entry.name), "utf8"));
      const file = String(manifest.artifact?.file || "");
      const row = {
        manifest: entry.name,
        target: String(manifest.target || ""),
        file,
        bytes: Number(manifest.artifact?.size) || 0,
        mtimeMs: 0,
        present: false,
      };
      if (file && existsSync(join(artifactsRoot, file))) {
        row.present = true;
        row.mtimeMs = Math.round(statSync(join(artifactsRoot, file)).mtimeMs);
      }
      rows.push(row);
    } catch {
      rows.push({ manifest: entry.name, target: "", file: "", bytes: 0, mtimeMs: 0, present: false, malformed: true });
    }
  }
  rows.sort((a, b) => b.mtimeMs - a.mtimeMs || a.manifest.localeCompare(b.manifest));
  return rows;
}

export function getGameBuilds(root, gameId) {
  const mount = resolveGameMount(root, gameId);
  if (!mount) return null;
  const gameRoot = join(root, mount.root);
  const configs = safeDirs(join(gameRoot, GAME_LAYOUT.buildDir))
    .filter((entry) => !entry.name.startsWith("_"))
    .map((entry) => buildConfig(gameRoot, entry.name))
    .filter(Boolean)
    .sort((a, b) => b.freshnessMs - a.freshnessMs);
  return {
    schema: "ai_studio.game_page.builds.v1",
    game: { id: mount.gameId, root: mount.root },
    configs,
    release: releaseArtifacts(gameRoot),
  };
}

// Prefix check plus realpath: a junction inside the game folder (heavy
// workfiles live on synced drives in this studio) must not leak files from
// outside the repository.
export function confinedGamePath(root, mount, relPath) {
  const gameRoot = resolve(join(root, mount.root));
  const full = resolve(gameRoot, normalize(String(relPath || "")));
  if (full !== gameRoot && !full.startsWith(gameRoot + sep)) return null;
  try {
    const realFull = realpathSync.native(full);
    const realRoot = realpathSync.native(gameRoot);
    if (realFull !== realRoot && !realFull.startsWith(realRoot + sep)) return null;
    return realFull;
  } catch {
    return null;
  }
}

function packContext(root, gameId, relPath) {
  const mount = resolveGameMount(root, gameId);
  if (!mount) return null;
  const full = confinedGamePath(root, mount, relPath);
  if (!full || extname(full).toLowerCase() !== ".ntpack" || !existsSync(full)) return null;
  const stem = basename(full, ".ntpack");
  const namesFromRel = `${GAME_LAYOUT.generatedNamesDir}/${stem}.h`;
  const nameHeaderPath = join(root, mount.root, namesFromRel);
  const names = existsSync(nameHeaderPath) ? parseNameHeader(readFileSync(nameHeaderPath, "utf8")) : new Map();
  return { mount, full, names, nameHeaderPath, namesFromRel };
}

// One cache for everything derived from a pack file: the raw buffer (entry
// data requests) and the parsed dump (tables and entry lookups). Keyed by
// pack AND name-header identity so a rebuild or regenerated names refresh it.
const packCache = new Map();

function packCacheKey(context) {
  let nameStamp = "none";
  try {
    const stats = statSync(context.nameHeaderPath);
    nameStamp = `${Math.round(stats.mtimeMs)}:${stats.size}`;
  } catch {
    // keep "none"
  }
  const stats = statSync(context.full);
  return `${context.full}\0${Math.round(stats.mtimeMs)}\0${stats.size}\0${nameStamp}`;
}

function loadPack(context) {
  const cacheKey = packCacheKey(context);
  if (!packCache.has(cacheKey)) {
    const buffer = readFileSync(context.full);
    const stats = statSync(context.full);
    let dump;
    try {
      const parsed = parseNtpack(buffer, { names: context.names });
      dump = {
        schema: "ai_studio.game_page.pack.v1",
        game: { id: context.mount.gameId, root: context.mount.root },
        pack: {
          rel: "",
          bytes: stats.size,
          mtimeMs: Math.round(stats.mtimeMs),
          namesFrom: existsSync(context.nameHeaderPath) ? context.namesFromRel : "",
        },
        ...parsed,
      };
    } catch (error) {
      dump = {
        schema: "ai_studio.game_page.pack.v1",
        game: { id: context.mount.gameId, root: context.mount.root },
        pack: { rel: "", bytes: stats.size, mtimeMs: Math.round(stats.mtimeMs) },
        error: error?.message || String(error),
      };
    }
    packCache.set(cacheKey, { buffer, dump });
    if (packCache.size > 4) packCache.delete(packCache.keys().next().value);
  }
  return packCache.get(cacheKey);
}

export function getPackDump(root, gameId, relPath) {
  const context = packContext(root, gameId, relPath);
  if (!context) return null;
  const { dump } = loadPack(context);
  return { ...dump, pack: { ...dump.pack, rel: String(relPath) } };
}

export function getPackEntryDetail(root, gameId, relPath, index) {
  const context = packContext(root, gameId, relPath);
  if (!context) return null;
  const { buffer, dump } = loadPack(context);
  const entry = dump.entries?.[Number(index)];
  if (!entry) return null;
  const detail = entryDetail(buffer, entry, context.names);
  return {
    schema: "ai_studio.game_page.pack_entry.v1",
    entry,
    ...(detail || { kind: "invalid" }),
  };
}

export function getPackEntryData(root, gameId, relPath, index) {
  const context = packContext(root, gameId, relPath);
  if (!context) return null;
  const { buffer, dump } = loadPack(context);
  const entry = dump.entries?.[Number(index)];
  if (!entry || !entry.inBounds) return null;
  return {
    entry,
    bytes: buffer.subarray(entry.offset, entry.offset + entry.size),
  };
}

// Native saves live outside the repo, in the engine's per-user storage root:
// <LOCALAPPDATA>/neotolis/<app-id>/saves (game_storage_backend_native.c). The
// app id matches the game id for template-derived games. A game launched with
// --save-root writes elsewhere and simply is not visible here.
function defaultSavesRoot(gameId, options = {}) {
  if (options.storageRoot) return join(options.storageRoot, gameId, "saves");
  const localAppData = process.env.LOCALAPPDATA;
  if (!localAppData) return null;
  return join(localAppData, "neotolis", gameId, "saves");
}

const SAVE_SLOT_NAME = /^[\w.-]+\.(json|bak)$/;

export function getGameSaves(root, gameId, options = {}) {
  const mount = resolveGameMount(root, gameId);
  if (!mount) return null;
  const savesRoot = defaultSavesRoot(mount.gameId, options);
  const slots = [];
  if (savesRoot) {
    for (const entry of safeFiles(savesRoot)) {
      if (!SAVE_SLOT_NAME.test(entry.name)) continue;
      const row = fileRow(savesRoot, entry.name);
      if (!row) continue;
      const slot = { slot: entry.name, bytes: row.bytes, mtimeMs: row.mtimeMs };
      try {
        const parsed = JSON.parse(readFileSync(join(savesRoot, entry.name), "utf8"));
        slot.meta = {
          saveVersion: parsed.save_version ?? null,
          savedAt: Number(parsed.saved_at) || 0,
          saveSeq: parsed.save_seq ?? null,
          app: String(parsed.app || ""),
        };
      } catch {
        slot.malformed = true;
      }
      slots.push(slot);
    }
  }
  slots.sort((a, b) => b.mtimeMs - a.mtimeMs);
  return {
    schema: "ai_studio.game_page.saves.v1",
    game: { id: mount.gameId },
    savesRoot: savesRoot || "",
    slots,
  };
}

export function getGameSaveContent(root, gameId, slot, options = {}) {
  const mount = resolveGameMount(root, gameId);
  if (!mount) return null;
  const name = String(slot || "");
  if (!SAVE_SLOT_NAME.test(name)) return null;
  const savesRoot = defaultSavesRoot(mount.gameId, options);
  if (!savesRoot || !existsSync(join(savesRoot, name))) return null;
  try {
    return {
      schema: "ai_studio.game_page.save.v1",
      game: { id: mount.gameId },
      slot: name,
      content: JSON.parse(readFileSync(join(savesRoot, name), "utf8")),
    };
  } catch (error) {
    return {
      schema: "ai_studio.game_page.save.v1",
      game: { id: mount.gameId },
      slot: name,
      error: error?.message || String(error),
    };
  }
}

export function getGameStateSchemas(root, gameId) {
  const mount = resolveGameMount(root, gameId);
  if (!mount) return null;
  const stateRoot = join(root, mount.root, GAME_LAYOUT.stateDir);
  const schemas = safeFiles(stateRoot)
    .filter((entry) => entry.name.endsWith(".schema.json"))
    .map((entry) => fileRow(join(root, mount.root), `${GAME_LAYOUT.stateDir}/${entry.name}`))
    .filter(Boolean)
    .sort((a, b) => a.rel.localeCompare(b.rel));
  return { schema: "ai_studio.game_page.state.v1", game: { id: mount.gameId }, schemas };
}

const CAPTURE_SESSION_LIMIT = 100;
const CAPTURE_MEDIA = new Set([".png", ".mp4", ".mkv", ".webm", ".jpg", ".webp"]);

// A capture "take" is any folder (up to 3 levels under the captures root)
// holding a capture.json or media files: the capture workflow writes
// <shot>/<session>/<stage>/, ad-hoc recorders write flatter paths. Dot-folders
// (recorder state) are skipped by safeDirs.
function captureTakeRows(gameRoot) {
  const rows = [];
  const walk = (rel, depth) => {
    const files = safeFiles(join(gameRoot, rel))
      .map((entry) => fileRow(gameRoot, `${rel}/${entry.name}`))
      .filter(Boolean);
    const isTake = files.some((file) =>
      file.rel.endsWith("/capture.json") || CAPTURE_MEDIA.has(extname(file.rel).toLowerCase()));
    if (isTake) {
      const named = (name) => files.find((file) => file.rel.endsWith(`/${name}`));
      const parts = rel.split("/").slice(2);
      rows.push({
        label: parts.join(" · "),
        rel,
        mtimeMs: newestMtime(files),
        bytes: files.reduce((sum, file) => sum + file.bytes, 0),
        fileCount: files.length,
        previewRel: named("representative-frame.png")?.rel
          || files.find((file) => [".png", ".jpg", ".webp"].includes(extname(file.rel).toLowerCase()))?.rel
          || "",
        videoRel: (named("edit.mp4")
          || files.find((file) => [".mp4", ".webm", ".mkv"].includes(extname(file.rel).toLowerCase())))?.rel || "",
      });
    }
    if (depth <= 0) return;
    for (const dir of safeDirs(join(gameRoot, rel))) walk(`${rel}/${dir.name}`, depth - 1);
  };
  for (const dir of safeDirs(join(gameRoot, GAME_LAYOUT.capturesDir))) {
    walk(`${GAME_LAYOUT.capturesDir}/${dir.name}`, 2);
  }
  return rows;
}

export function getGameCaptures(root, gameId) {
  const mount = resolveGameMount(root, gameId);
  if (!mount) return null;
  const rows = captureTakeRows(join(root, mount.root));
  rows.sort((a, b) => b.mtimeMs - a.mtimeMs);
  return {
    schema: "ai_studio.game_page.captures.v1",
    game: { id: mount.gameId },
    truncated: rows.length > CAPTURE_SESSION_LIMIT,
    sessions: rows.slice(0, CAPTURE_SESSION_LIMIT),
  };
}

export function getGameOverview(root, gameId) {
  const mount = resolveGameMount(root, gameId);
  if (!mount) return null;
  const gameRoot = join(root, mount.root);
  let identity = null;
  try {
    identity = JSON.parse(readFileSync(join(gameRoot, "game.json"), "utf8"));
  } catch {
    identity = { id: mount.gameId, title: mount.title };
  }
  return {
    schema: "ai_studio.game_page.overview.v1",
    game: {
      id: mount.gameId,
      title: mount.title,
      visibility: mount.visibility,
      root: mount.root,
      storeId: mount.storeId,
      version: String(identity.version || ""),
    },
    designDocs: designDocs(gameRoot),
    taskboardProjects: taskboardProjects(root, mount),
  };
}
