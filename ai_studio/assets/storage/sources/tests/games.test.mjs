import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { gameRegistryPath, listRegisteredGames, registerGameAssetSource } from "../games.mjs";

function tempRoot() {
  return mkdtempSync(join(tmpdir(), "ai-studio-games-registry-"));
}

test("registerGameAssetSource creates and lists a game asset source", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const registered = registerGameAssetSource(root, { id: "test-game", title: "Test Game" });

  assert.equal(registered.assets, "games/test-game/assets");
  assert.equal(gameRegistryPath(root), "ai_studio/assets/storage/sources/games.json");
  assert.deepEqual(listRegisteredGames(root), [{
    id: "test-game",
    title: "Test Game",
    folder: "games/test-game",
    assets: "games/test-game/assets",
    status: "active",
  }]);
});

test("registerGameAssetSource upserts by game id", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  registerGameAssetSource(root, { id: "test-game", title: "Old" });
  registerGameAssetSource(root, { id: "test-game", title: "New", folder: "./games/test-game/", assets: "./games/test-game/assets/" });

  const parsed = JSON.parse(readFileSync(join(root, "ai_studio", "assets", "storage", "sources", "games.json"), "utf8"));
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

test("registerGameAssetSource keeps folder and assets inside the repository", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  assert.throws(
    () => registerGameAssetSource(root, { id: "bad-game", folder: "../outside" }),
    /game folder must be repo-relative/,
  );
  assert.throws(
    () => registerGameAssetSource(root, { id: "bad-game", assets: "C:/outside/assets" }),
    /game assets must be repo-relative/,
  );
});

test("listRegisteredGames accepts UTF-8 BOM registry files", (t) => {
  const root = tempRoot();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const path = join(root, "ai_studio", "assets", "storage", "sources", "games.json");
  mkdirSync(join(root, "ai_studio", "assets", "storage", "sources"), { recursive: true });
  writeFileSync(path, `\uFEFF${JSON.stringify({
    schema: "ai_studio.assets.games.v1",
    games: [{ id: "test-game", title: "Test Game", folder: "games/test-game", assets: "games/test-game/assets" }],
  })}`, "utf8");

  assert.deepEqual(listRegisteredGames(root), [{
    id: "test-game",
    title: "Test Game",
    folder: "games/test-game",
    assets: "games/test-game/assets",
    status: "active",
  }]);
});
