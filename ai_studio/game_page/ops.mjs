// Game Page ops: read-only aggregation of one game's studio-visible state.
// The page never mutates game data; every section links into the owning tool.
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { gzipSync } from "node:zlib";
import { extname, join } from "node:path";

import { listGameMounts } from "../workspace/games.mjs";
import { listProjects } from "../taskboard/store.mjs";

const DESIGN_DOC_CANDIDATES = [
  { rel: "design/README.md", label: "Design home" },
  { rel: "design/gdd.md", label: "GDD" },
  { rel: "design/concept.md", label: "Concept" },
  { rel: "design/art_contract.md", label: "Art contract" },
];

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

export function resolveGameMount(root, gameId) {
  const wanted = comparable(gameId);
  if (!wanted) return null;
  const mounts = listGameMounts(root, { includePrivate: true, warnings: [] });
  return mounts.find((mount) =>
    [mount.gameId, mount.storageNamespace, mount.storeId, ...(mount.aliases || [])]
      .some((alias) => comparable(alias) === wanted)) || null;
}

function designDocs(gameRoot) {
  return DESIGN_DOC_CANDIDATES
    .filter((doc) => existsSync(join(gameRoot, doc.rel)))
    .map((doc) => ({ ...doc }));
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

// Shipping payload extensions inside a config's bin/; CMake infrastructure
// stays out of the report by only listing configs that carry one of these.
const BIN_EXTENSIONS = new Set([".exe", ".wasm", ".js", ".html", ".data"]);

// gz sizes are estimates for serving cost; keyed by path+mtime+size so a
// rebuilt artifact is re-measured and an unchanged one is free.
const gzSizeCache = new Map();

function fileRow(base, rel) {
  const stats = statSync(join(base, rel));
  return { rel, bytes: stats.size, mtimeMs: Math.round(stats.mtimeMs) };
}

function safeDirs(path) {
  try {
    return readdirSync(path, { withFileTypes: true }).filter((entry) => entry.isDirectory());
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
  }
  return gzSizeCache.get(key);
}

function buildConfig(gameRoot, name) {
  const configRoot = join(gameRoot, "build", name);
  const packs = [];
  for (const packDir of ["bin/assets", "pack"]) {
    for (const entry of safeFiles(join(configRoot, packDir))) {
      if (extname(entry.name).toLowerCase() !== ".ntpack") continue;
      packs.push(fileRow(configRoot, `${packDir}/${entry.name}`));
    }
  }
  const binFiles = safeFiles(join(configRoot, "bin"))
    .filter((entry) => BIN_EXTENSIONS.has(extname(entry.name).toLowerCase()))
    .map((entry) => fileRow(configRoot, `bin/${entry.name}`));
  if (!packs.length && !binFiles.length) return null;
  const web = binFiles.some((row) => row.rel.endsWith(".wasm"));
  if (web) {
    for (const row of binFiles) row.gzBytes = gzSize(join(configRoot, row.rel), row);
  }
  const rows = [...packs, ...binFiles];
  return {
    name,
    web,
    freshnessMs: Math.max(...rows.map((row) => row.mtimeMs)),
    packs,
    binFiles,
  };
}

function releaseArtifacts(gameRoot) {
  const artifactsRoot = join(gameRoot, "release", "artifacts");
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
  const configs = safeDirs(join(gameRoot, "build"))
    .filter((entry) => !entry.name.startsWith(".") && !entry.name.startsWith("_"))
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
    links: {
      taskboard: "/taskboard/",
      assetViewer: `/asset_viewer/?sourceId=${encodeURIComponent(mount.storeId)}&include-private=1`,
    },
  };
}
