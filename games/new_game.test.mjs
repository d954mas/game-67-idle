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
  writeFileSync(join(root, "templates", "template", "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.20)\nset(GAME_STATE_DIR \"${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state\")\n", "utf8");
  writeFileSync(join(root, "templates", "template", "assets", "readme.txt"), "asset\n", "utf8");
  writeFileSync(join(root, "templates", "template", "src", "generated", "game.h"), "#pragma once\n", "utf8");
  writeFileSync(join(root, "templates", "template", "build", "stale.obj"), "generated\n", "utf8");
  mkdirSync(join(root, "ai_studio", "workspace"), { recursive: true });
  writeFileSync(join(root, "templates", "template", "template.json"), JSON.stringify({
    schema: "ai_studio.template.v1", id: "template", title: "Template", storageNamespace: "template",
  }), "utf8");
  writeFileSync(join(root, "templates", "template", "game-dependencies.json"), JSON.stringify({
    schema: "ai_studio.game.dependencies.seed.v1",
    engine: { source: "external/neotolis-engine", compatibility: "tested" },
    features: [{ id: "game-state", source: "features/game-state", compatibility: "tested" }],
    compatibility: "tested template seed",
  }), "utf8");
  writeFileSync(join(root, "ai_studio", "workspace", "catalog.json"), JSON.stringify({
    schema: "ai_studio.workspace.catalog.v1",
    mounts: [{ kind: "template", root: "templates/template", visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] }],
  }), "utf8");
  mkdirSync(join(root, "features", "game-state"), { recursive: true });
  writeFileSync(join(root, "features", "game-state", "README.md"), "# game-state\n", "utf8");
  const engineRoot = join(root, "external", "neotolis-engine");
  mkdirSync(engineRoot, { recursive: true });
  execFileSync("git", ["init"], { cwd: engineRoot, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "tests@example.invalid"], { cwd: engineRoot, stdio: "ignore" });
  execFileSync("git", ["config", "user.name", "Tests"], { cwd: engineRoot, stdio: "ignore" });
  writeFileSync(join(engineRoot, "README.md"), "# engine\n", "utf8");
  execFileSync("git", ["add", "."], { cwd: engineRoot, stdio: "ignore" });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: engineRoot, stdio: "ignore" });
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "tests@example.invalid"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.name", "Tests"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["add", "."], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });
  return root;
}

test("new_game copies template and registers game assets in AI Studio", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const output = execFileSync(process.execPath, [script, "--root", root, "--id", "test-game"], { encoding: "utf8" });

  assert.match(output, /new game 'test-game' created/);
  assert.match(output, /registered assets: ai_studio\/workspace\/catalog\.json -> games\/test-game\/assets/);
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

  const registryPath = join(root, "ai_studio", "workspace", "catalog.json");
  assert.equal(existsSync(registryPath), true);
  const registry = JSON.parse(readFileSync(registryPath, "utf8"));
  assert.deepEqual(registry.mounts.map((mount) => mount.root), ["games/test-game", "templates/template"]);
  assert.equal(existsSync(join(root, "games", "test-game", "game.json")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "dependencies.json")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "game-dependencies.json")), false);
  const dependencies = JSON.parse(readFileSync(join(root, "games", "test-game", "dependencies.json"), "utf8"));
  assert.match(dependencies.engine.revision, /^[0-9a-f]{40}$/);
  assert.deepEqual(dependencies.features.map((feature) => feature.id), ["game-state"]);
  assert.match(dependencies.features[0].revision, /^[0-9a-f]{40}$/);

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

  const localRegistry = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.local.json"), "utf8"));
  assert.deepEqual(localRegistry.mounts.map((game) => ({
    root: game.root,
    aliases: game.aliases,
    commitPolicy: game.commitPolicy,
  })), [{
    root: "games/secret-game",
    aliases: ["Private Slot"],
    commitPolicy: "nested-private",
  }]);
  assert.match(readFileSync(join(root, ".git", "info", "exclude"), "utf8"), /games\/secret-game\//);
});

test("new_game --visibility public explicitly creates a tracked public game", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const output = execFileSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "test-game",
    "--visibility",
    "public",
  ], { encoding: "utf8" });

  assert.match(output, /new game 'test-game' created/);

  const workspace = JSON.parse(readFileSync(join(root, "games", "test-game", ".ai_studio", "workspace.json"), "utf8"));
  assert.equal(workspace.visibility, "public");

  const registry = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.deepEqual(registry.mounts.map((mount) => mount.root), ["games/test-game", "templates/template"]);
  assert.equal(existsSync(join(taskboardItems(root), "projects", "P001-test-game.md")), true);
  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), true);
});

test("new_game --visibility private creates a nested private game without public Studio writes", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });

  const output = execFileSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "secret-game",
    "--visibility",
    "private",
  ], { encoding: "utf8" });

  assert.match(output, /new private game 'secret-game' created/);

  const workspace = JSON.parse(readFileSync(join(root, "games", "secret-game", ".ai_studio", "workspace.json"), "utf8"));
  assert.equal(workspace.visibility, "private");
  assert.equal(existsSync(join(root, "games", "games.json")), false);
  assert.equal(existsSync(join(taskboardItems(root), "projects", "P001-secret-game.md")), false);
  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), false);
});

