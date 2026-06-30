import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { gameRegistryPath, listRegisteredGames, registerGameAssetSource } from "../games_registry.mjs";

function tempRoot() {
  return mkdtempSync(join(tmpdir(), "ai-studio-games-registry-"));
}

test("registerGameAssetSource creates and lists a game asset source", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const registered = registerGameAssetSource(root, { id: "test-game", title: "Test Game" });

  assert.equal(registered.assets, "test-game/assets");
  assert.equal(gameRegistryPath(root), "ai_studio/assets/assets_storage/source_registry/games.json");
  assert.deepEqual(listRegisteredGames(root), [{
    id: "test-game",
    title: "Test Game",
    folder: "test-game",
    assets: "test-game/assets",
    status: "active",
  }]);
});

test("registerGameAssetSource upserts by game id", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  registerGameAssetSource(root, { id: "test-game", title: "Old" });
  registerGameAssetSource(root, { id: "test-game", title: "New", folder: "./games/test-game/", assets: "./games/test-game/assets/" });

  const parsed = JSON.parse(readFileSync(join(root, "ai_studio", "assets", "assets_storage", "source_registry", "games.json"), "utf8"));
  assert.equal(parsed.games.length, 1);
  assert.deepEqual(listRegisteredGames(root), [{
    id: "test-game",
    title: "New",
    folder: "games/test-game",
    assets: "games/test-game/assets",
    status: "active",
  }]);
});

test("registerGameAssetSource rejects non-kebab ids", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.throws(() => registerGameAssetSource(root, { id: "Bad Game" }), /lowercase kebab-case/);
});
