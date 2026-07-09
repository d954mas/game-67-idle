import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { execFileSync, spawnSync } from "node:child_process";
import test from "node:test";

const script = resolve("games/new_game.mjs");

function taskboardItems(root) {
  return join(root, "ai_studio", "taskboard", "items");
}

function tempRepo() {
  const root = mkdtempSync(join(tmpdir(), "new-game-"));
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
  assert.match(output, /created taskboard project: P001/);
  assert.match(output, /updated VS Code tasks\/launch/);
  assert.equal(existsSync(join(root, "games", "test-game", "CMakeLists.txt")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "assets", "readme.txt")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "src", "generated", "game.h")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "build", "stale.obj")), false);
  assert.equal(existsSync(join(root, "games", "test-game", ".ai_studio", "workspace.json")), true);
  assert.equal(existsSync(join(root, "games", "test-game", ".ai_studio", "taskboard", "items")), true);
  assert.equal(existsSync(join(root, "games", "test-game", ".ai_studio", "canvas", "projects")), true);
  assert.equal(existsSync(join(root, "games", "test-game", ".ai_studio", "evidence")), true);
  const workspace = JSON.parse(readFileSync(join(root, "games", "test-game", ".ai_studio", "workspace.json"), "utf8"));
  assert.equal(workspace.gameId, "test-game");
  assert.equal(workspace.visibility, "public");

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

  const taskboardProject = readFileSync(join(taskboardItems(root), "projects", "P001-test-game.md"), "utf8");
  assert.match(taskboardProject, /kind: game/);
  assert.match(taskboardProject, /target: games\/test-game/);
});

test("new_game --private creates a nested private game without public Studio writes", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });

  const output = execFileSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "secret-game",
    "--private",
    "--public-alias",
    "Private Slot",
  ], { encoding: "utf8" });

  assert.match(output, /new private game 'secret-game' created/);
  assert.equal(existsSync(join(root, "games", "secret-game", "CMakeLists.txt")), true);
  assert.equal(existsSync(join(root, "games", "secret-game", ".git")), true);
  assert.equal(existsSync(join(root, "games", "secret-game", ".ai_studio", "workspace.json")), true);
  assert.equal(existsSync(join(root, "games", "secret-game", ".ai_studio", "taskboard", "items")), true);
  assert.equal(existsSync(join(root, "games", "secret-game", ".ai_studio", "canvas", "projects")), true);
  assert.equal(existsSync(join(root, "games", "secret-game", ".ai_studio", "evidence")), true);

  const workspace = JSON.parse(readFileSync(join(root, "games", "secret-game", ".ai_studio", "workspace.json"), "utf8"));
  assert.equal(workspace.gameId, "secret-game");
  assert.equal(workspace.visibility, "private");

  assert.equal(existsSync(join(root, "games", "games.json")), false);
  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), false);
  assert.equal(existsSync(join(root, ".vscode", "launch.json")), false);
  assert.equal(existsSync(join(taskboardItems(root), "projects", "P001-secret-game.md")), false);

  const localRegistry = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "games.local.json"), "utf8"));
  assert.deepEqual(localRegistry.games.map((game) => ({
    gameId: game.gameId,
    root: game.root,
    publicAlias: game.publicAlias,
    commitPolicy: game.commitPolicy,
  })), [{
    gameId: "secret-game",
    root: "games/secret-game",
    publicAlias: "Private Slot",
    commitPolicy: "nested-private",
  }]);
  assert.match(readFileSync(join(root, ".git", "info", "exclude"), "utf8"), /games\/secret-game\//);
});

test("new_game --private rejects public game id collisions", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  mkdirSync(dirname(join(root, "games", "games.json")), { recursive: true });
  writeFileSync(join(root, "games", "games.json"), JSON.stringify({
    schema: "ai_studio.assets.games.v1",
    games: [{ id: "secret-game", title: "Public", folder: "games/secret-game", assets: "games/secret-game/assets", status: "active" }],
  }, null, 2), "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "secret-game", "--private"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /already registered as a public game/);
  assert.equal(existsSync(join(root, "ai_studio", "workspace", "games.local.json")), false);
});

test("new_game --private --force refuses parent-tracked target roots before copying", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  mkdirSync(join(root, "games", "secret-game"), { recursive: true });
  writeFileSync(join(root, "games", "secret-game", "CMakeLists.txt"), "tracked parent file\n", "utf8");
  execFileSync("git", ["add", "games/secret-game/CMakeLists.txt"], { cwd: root, stdio: "ignore" });

  const result = spawnSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "secret-game",
    "--private",
    "--force",
  ], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /tracked by the parent repository/);
  assert.equal(readFileSync(join(root, "games", "secret-game", "CMakeLists.txt"), "utf8"), "tracked parent file\n");
});

test("new_game --private verifies an existing nested git repo", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  mkdirSync(join(root, "games", "secret-game", ".git"), { recursive: true });
  writeFileSync(join(root, "games", "secret-game", "CMakeLists.txt"), "existing private file\n", "utf8");

  const result = spawnSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "secret-game",
    "--private",
    "--force",
  ], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /does not contain a valid nested git repository/);
  assert.equal(readFileSync(join(root, "games", "secret-game", "CMakeLists.txt"), "utf8"), "existing private file\n");
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
  const taskboardProject = readFileSync(join(taskboardItems(root), "projects", "P001-test-game.md"), "utf8");
  assert.match(taskboardProject, /target: games\/test-game/);
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

test("new_game --help prints usage", () => {
  const result = spawnSync(process.execPath, [script, "--help"], { encoding: "utf8" });

  assert.equal(result.status, 0);
  assert.match(result.stdout, /usage: node games\/new_game\.mjs/);
});

test("new_game rejects unknown arguments", () => {
  const result = spawnSync(process.execPath, [script, "--wat"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /unknown argument: --wat/);
  assert.match(result.stderr, /usage: node games\/new_game\.mjs/);
});
