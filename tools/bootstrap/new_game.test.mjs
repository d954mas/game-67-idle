import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { execFileSync } from "node:child_process";
import test from "node:test";

const script = resolve("tools/bootstrap/new_game.mjs");

function tempRepo() {
  const root = mkdtempSync(join(tmpdir(), "new-game-bootstrap-"));
  mkdirSync(join(root, "template", "assets"), { recursive: true });
  writeFileSync(join(root, "template", "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.20)\n", "utf8");
  writeFileSync(join(root, "template", "assets", "readme.txt"), "asset\n", "utf8");
  return root;
}

test("new_game copies template and registers game assets in AI Studio", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const output = execFileSync(process.execPath, [script, "--root", root, "--id", "test-game"], { encoding: "utf8" });

  assert.match(output, /new game 'test-game' created/);
  assert.match(output, /registered assets: ai_studio\/assets\/storage\/sources\/games\.json -> test-game\/assets/);
  assert.equal(existsSync(join(root, "test-game", "CMakeLists.txt")), true);
  assert.equal(existsSync(join(root, "test-game", "assets", "readme.txt")), true);

  const registryPath = join(root, "ai_studio", "assets", "storage", "sources", "games.json");
  assert.equal(existsSync(registryPath), true);
  const registry = JSON.parse(readFileSync(registryPath, "utf8"));
  assert.deepEqual(registry.games, [{
    id: "test-game",
    title: "test-game",
    folder: "test-game",
    assets: "test-game/assets",
    status: "active",
  }]);
});

test("new_game --force updates the same registry entry", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(dirname(join(root, "ai_studio", "assets", "storage", "sources", "games.json")), { recursive: true });
  writeFileSync(join(root, "ai_studio", "assets", "storage", "sources", "games.json"), JSON.stringify({
    schema: "ai_studio.assets.games.v1",
    games: [{ id: "test-game", title: "Old", folder: "old", assets: "old/assets", status: "active" }],
  }, null, 2), "utf8");

  execFileSync(process.execPath, [script, "--root", root, "--id", "test-game", "--force"], { encoding: "utf8" });

  const registry = JSON.parse(readFileSync(join(root, "ai_studio", "assets", "storage", "sources", "games.json"), "utf8"));
  assert.deepEqual(registry.games, [{
    id: "test-game",
    title: "test-game",
    folder: "test-game",
    assets: "test-game/assets",
    status: "active",
  }]);
});
