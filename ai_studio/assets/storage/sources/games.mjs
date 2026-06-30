import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";

const defaultRegistry = {
  schema: "ai_studio.assets.games.v1",
  games: [],
};

function registryPath(root) {
  return join(root, "ai_studio", "assets", "storage", "sources", "games.json");
}

function normalizeRelPath(value) {
  return String(value || "").replace(/\\/g, "/").replace(/^\.?\//, "").replace(/\/+$/, "");
}

function readRegistry(root) {
  const path = registryPath(root);
  if (!existsSync(path)) return { ...defaultRegistry, games: [] };
  const parsed = JSON.parse(readFileSync(path, "utf8"));
  return {
    schema: parsed.schema || defaultRegistry.schema,
    games: Array.isArray(parsed.games) ? parsed.games : [],
  };
}

function writeRegistry(root, registry) {
  const path = registryPath(root);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(registry, null, 2)}\n`, "utf8");
}

export function listRegisteredGames(root) {
  const registry = readRegistry(root);
  return registry.games
    .filter((game) => game && game.id && game.assets)
    .map((game) => ({
      id: String(game.id),
      title: String(game.title || game.id),
      folder: normalizeRelPath(game.folder || game.id),
      assets: normalizeRelPath(game.assets),
      status: String(game.status || "active"),
    }))
    .sort((a, b) => a.id.localeCompare(b.id));
}

export function registerGameAssetSource(root, { id, title = "", folder = "", assets = "", status = "active" }) {
  const gameId = String(id || "").trim();
  if (!/^[a-z][a-z0-9-]*$/.test(gameId)) {
    throw new Error("game id must be lowercase kebab-case");
  }

  const relFolder = normalizeRelPath(folder || gameId);
  const relAssets = normalizeRelPath(assets || `${relFolder}/assets`);
  const registry = readRegistry(root);
  const next = {
    id: gameId,
    title: String(title || gameId),
    folder: relFolder,
    assets: relAssets,
    status,
  };

  const existing = registry.games.findIndex((game) => game && game.id === gameId);
  if (existing >= 0) registry.games[existing] = next;
  else registry.games.push(next);
  registry.games.sort((a, b) => String(a.id).localeCompare(String(b.id)));
  writeRegistry(root, registry);
  return next;
}

export function gameRegistryPath(root) {
  return relative(root, registryPath(root)).replace(/\\/g, "/");
}
