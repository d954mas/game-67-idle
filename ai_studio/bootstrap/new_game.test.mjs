import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { execFileSync, spawnSync } from "node:child_process";
import test from "node:test";

const script = resolve("ai_studio/bootstrap/new_game.mjs");

function tempRepo() {
  const root = mkdtempSync(join(tmpdir(), "new-game-bootstrap-"));
  mkdirSync(join(root, "templates", "template", "assets"), { recursive: true });
  mkdirSync(join(root, "templates", "template", "src", "generated"), { recursive: true });
  mkdirSync(join(root, "templates", "template", "build"), { recursive: true });
  writeFileSync(join(root, "templates", "template", "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.20)\n", "utf8");
  writeFileSync(join(root, "templates", "template", "assets", "readme.txt"), "asset\n", "utf8");
  writeFileSync(join(root, "templates", "template", "src", "generated", "game.h"), "#pragma once\n", "utf8");
  writeFileSync(join(root, "templates", "template", "build", "stale.obj"), "generated\n", "utf8");
  return root;
}

test("new_game copies template and registers game assets in AI Studio", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const output = execFileSync(process.execPath, [script, "--root", root, "--id", "test-game"], { encoding: "utf8" });

  assert.match(output, /new game 'test-game' created/);
  assert.match(output, /registered assets: games\/games\.json -> games\/test-game\/assets/);
  assert.match(output, /updated VS Code tasks\/launch/);
  assert.equal(existsSync(join(root, "games", "test-game", "CMakeLists.txt")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "assets", "readme.txt")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "src", "generated", "game.h")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "build", "stale.obj")), false);

  const registryPath = join(root, "games", "games.json");
  assert.equal(existsSync(registryPath), true);
  const registry = JSON.parse(readFileSync(registryPath, "utf8"));
  assert.deepEqual(registry.games, [{
    id: "test-game",
    title: "test-game",
    folder: "games/test-game",
    assets: "games/test-game/assets",
    status: "active",
  }]);

  const tasks = JSON.parse(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"));
  const launch = JSON.parse(readFileSync(join(root, ".vscode", "launch.json"), "utf8"));
  assert.ok(tasks.tasks.some((task) => task.label === "Game: test-game: build packs native debug"));
  assert.ok(launch.configurations.some((config) => config.name === "Debug Game: test-game (native debug)"));
});

test("new_game --force updates the same registry entry", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(dirname(join(root, "games", "games.json")), { recursive: true });
  writeFileSync(join(root, "games", "games.json"), JSON.stringify({
    schema: "ai_studio.assets.games.v1",
    games: [{ id: "test-game", title: "Old", folder: "old", assets: "old/assets", status: "active" }],
  }, null, 2), "utf8");

  execFileSync(process.execPath, [script, "--root", root, "--id", "test-game", "--force"], { encoding: "utf8" });

  const registry = JSON.parse(readFileSync(join(root, "games", "games.json"), "utf8"));
  assert.deepEqual(registry.games, [{
    id: "test-game",
    title: "test-game",
    folder: "games/test-game",
    assets: "games/test-game/assets",
    status: "active",
  }]);
});

test("new_game --template copies a registered template id", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const templateDir = join(root, "templates", "cozy-template");
  mkdirSync(join(templateDir, "assets"), { recursive: true });
  writeFileSync(join(templateDir, "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.20)\n", "utf8");
  writeFileSync(join(templateDir, "assets", "cozy.txt"), "asset\n", "utf8");
  mkdirSync(dirname(join(root, "templates", "templates.json")), { recursive: true });
  writeFileSync(join(root, "templates", "templates.json"), JSON.stringify({
    schema: "ai_studio.assets.templates.v1",
    templates: [{
      id: "cozy-template",
      title: "Cozy Template",
      folder: "templates/cozy-template",
      assets: "templates/cozy-template/assets",
      status: "active",
    }],
  }, null, 2), "utf8");

  const output = execFileSync(process.execPath, [script, "--root", root, "--id", "cozy-game", "--template", "cozy-template"], { encoding: "utf8" });

  assert.match(output, /new game 'cozy-game' created from templates\/cozy-template\/ -> games\/cozy-game\//);
  assert.equal(existsSync(join(root, "games", "cozy-game", "assets", "cozy.txt")), true);
  const registry = JSON.parse(readFileSync(join(root, "games", "games.json"), "utf8"));
  assert.deepEqual(registry.games, [{
    id: "cozy-game",
    title: "cozy-game",
    folder: "games/cozy-game",
    assets: "games/cozy-game/assets",
    status: "active",
  }]);
});

test("new_game --template rejects disabled template ids", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(dirname(join(root, "templates", "templates.json")), { recursive: true });
  writeFileSync(join(root, "templates", "templates.json"), JSON.stringify({
    schema: "ai_studio.assets.templates.v1",
    templates: [{
      id: "paused-template",
      title: "Paused Template",
      folder: "templates/template",
      assets: "templates/template/assets",
      status: "disabled",
    }],
  }, null, 2), "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "test-game", "--template", "paused-template"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /template 'paused-template' is not registered or is disabled/);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
});
