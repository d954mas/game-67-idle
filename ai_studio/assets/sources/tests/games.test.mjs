import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import { listRegisteredGames } from "../games.mjs";
import { upsertWorkspaceMount, writeIdentityManifest } from "../../../workspace/catalog.mjs";

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "catalog-games-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
}

function dependencies(root, id) {
  const path = join(root, "games", id, "dependencies.json");
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify({
    schema: "ai_studio.game.dependencies.v2",
    engine: { source: "external/neotolis-engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" },
    features: [], compatibility: "test",
  }));
}

test("game asset source lists public workspace mounts", (t) => {
  const root = fixture(t);
  dependencies(root, "demo-game");
  writeIdentityManifest(root, "game", { id: "demo-game", title: "Demo" });
  upsertWorkspaceMount(root, {
    kind: "game",
    root: "games/demo-game",
    visibility: "public",
    gitRoot: "",
    commitPolicy: "parent-public",
    enabledStores: ["assets"],
    aliases: [],
  });

  assert.deepEqual(listRegisteredGames(root), [{
    id: "demo-game", title: "Demo", folder: "games/demo-game", assets: "games/demo-game/assets", status: "active",
  }]);
});
