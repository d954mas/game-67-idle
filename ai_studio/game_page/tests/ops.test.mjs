import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import { getGameOverview, listGames, resolveGameMount } from "../ops.mjs";

function writeJson(root, rel, value) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function gameIdentity(root, rel, id, title) {
  writeJson(root, `${rel}/game.json`, {
    schema: "ai_studio.game.v1", id, title, storageNamespace: id, version: "0.3.0",
  });
  writeJson(root, `${rel}/dependencies.json`, {
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" },
    features: [], compatibility: "test",
  });
}

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "game-page-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "games", "private"), { recursive: true });
  gameIdentity(root, "games/alpha-game", "alpha-game", "Alpha Game");
  gameIdentity(root, "games/private/beta-game", "beta-game", "Beta Game");
  mkdirSync(join(root, "games/private/beta-game/.git"), { recursive: true });
  return root;
}

test("listGames returns public and private games sorted by title", (t) => {
  const root = fixture(t);
  const payload = listGames(root);
  assert.equal(payload.schema, "ai_studio.game_page.games.v1");
  assert.deepEqual(payload.games.map(({ id, visibility }) => ({ id, visibility })), [
    { id: "alpha-game", visibility: "public" },
    { id: "beta-game", visibility: "private" },
  ]);
  assert.equal(payload.games[1].root, "games/private/beta-game");
});

test("resolveGameMount matches id case-insensitively and rejects unknown ids", (t) => {
  const root = fixture(t);
  assert.equal(resolveGameMount(root, "Beta-Game").gameId, "beta-game");
  assert.equal(resolveGameMount(root, "missing-game"), null);
  assert.equal(resolveGameMount(root, ""), null);
});

test("getGameOverview reports identity, design docs, and studio taskboard project", (t) => {
  const root = fixture(t);
  writeFileSync(join(root, "games/alpha-game/game.json"), JSON.stringify({
    schema: "ai_studio.game.v1", id: "alpha-game", title: "Alpha Game",
    storageNamespace: "alpha-game", version: "1.2.3",
  }), "utf8");
  mkdirSync(join(root, "games/alpha-game/design"), { recursive: true });
  writeFileSync(join(root, "games/alpha-game/design/gdd.md"), "# GDD\n", "utf8");
  const projectPath = join(root, "ai_studio/taskboard/items/projects/P001-alpha.md");
  mkdirSync(dirname(projectPath), { recursive: true });
  writeFileSync(projectPath, [
    "---",
    "id: P001",
    'title: "Alpha"',
    "status: active",
    "kind: game",
    "target: games/alpha-game",
    "priority: P1",
    "created: 2026-08-23",
    "updated: 2026-08-23",
    "---",
    "",
    "## Goal",
    "",
  ].join("\n"), "utf8");

  const overview = getGameOverview(root, "alpha-game");
  assert.equal(overview.game.version, "1.2.3");
  assert.equal(overview.game.visibility, "public");
  assert.deepEqual(overview.designDocs.map((doc) => doc.rel), ["design/gdd.md"]);
  assert.deepEqual(overview.taskboardProjects.map(({ id, status, store }) => ({ id, status, store })), [
    { id: "P001", status: "active", store: "studio" },
  ]);
  assert.match(overview.links.assetViewer, /sourceId=game%3Aalpha-game/);
});

test("getGameOverview returns null for unknown games", (t) => {
  const root = fixture(t);
  assert.equal(getGameOverview(root, "nope"), null);
});
