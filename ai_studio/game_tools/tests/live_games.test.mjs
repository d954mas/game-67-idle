import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import { checkLiveGames } from "../live_games.mjs";

const ENGINE = "1".repeat(40);

function writeJson(root, rel, value) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify(value), "utf8");
}

function game(root, id, { engine = ENGINE, lifecycle } = {}) {
  writeJson(root, `games/${id}/game.json`, { schema: "ai_studio.game.v1", id, title: id, storageNamespace: id, ...(lifecycle ? { lifecycle } : {}) });
  writeJson(root, `games/${id}/dependencies.json`, {
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "external/neotolis-engine", version: engine.slice(0, 8), revision: engine, compatibility: "test" },
    features: [], compatibility: "test",
  });
}

test("every live game runs its own tests; frozen games are skipped and drift is reported", (t) => {
  const root = mkdtempSync(join(tmpdir(), "live-games-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  game(root, "green-game");
  game(root, "red-game", { engine: "2".repeat(40) });
  game(root, "old-game", { lifecycle: { state: "frozen", studioCommit: "a".repeat(40), since: "2026-10-19" } });

  const calls = [];
  const spawn = (command, args, options) => {
    calls.push({ args, cwd: options.cwd });
    const red = options.cwd.endsWith("red-game");
    return { status: red ? 1 : 0, stdout: red ? "native game tests exited 1\n" : "game tests passed: node 1 passed\n", stderr: "" };
  };
  const rows = checkLiveGames(root, { spawn, engineRevision: ENGINE, tier: "core" });

  assert.deepEqual(rows.map((row) => [row.game, row.testsPassed, row.drift.length]), [["green-game", true, 0], ["red-game", false, 1]]);
  assert.equal(rows[1].summary, "native game tests exited 1");
  assert.match(calls[0].args[0], /tools[\\/]game\.mjs$/);
  assert.deepEqual(calls[0].args.slice(1), ["test", "--tier", "core"]);
  assert.deepEqual(checkLiveGames(root, { spawn, engineRevision: ENGINE, games: ["green-game"] }).map((row) => row.game), ["green-game"]);
});

test("a game listing the metaserver library vets and builds its metaserver module", (t) => {
  const root = mkdtempSync(join(tmpdir(), "live-games-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writeJson(root, "features/metaserver/feature.json", { id: "metaserver", version: "1.0.0" });
  for (const id of ["online-game", "broken-game"]) {
    game(root, id);
    writeJson(root, `games/${id}/dependencies.json`, {
      schema: "ai_studio.game.dependencies.v3",
      engine: { source: "external/neotolis-engine", version: ENGINE.slice(0, 8), revision: ENGINE, compatibility: "test" },
      features: [{ id: "metaserver", source: "features/metaserver", version: "1.0.0", compatibility: "test" }], compatibility: "test",
    });
  }
  const calls = [];
  const spawn = (command, args, options) => {
    calls.push([command, ...args, options.cwd]);
    const broken = command === "go" && args[0] === "build" && options.cwd.includes("broken-game");
    return { status: broken ? 1 : 0, stdout: broken ? "main.go:7: undefined: metaserver.Gone\n" : "ok\n", stderr: "" };
  };
  const rows = checkLiveGames(root, { spawn, engineRevision: ENGINE });

  assert.deepEqual(rows.map((row) => [row.game, row.testsPassed, row.metaserver]),
    [["broken-game", true, "go build: main.go:7: undefined: metaserver.Gone"], ["online-game", true, ""]]);
  const online = calls.filter((call) => call[0] === "go" && call.at(-1).includes("online-game"));
  assert.deepEqual(online.map((call) => call.slice(1, 3)), [["vet", "./..."], ["build", "./..."]]);
  assert.match(online[0].at(-1), /online-game[\\/]metaserver$/);
});
