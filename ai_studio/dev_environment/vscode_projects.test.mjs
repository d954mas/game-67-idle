import assert from "node:assert/strict";
import test from "node:test";
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import { localGameRegistryRelPath } from "../workspace/games.mjs";
import {
  collectPlayableProjects,
  renderLaunch,
  renderTasks,
  writePrivateGameVscodeProjectFiles,
  writeVscodeProjectFiles,
} from "./vscode_projects.mjs";

function writeJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function makePlayable(root, rel) {
  mkdirSync(join(root, rel), { recursive: true });
  writeFileSync(join(root, rel, "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.25)\n", "utf8");
}

function privateGameFixture(root, gameId = "secret-game") {
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), `${localGameRegistryRelPath()}\n`, "utf8");
  writeFileSync(join(root, ".git", "info", "exclude"), `games/${gameId}/\n`, "utf8");
  makePlayable(root, `games/${gameId}`);
  execFileSync("git", ["init"], { cwd: join(root, "games", gameId), stdio: "ignore" });
  writeJson(join(root, "ai_studio", "workspace", "games.local.json"), {
    schema: "ai_studio.workspace.games.local.v1",
    games: [
      {
        schemaVersion: 1,
        storeId: `game:${gameId}`,
        kind: "game",
        gameId,
        root: `games/${gameId}`,
        visibility: "private",
        gitRoot: `games/${gameId}`,
        commitPolicy: "nested-private",
        enabledStores: ["assets", "taskboard", "canvas", "evidence"],
        assetRoot: `games/${gameId}/assets`,
      },
    ],
  });
}

test("collectPlayableProjects reads active registered templates and games", (t) => {
  const root = mkdtempSync(join(tmpdir(), "vscode-projects-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  makePlayable(root, "templates/base");
  makePlayable(root, "games/first-game");
  mkdirSync(join(root, "templates/disabled"), { recursive: true });
  mkdirSync(join(root, "games/missing-cmake"), { recursive: true });
  writeJson(join(root, "templates", "templates.json"), {
    schema: "ai_studio.assets.templates.v1",
    templates: [
      { id: "base", title: "Base", folder: "templates/base", assets: "templates/base/assets", status: "active" },
      { id: "disabled", title: "Disabled", folder: "templates/disabled", assets: "templates/disabled/assets", status: "disabled" },
    ],
  });
  writeJson(join(root, "games", "games.json"), {
    schema: "ai_studio.assets.games.v1",
    games: [
      { id: "first-game", title: "First Game", folder: "games/first-game", assets: "games/first-game/assets", status: "active" },
      { id: "missing-cmake", title: "Missing CMake", folder: "games/missing-cmake", assets: "games/missing-cmake/assets", status: "active" },
    ],
  });

  assert.deepEqual(collectPlayableProjects(root).map((project) => `${project.kind}:${project.id}`), [
    "game:first-game",
    "template:base",
  ]);
});

test("renderTasks and renderLaunch create matching project build and run entries", () => {
  const projects = [{ kind: "game", id: "first-game", title: "First Game", folder: "games/first-game" }];
  const tasks = renderTasks(projects);
  const labels = tasks.tasks.map((task) => task.label);
  assert.ok(labels.includes("Game: first-game: configure native debug"));
  assert.ok(labels.includes("Game: first-game: build packs native debug"));
  assert.ok(labels.includes("Game: first-game: build native debug"));
  assert.ok(labels.includes("Game: first-game: run native debug"));
  assert.ok(labels.includes("Game: first-game: capture settings native debug"));

  const launch = renderLaunch(projects);
  assert.deepEqual(launch.configurations.map((config) => config.name), [
    "Debug Game: first-game (native debug)",
    "Run Game: first-game (native release)",
  ]);
  assert.equal(launch.configurations[0].preLaunchTask, "Game: first-game: build native debug");
  assert.equal(launch.configurations[0].program, "${workspaceFolder}/games/first-game/build/native-debug/bin/game.exe");
});

test("writeVscodeProjectFiles writes tasks and launch files", (t) => {
  const root = mkdtempSync(join(tmpdir(), "vscode-projects-write-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  makePlayable(root, "templates/base");
  writeJson(join(root, "templates", "templates.json"), {
    schema: "ai_studio.assets.templates.v1",
    templates: [{ id: "base", title: "Base", folder: "templates/base", assets: "templates/base/assets", status: "active" }],
  });

  const result = writeVscodeProjectFiles(root);

  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), true);
  assert.equal(existsSync(join(root, ".vscode", "launch.json")), true);
  assert.equal(result.projects.length, 1);
  const tasks = JSON.parse(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"));
  const launch = JSON.parse(readFileSync(join(root, ".vscode", "launch.json"), "utf8"));
  assert.equal(tasks.tasks[0].label, "Template: base: configure native debug");
  assert.equal(launch.configurations[0].name, "Debug Template: base (native debug)");
});

test("writeVscodeProjectFiles excludes local private game mounts", (t) => {
  const root = mkdtempSync(join(tmpdir(), "vscode-projects-private-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  makePlayable(root, "games/public-game");
  makePlayable(root, "games/secret-game");
  writeJson(join(root, "games", "games.json"), {
    schema: "ai_studio.assets.games.v1",
    games: [
      {
        id: "public-game",
        title: "Public Game",
        folder: "games/public-game",
        assets: "games/public-game/assets",
        status: "active",
      },
    ],
  });
  writeJson(join(root, "ai_studio", "workspace", "games.local.json"), {
    schema: "ai_studio.workspace.games.local.v1",
    games: [
      {
        schemaVersion: 1,
        storeId: "game:secret-game",
        kind: "game",
        gameId: "secret-game",
        root: "games/secret-game",
        visibility: "private",
        gitRoot: "games/secret-game",
        commitPolicy: "nested-private",
        enabledStores: ["assets", "taskboard", "canvas", "evidence"],
      },
    ],
  });

  const result = writeVscodeProjectFiles(root);
  const tasksText = readFileSync(join(root, ".vscode", "tasks.json"), "utf8");
  const launchText = readFileSync(join(root, ".vscode", "launch.json"), "utf8");

  assert.deepEqual(result.projects.map((project) => `${project.kind}:${project.id}`), ["game:public-game"]);
  assert.match(tasksText, /public-game/);
  assert.match(launchText, /public-game/);
  assert.doesNotMatch(tasksText, /secret-game|games\/secret-game|game:secret-game/);
  assert.doesNotMatch(launchText, /secret-game|games\/secret-game|game:secret-game/);
});

test("writePrivateGameVscodeProjectFiles writes game-local IDE files for private mounts", (t) => {
  const root = mkdtempSync(join(tmpdir(), "vscode-projects-private-local-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  privateGameFixture(root);

  const result = writePrivateGameVscodeProjectFiles(root, "secret-game");
  const tasksText = readFileSync(join(root, "games", "secret-game", ".vscode", "tasks.json"), "utf8");
  const launch = JSON.parse(readFileSync(join(root, "games", "secret-game", ".vscode", "launch.json"), "utf8"));

  assert.equal(result.tasksPath, join(root, "games", "secret-game", ".vscode", "tasks.json"));
  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), false);
  assert.match(tasksText, /Game: secret-game: configure native debug/);
  assert.doesNotMatch(tasksText, /games\/secret-game|game:secret-game/);
  assert.equal(launch.configurations[0].program, "${workspaceFolder}/build/native-debug/bin/game.exe");
});
