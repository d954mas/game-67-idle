import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import { gameRegistryPath, listRegisteredGames, registerGameAssetSource } from "../games.mjs";

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "catalog-games-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
}

function dependencies(root, id) {
  const path = join(root, "games", id, "dependencies.json");
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify({
    schema: "ai_studio.game.dependencies.v1",
    engine: { source: "external/neotolis-engine", revision: "0000000000000000000000000000000000000000", compatibility: "test" },
    features: [], compatibility: "test",
  }));
}

test("game asset source is a workspace catalog adapter", (t) => {
  const root = fixture(t);
  dependencies(root, "demo-game");
  assert.deepEqual(registerGameAssetSource(root, { id: "demo-game", title: "Demo" }), {
    id: "demo-game", title: "Demo", folder: "games/demo-game", assets: "games/demo-game/assets", status: "active",
  });
  assert.equal(gameRegistryPath(root), "ai_studio/workspace/catalog.json");
  assert.deepEqual(listRegisteredGames(root).map((game) => game.id), ["demo-game"]);
  const catalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.deepEqual(catalog.mounts[0].enabledStores, ["assets"]);
});

test("game asset source enforces derived roots and strict ids", (t) => {
  const root = fixture(t);
  assert.throws(() => registerGameAssetSource(root, { id: "Bad" }), /lowercase kebab-case/);
  assert.throws(() => registerGameAssetSource(root, { id: "demo", folder: "../escape" }), /folder must be games\/demo/);
});
