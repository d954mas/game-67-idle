// Game Page ops: read-only aggregation of one game's studio-visible state.
// The page never mutates game data; every section links into the owning tool.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

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