test("new_game --require-visibility rejects missing public or private choice before copying", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "test-game",
    "--require-visibility",
  ], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /missing visibility choice/);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
});

test("new_game rejects conflicting private compatibility flag and public visibility", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "test-game",
    "--private",
    "--visibility",
    "public",
  ], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /--private conflicts with --visibility public/);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
});

test("new_game rejects missing visibility value", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "test-game",
    "--visibility",
  ], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /--visibility requires public or private/);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
});

test("new_game rejects invalid visibility values before copying", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "test-game",
    "--visibility",
    "secret",
  ], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /invalid --visibility 'secret'/);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
  assert.equal(existsSync(join(root, "games", "games.json")), false);
  assert.equal(existsSync(join(root, "ai_studio", "workspace", "games.local.json")), false);
});

test("new_game rejects public aliases outside private visibility", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "test-game",
    "--visibility",
    "public",
    "--public-alias",
    "Safe Alias",
  ], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /--public-alias is only valid with private visibility/);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
  assert.equal(existsSync(join(root, "games", "games.json")), false);
});

test("new_game --private rejects public game id collisions", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  mkdirSync(join(root, "games", "secret-game"), { recursive: true });
  writeFileSync(join(root, "games", "secret-game", "game.json"), JSON.stringify({ schema: "ai_studio.game.v1", id: "secret-game", title: "Public", storageNamespace: "secret-game" }), "utf8");
  writeFileSync(join(root, "games", "secret-game", "dependencies.json"), JSON.stringify({ schema: "ai_studio.game.dependencies.v1", engine: { source: "engine", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" }), "utf8");
  const publicCatalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  publicCatalog.mounts.push({ kind: "game", root: "games/secret-game", visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] });
  writeFileSync(join(root, "ai_studio", "workspace", "catalog.json"), JSON.stringify(publicCatalog), "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "secret-game", "--private", "--force"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /already registered as a public game/);
  assert.equal(existsSync(join(root, "ai_studio", "workspace", "catalog.local.json")), false);
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
  execFileSync(process.execPath, [script, "--root", root, "--id", "test-game", "--force"], { encoding: "utf8" });

  const registry = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.equal(registry.mounts.filter((mount) => mount.root === "games/test-game").length, 1);
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
  writeFileSync(join(templateDir, "template.json"), JSON.stringify({ schema: "ai_studio.template.v1", id: "cozy-template", title: "Cozy Template", storageNamespace: "cozy-template" }), "utf8");
  writeFileSync(join(templateDir, "game-dependencies.json"), JSON.stringify({ schema: "ai_studio.game.dependencies.seed.v1", engine: { source: "external/neotolis-engine", compatibility: "tested" }, features: [], compatibility: "tested" }), "utf8");
  const catalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  catalog.mounts.push({ kind: "template", root: "templates/cozy-template", visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] });
  writeFileSync(join(root, "ai_studio", "workspace", "catalog.json"), JSON.stringify(catalog), "utf8");
  execFileSync("git", ["add", "templates/cozy-template", "ai_studio/workspace/catalog.json"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["commit", "-m", "cozy template fixture"], { cwd: root, stdio: "ignore" });

  const output = execFileSync(process.execPath, [script, "--root", root, "--id", "cozy-game", "--template", "cozy-template"], { encoding: "utf8" });

  assert.match(output, /new game 'cozy-game' created from templates\/cozy-template\/ -> games\/cozy-game\//);
  assert.equal(existsSync(join(root, "games", "cozy-game", "assets", "cozy.txt")), true);
  const registry = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.ok(registry.mounts.some((mount) => mount.root === "games/cozy-game"));
});

test("new_game --template rejects unregistered template ids", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "test-game", "--template", "paused-template"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /template 'paused-template' is not registered or is disabled/);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
});

test("new_game refuses dependency placeholders outside an exact Git checkout", (t) => {
  const root = mkdtempSync(join(tmpdir(), "new-game-no-git-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "test-game"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /requires an exact Git revision/);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
});

test("new_game rejects a clean engine checkout that differs from the parent gitlink", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const engineRoot = join(root, "external", "neotolis-engine");
  writeFileSync(join(engineRoot, "new.txt"), "new engine commit\n", "utf8");
  execFileSync("git", ["add", "."], { cwd: engineRoot, stdio: "ignore" });
  execFileSync("git", ["commit", "-m", "advance engine"], { cwd: engineRoot, stdio: "ignore" });

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "test-game"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /engine checkout HEAD must match the parent engine gitlink/);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
});

test("new_game --help prints usage", () => {
  const result = spawnSync(process.execPath, [script, "--help"], { encoding: "utf8" });

  assert.equal(result.status, 0);
  assert.match(result.stdout, /usage: node games\/new_game\.mjs/);
  assert.match(result.stdout, /--visibility public\|private/);
  assert.match(result.stdout, /omitting --visibility still creates a public\/tracked game/);
});

test("new_game rejects unknown arguments", () => {
  const result = spawnSync(process.execPath, [script, "--wat"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /unknown argument: --wat/);
  assert.match(result.stderr, /usage: node games\/new_game\.mjs/);
});
