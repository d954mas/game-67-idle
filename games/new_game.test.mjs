import assert from "node:assert/strict";
import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { execFileSync, spawn, spawnSync } from "node:child_process";
import test, { after } from "node:test";
import {
  parseArgs, resolveVisibility, usageText, validateRequestedIdentity,
} from "./new_game.mjs";

const script = resolve("games/new_game.mjs");

function runNewGame(args, env = {}) {
  return new Promise((resolveResult) => {
    const child = spawn(process.execPath, [script, ...args], {
      env: { ...process.env, ...env }, stdio: ["ignore", "pipe", "pipe"],
    });
    let stdout = "";
    let stderr = "";
    child.stdout.on("data", (chunk) => { stdout += chunk; });
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    child.on("close", (status) => resolveResult({ status, stdout, stderr }));
  });
}

async function waitForFile(path, timeoutMs = 10000) {
  const deadline = Date.now() + timeoutMs;
  while (!existsSync(path)) {
    if (Date.now() >= deadline) throw new Error(`timed out waiting for ${path}`);
    await new Promise((resolveWait) => setTimeout(resolveWait, 20));
  }
}

function taskboardItems(root) {
  return join(root, "ai_studio", "taskboard", "items");
}

function buildFixtureRepo() {
  const root = mkdtempSync(join(tmpdir(), "new-game-"));
  mkdirSync(join(root, "templates", "template", "assets"), { recursive: true });
  mkdirSync(join(root, "templates", "template", "src", "generated"), { recursive: true });
  mkdirSync(join(root, "templates", "template", "build"), { recursive: true });
  writeFileSync(join(root, "templates", "template", "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.20)\nset(GAME_STATE_DIR \"${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state\")\ntarget_compile_definitions(${GAME_TARGET} PRIVATE GAME_STORAGE_APP_ID=\"template\")\n", "utf8");
  mkdirSync(join(root, "templates", "template", "cmake"), { recursive: true });
  mkdirSync(join(root, "templates", "template", "tests"), { recursive: true });
  mkdirSync(join(root, "templates", "template", "src"), { recursive: true });
  writeFileSync(join(root, "templates", "template", "cmake", "GameOptions.cmake"), "set(GAME_TITLE \"Template\" CACHE STRING \"Game window title base\")\n", "utf8");
  writeFileSync(join(root, "templates", "template", "cmake", "GamePlatform.cmake"), "target_compile_definitions(${GAME_TARGET} PRIVATE GAME_STORAGE_APP_ID=\"template\")\n", "utf8");
  writeFileSync(join(root, "templates", "template", "cmake", "GameTests.cmake"), "target_compile_definitions(test_one PRIVATE GAME_STORAGE_APP_ID=\"template_test\")\ntarget_compile_definitions(test_two PRIVATE GAME_STORAGE_APP_ID=\"template_test\")\ntarget_compile_definitions(test_three PRIVATE GAME_STORAGE_APP_ID=\"template_test\")\ntarget_compile_definitions(test_four PRIVATE GAME_STORAGE_APP_ID=\"template_test\")\n", "utf8");
  writeFileSync(join(root, "templates", "template", "src", "game_save.c"), "#define GAME_STORAGE_APP_ID \"template\"\n", "utf8");
  writeFileSync(join(root, "templates", "template", "src", "main.c"), "config.app_name = \"Template\";\n#define GAME_WINDOW_TITLE \"Template\"\n", "utf8");
  writeFileSync(join(root, "templates", "template", "tests", "web_persistence_check.py"), "STORAGE_KEY = \"template/save/autosave\"\n", "utf8");
  writeFileSync(join(root, "templates", "template", "assets", "readme.txt"), "asset\n", "utf8");
  for (const [rel, body] of [
    ["tools/game.mjs", "// game-owned CLI\n"],
    ["tools/package_web.mjs", "// package owner\n"],
    ["tools/portal_evidence.mjs", "// evidence owner\n"],
    ["tools/lib/zip_store.mjs", "// ZIP owner\n"],
    ["release/README.md", "# Release owner\n"],
    [".github/workflows/game-verify.yml", "name: game verify\n"],
    [".gitignore", "release/artifacts/\n"],
  ]) {
    mkdirSync(dirname(join(root, "templates", "template", rel)), { recursive: true });
    writeFileSync(join(root, "templates", "template", rel), body, "utf8");
  }
  writeFileSync(join(root, "templates", "template", "src", "generated", "game.h"), "#pragma once\n", "utf8");
  writeFileSync(join(root, "templates", "template", "build", "stale.obj"), "generated\n", "utf8");
  mkdirSync(join(root, "ai_studio", "workspace"), { recursive: true });
  writeFileSync(join(root, "templates", "template", "template.json"), JSON.stringify({
    schema: "ai_studio.template.v1", id: "template", title: "Template", storageNamespace: "template",
  }), "utf8");
  writeFileSync(join(root, "templates", "template", "game-dependencies.json"), JSON.stringify({
    schema: "ai_studio.game.dependencies.seed.v2",
    engine: { source: "external/neotolis-engine", version: "0.1.0", compatibility: "tested" },
    features: [{ id: "game-state", source: "features/game-state", version: "1.0.0", compatibility: "tested" }],
    compatibility: "tested template seed",
  }), "utf8");
  writeFileSync(join(root, "ai_studio", "workspace", "catalog.json"), JSON.stringify({
    schema: "ai_studio.workspace.catalog.v1",
    mounts: [{ kind: "template", root: "templates/template", visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] }],
  }), "utf8");
  mkdirSync(join(root, "features", "game-state"), { recursive: true });
  writeFileSync(join(root, "features", "game-state", "README.md"), "# game-state\n", "utf8");
  writeFileSync(join(root, "features", "game-state", "feature.json"), JSON.stringify({
    schema: "ai_studio.feature.v1", id: "game-state", version: "1.0.0",
  }), "utf8");
  const engineRoot = join(root, "external", "neotolis-engine");
  mkdirSync(engineRoot, { recursive: true });
  mkdirSync(join(engineRoot, "engine", "core"), { recursive: true });
  writeFileSync(join(engineRoot, "engine", "core", "nt_core.h"), [
    "#define NT_VERSION_MAJOR 0", "#define NT_VERSION_MINOR 1", "#define NT_VERSION_PATCH 0", "",
  ].join("\n"), "utf8");
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

let fixtureRepo;
after(() => {
  if (fixtureRepo) rmSync(fixtureRepo, { recursive: true, force: true });
});

function tempRepo() {
  fixtureRepo ||= buildFixtureRepo();
  const root = mkdtempSync(join(tmpdir(), "new-game-"));
  cpSync(fixtureRepo, root, { recursive: true });
  return root;
}

test("new_game --visibility public copies template and registers game assets in AI Studio", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const output = execFileSync(process.execPath, [script, "--root", root, "--id", "test-game", "--visibility", "public"], { encoding: "utf8" });

  assert.match(output, /new game 'test-game' created/);
  assert.match(output, /registered assets: ai_studio\/workspace\/catalog\.json -> games\/test-game\/assets/);
  assert.match(output, /created taskboard project: P001/);
  assert.match(output, /updated VS Code tasks\/launch/);
  assert.equal(existsSync(join(root, "games", "test-game", "CMakeLists.txt")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "assets", "readme.txt")), true);
  assert.equal(existsSync(join(root, "games", "test-game", "src", "generated", "game.h")), true);
  for (const rel of ["tools/game.mjs", "tools/package_web.mjs", "tools/portal_evidence.mjs", "tools/lib/zip_store.mjs", "release/README.md", ".github/workflows/game-verify.yml", ".gitignore"]) {
    assert.equal(readFileSync(join(root, "games", "test-game", rel), "utf8"), readFileSync(join(root, "templates", "template", rel), "utf8"), rel);
  }
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
  assert.equal(dependencies.engine.version, "0.1.0");
  assert.deepEqual(dependencies.features.map((feature) => feature.id), ["game-state"]);
  assert.equal(dependencies.features[0].version, "1.0.0");
  assert.match(dependencies.features[0].revision, /^[0-9a-f]{40}$/);

  const tasks = JSON.parse(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"));
  const launch = JSON.parse(readFileSync(join(root, ".vscode", "launch.json"), "utf8"));
  assert.ok(tasks.tasks.some((task) => task.label === "Game: test-game: build packs native debug"));
  assert.ok(launch.configurations.some((config) => config.name === "Debug Game: test-game (native debug)"));

  const taskboardProject = readFileSync(join(taskboardItems(root), "projects", "P001-test-game.md"), "utf8");
  assert.match(taskboardProject, /kind: game/);
  assert.match(taskboardProject, /target: games\/test-game/);
});

test("new_game rejects a dependency seed version that drifts from feature metadata", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const seedPath = join(root, "templates", "template", "game-dependencies.json");
  const seed = JSON.parse(readFileSync(seedPath, "utf8"));
  seed.features[0].version = "1.1.0";
  writeFileSync(seedPath, JSON.stringify(seed), "utf8");
  execFileSync("git", ["add", "templates/template/game-dependencies.json"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["commit", "-m", "drift dependency seed"], { cwd: root, stdio: "ignore" });

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "test-game"], { encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /dependency seed version 1\.1\.0 does not match feature\.json 1\.0\.0/i);
  assert.equal(existsSync(join(root, "games", "test-game")), false);
});

test("new_game --visibility private creates a nested private game without public Studio writes", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const output = execFileSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "secret-game",
    "--visibility",
    "private",
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

test("new_game defaults omitted visibility to public for backward compatibility", () => {
  const args = parseArgs(["--id", "test-game"]);
  assert.equal(resolveVisibility(args), "public");
});

test("new_game --private remains a compatibility alias for private visibility", () => {
  const args = parseArgs(["--id", "secret-game", "--private"]);
  assert.equal(resolveVisibility(args), "private");
});

test("new_game --require-visibility rejects missing public or private choice", () => {
  const args = parseArgs(["--id", "test-game", "--require-visibility"]);
  assert.throws(() => resolveVisibility(args), /missing visibility choice/);
});

test("new_game rejects conflicting private compatibility flag and public visibility", () => {
  const args = parseArgs(["--id", "test-game", "--private", "--visibility", "public"]);
  assert.throws(() => resolveVisibility(args), /--private conflicts with --visibility public/);
});

test("new_game rejects missing visibility value", () => {
  assert.throws(
    () => parseArgs(["--id", "test-game", "--visibility"]),
    /--visibility requires public or private/,
  );
});

test("new_game rejects invalid visibility values", () => {
  const args = parseArgs(["--id", "test-game", "--visibility", "secret"]);
  assert.throws(() => resolveVisibility(args), /invalid --visibility 'secret'/);
});

test("new_game rejects public aliases outside private visibility", () => {
  const args = parseArgs([
    "--id", "test-game", "--visibility", "public", "--public-alias", "Safe Alias",
  ]);
  assert.throws(() => resolveVisibility(args), /--public-alias is only valid with private visibility/);
});

test("new_game --private rejects public game id collisions", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "games", "secret-game"), { recursive: true });
  writeFileSync(join(root, "games", "secret-game", "game.json"), JSON.stringify({ schema: "ai_studio.game.v1", id: "secret-game", title: "Public", storageNamespace: "secret-game" }), "utf8");
  writeFileSync(join(root, "games", "secret-game", "dependencies.json"), JSON.stringify({ schema: "ai_studio.game.dependencies.v2", engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" }), "utf8");
  const publicCatalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  publicCatalog.mounts.push({ kind: "game", root: "games/secret-game", visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] });
  writeFileSync(join(root, "ai_studio", "workspace", "catalog.json"), JSON.stringify(publicCatalog), "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "secret-game", "--private", "--replace"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /already registered as a public game/);
  assert.equal(existsSync(join(root, "ai_studio", "workspace", "catalog.local.json")), false);
});

test("new_game --private --replace refuses parent-tracked target roots before copying", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
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
    "--replace",
  ], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /tracked by the parent repository/);
  assert.equal(readFileSync(join(root, "games", "secret-game", "CMakeLists.txt"), "utf8"), "tracked parent file\n");
});

test("new_game --private replacement replaces an invalid old nested git directory transactionally", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "games", "secret-game", ".git"), { recursive: true });
  writeFileSync(join(root, "games", "secret-game", "CMakeLists.txt"), "existing private file\n", "utf8");

  const result = spawnSync(process.execPath, [
    script,
    "--root",
    root,
    "--id",
    "secret-game",
    "--private",
    "--replace",
  ], { encoding: "utf8" });

  assert.equal(result.status, 0);
  assert.notEqual(readFileSync(join(root, "games", "secret-game", "CMakeLists.txt"), "utf8"), "existing private file\n");
  assert.equal(existsSync(join(root, "games", "secret-game", ".git")), true);
});

test("new_game public --replace refuses an existing private owner without side effects", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync(process.execPath, [script, "--root", root, "--id", "secret-game", "--private"], { encoding: "utf8" });
  const destination = join(root, "games", "secret-game");
  writeFileSync(join(destination, "private-history.txt"), "private bytes\n", "utf8");
  execFileSync("git", ["add", "."], { cwd: destination, stdio: "ignore" });
  execFileSync("git", ["-c", "user.email=tests@example.invalid", "-c", "user.name=Tests", "commit", "-m", "private history"], { cwd: destination, stdio: "ignore" });
  const nestedHead = execFileSync("git", ["rev-parse", "HEAD"], { cwd: destination, encoding: "utf8" });
  const localPath = join(root, "ai_studio", "workspace", "catalog.local.json");
  const excludePath = join(root, ".git", "info", "exclude");
  const beforeLocal = readFileSync(localPath);
  const beforeExclude = readFileSync(excludePath);
  const beforePrivate = readFileSync(join(destination, "private-history.txt"));

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "secret-game", "--visibility", "public", "--replace"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /already owned by a private game/);
  assert.deepEqual(readFileSync(join(destination, "private-history.txt")), beforePrivate);
  assert.equal(execFileSync("git", ["rev-parse", "HEAD"], { cwd: destination, encoding: "utf8" }), nestedHead);
  assert.deepEqual(readFileSync(localPath), beforeLocal);
  assert.deepEqual(readFileSync(excludePath), beforeExclude);
  const publicCatalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.equal(publicCatalog.mounts.some((mount) => mount.root === "games/secret-game"), false);
});

test("new_game --replace updates the same registry entry", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync(process.execPath, [script, "--root", root, "--id", "test-game"], { encoding: "utf8" });
  execFileSync(process.execPath, [script, "--root", root, "--id", "test-game", "--title", "Replaced Game", "--replace"], { encoding: "utf8" });

  const registry = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.equal(registry.mounts.filter((mount) => mount.root === "games/test-game").length, 1);
  const taskboardProject = readFileSync(join(taskboardItems(root), "projects", "P001-test-game.md"), "utf8");
  assert.match(taskboardProject, /target: games\/test-game/);
  assert.match(taskboardProject, /title: Replaced Game/);
});

test("new_game rejects retired --force", () => {
  assert.throws(
    () => parseArgs(["--id", "test-game", "--force"]),
    /--force was retired; use explicit --replace/,
  );
});

test("new_game --template copies a registered template id", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const templateDir = join(root, "templates", "cozy-template");
  cpSync(join(root, "templates", "template"), templateDir, { recursive: true });
  for (const [rel, before, after] of [
    ["CMakeLists.txt", 'GAME_STORAGE_APP_ID="template"', 'GAME_STORAGE_APP_ID="cozy-template"'],
    ["cmake/GamePlatform.cmake", 'GAME_STORAGE_APP_ID="template"', 'GAME_STORAGE_APP_ID="cozy-template"'],
    ["cmake/GameTests.cmake", 'GAME_STORAGE_APP_ID="template_test"', 'GAME_STORAGE_APP_ID="cozy-template_test"'],
    ["src/game_save.c", 'GAME_STORAGE_APP_ID "template"', 'GAME_STORAGE_APP_ID "cozy-template"'],
    ["tests/web_persistence_check.py", 'STORAGE_KEY = "template/save/autosave"', 'STORAGE_KEY = "cozy-template/save/autosave"'],
    ["cmake/GameOptions.cmake", 'set(GAME_TITLE "Template" CACHE STRING "Game window title base")', 'set(GAME_TITLE "Cozy Template" CACHE STRING "Game window title base")'],
    ["src/main.c", 'config.app_name = "Template"', 'config.app_name = "Cozy Template"'],
    ["src/main.c", 'GAME_WINDOW_TITLE "Template"', 'GAME_WINDOW_TITLE "Cozy Template"'],
  ]) {
    const path = join(templateDir, rel);
    writeFileSync(path, readFileSync(path, "utf8").split(before).join(after), "utf8");
  }
  writeFileSync(join(templateDir, "assets", "cozy.txt"), "asset\n", "utf8");
  writeFileSync(join(templateDir, "template.json"), JSON.stringify({ schema: "ai_studio.template.v1", id: "cozy-template", title: "Cozy Template", storageNamespace: "cozy-template" }), "utf8");
  writeFileSync(join(templateDir, "game-dependencies.json"), JSON.stringify({ schema: "ai_studio.game.dependencies.seed.v2", engine: { source: "external/neotolis-engine", version: "0.1.0", compatibility: "tested" }, features: [], compatibility: "tested" }), "utf8");
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
  mkdirSync(join(root, "templates", "template"), { recursive: true });
  writeFileSync(join(root, "templates", "template", "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.20)\n", "utf8");
  writeFileSync(join(root, "templates", "template", "template.json"), JSON.stringify({
    schema: "ai_studio.template.v1", id: "template", title: "Template", storageNamespace: "template",
  }), "utf8");
  writeFileSync(join(root, "templates", "template", "game-dependencies.json"), JSON.stringify({
    schema: "ai_studio.game.dependencies.seed.v2", engine: { source: "external/neotolis-engine", version: "0.1.0", compatibility: "tested" }, features: [], compatibility: "tested",
  }), "utf8");

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

test("new_game usage text documents visibility", () => {
  const result = usageText();

  assert.match(result, /usage: node games\/new_game\.mjs/);
  assert.match(result, /--visibility public\|private/);
  assert.match(result, /omitting --visibility still creates a public\/tracked game/);
});

test("new_game argument parser rejects unknown arguments", () => {
  assert.throws(() => parseArgs(["--wat"]), /unknown argument: --wat/);
});

test("new_game --help preserves the CLI success envelope", () => {
  const result = spawnSync(process.execPath, [script, "--help"], { encoding: "utf8" });

  assert.equal(result.status, 0);
  assert.equal(result.stderr, "");
  assert.equal(result.stdout, `${usageText()}\n`);
});

test("new_game unknown arguments preserve the CLI error envelope", () => {
  const result = spawnSync(process.execPath, [script, "--wat"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.equal(result.stdout, "");
  assert.equal(result.stderr, `error: unknown argument: --wat\n${usageText()}\n`);
});

test("existing destination is byte-safe without --replace", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const destination = join(root, "games", "test-game");
  mkdirSync(destination, { recursive: true });
  writeFileSync(join(destination, "stale.txt"), "keep exactly\n", "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "test-game"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.equal(readFileSync(join(destination, "stale.txt"), "utf8"), "keep exactly\n");
  assert.deepEqual(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8").includes("games/test-game"), false);
});

test("--replace publishes a complete tree and removes stale destination files", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const destination = join(root, "games", "test-game");
  mkdirSync(destination, { recursive: true });
  writeFileSync(join(destination, "stale.txt"), "obsolete\n", "utf8");

  execFileSync(process.execPath, [script, "--root", root, "--id", "test-game", "--replace"], { encoding: "utf8" });

  assert.equal(existsSync(join(destination, "stale.txt")), false);
  assert.equal(existsSync(join(destination, "game.json")), true);
  assert.deepEqual(readdirSync(join(root, "games")), ["test-game"]);
});

test("invalid title and storage identity reject before publish", () => {
  for (const [argv, expected] of [
    [["--title", "  padded"], /game title must be 1-80 trimmed characters/],
    [["--storage-namespace", "Bad Namespace"], /storage namespace must be lowercase kebab-case/],
  ]) {
    const args = parseArgs(["--id", "bad-game", ...argv]);
    assert.throws(() => validateRequestedIdentity(args), expected);
  }
});

test("identity is reread from game.json and drives title, storage, tests, IDE, and taskboard", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  execFileSync(process.execPath, [script, "--root", root, "--id", "moon-game", "--title", "Moon Game", "--storage-namespace", "moon-save"], { encoding: "utf8" });

  const gameDir = join(root, "games", "moon-game");
  assert.deepEqual(JSON.parse(readFileSync(join(gameDir, "game.json"), "utf8")), {
    schema: "ai_studio.game.v1", id: "moon-game", title: "Moon Game", storageNamespace: "moon-save",
  });
  assert.match(readFileSync(join(gameDir, "cmake", "GameOptions.cmake"), "utf8"), /GAME_TITLE "Moon Game"/);
  assert.match(readFileSync(join(gameDir, "CMakeLists.txt"), "utf8"), /GAME_STORAGE_APP_ID="moon-save"/);
  assert.match(readFileSync(join(gameDir, "cmake", "GameTests.cmake"), "utf8"), /GAME_STORAGE_APP_ID="moon-save-test"/);
  assert.match(readFileSync(join(gameDir, "tests", "web_persistence_check.py"), "utf8"), /moon-save\/save\/autosave/);
  assert.match(readFileSync(join(gameDir, "src", "main.c"), "utf8"), /config\.app_name = "Moon Game"/);
  assert.match(readFileSync(join(taskboardItems(root), "projects", "P001-moon-game.md"), "utf8"), /title: Moon Game/);
  assert.match(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"), /moon-game/);
});

test("two generated games have unique runtime and test storage namespaces", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  for (const id of ["alpha-game", "beta-game"]) {
    execFileSync(process.execPath, [script, "--root", root, "--id", id], { encoding: "utf8" });
    assert.match(readFileSync(join(root, "games", id, "CMakeLists.txt"), "utf8"), new RegExp(`GAME_STORAGE_APP_ID="${id}"`));
    assert.match(readFileSync(join(root, "games", id, "cmake", "GameTests.cmake"), "utf8"), new RegExp(`GAME_STORAGE_APP_ID="${id}-test"`));
  }
});

test("different game ids cannot claim the same storage namespace", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync(process.execPath, [script, "--root", root, "--id", "alpha-game", "--storage-namespace", "shared-save"], { encoding: "utf8" });
  const beforeCatalog = readFileSync(join(root, "ai_studio", "workspace", "catalog.json"));

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "beta-game", "--storage-namespace", "shared-save"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /storage namespace 'shared-save' is already owned by game 'alpha-game'/);
  assert.equal(existsSync(join(root, "games", "beta-game")), false);
  assert.deepEqual(readFileSync(join(root, "ai_studio", "workspace", "catalog.json")), beforeCatalog);
});

test("parallel same-id creators serialize and never overwrite the winner", async (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const barrier = join(root, "same-id-barrier");
  const env = { NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_LOCK_BARRIER: barrier };
  const first = runNewGame(["--root", root, "--id", "race-game", "--title", "First Winner"], env);
  await waitForFile(`${barrier}.ready`);
  const second = runNewGame(["--root", root, "--id", "race-game", "--title", "Second Loser"], env);
  writeFileSync(`${barrier}.release`, "release\n", "utf8");
  const results = await Promise.all([first, second]);

  assert.equal(results.filter((result) => result.status === 0).length, 1);
  assert.equal(results.filter((result) => result.status === 1).length, 1);
  assert.match(results.find((result) => result.status === 1).stderr, /already exists/);
  assert.equal(JSON.parse(readFileSync(join(root, "games", "race-game", "game.json"), "utf8")).title, "First Winner");
  const catalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.equal(catalog.mounts.filter((mount) => mount.root === "games/race-game").length, 1);
});

test("parallel different ids cannot lose mounts or share a storage namespace", async (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const barrier = join(root, "namespace-barrier");
  const env = { NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_LOCK_BARRIER: barrier };
  const first = runNewGame(["--root", root, "--id", "alpha-game", "--storage-namespace", "race-save"], env);
  await waitForFile(`${barrier}.ready`);
  const second = runNewGame(["--root", root, "--id", "beta-game", "--storage-namespace", "race-save"], env);
  writeFileSync(`${barrier}.release`, "release\n", "utf8");
  const results = await Promise.all([first, second]);

  assert.equal(results.filter((result) => result.status === 0).length, 1);
  assert.equal(results.filter((result) => result.status === 1).length, 1);
  assert.match(results.find((result) => result.status === 1).stderr, /storage namespace 'race-save' is already owned/);
  const catalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  const gameMounts = catalog.mounts.filter((mount) => mount.kind === "game");
  assert.deepEqual(gameMounts.map((mount) => mount.root), ["games/alpha-game"]);
  assert.equal(existsSync(join(root, "games", "beta-game")), false);
});

test("an abandoned unpublished lock candidate cannot brick new-game creation", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const abandoned = join(root, "games", ".new-game.claim-dead-owner.candidate");
  mkdirSync(abandoned, { recursive: true });
  writeFileSync(join(abandoned, "partial-owner.json"), "partial\n", "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "healthy-game"], { encoding: "utf8" });

  assert.equal(result.status, 0);
  assert.equal(existsSync(join(root, "games", "healthy-game", "game.json")), true);
});

test("new-game removes only exact abandoned dead-owner claim candidates", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const dead = spawnSync(process.execPath, ["-e", "process.exit(0)"]);
  const token = `${dead.pid}-123e4567-e89b-42d3-a456-426614174000`;
  const abandoned = join(root, "games", `.new-game.claim-${token}.candidate`);
  const lookalike = join(root, "games", `.new-game.claim-${token}.candidate-keep`);
  const liveToken = `${process.pid}-123e4567-e89b-42d3-a456-426614174001`;
  const live = join(root, "games", `.new-game.claim-${liveToken}.candidate`);
  const released = join(root, "games", `.new-game.claim.release-${dead.pid}-123e4567-e89b-42d3-a456-426614174002`);
  const freshReleased = join(root, "games", `.new-game.claim.release-${dead.pid}-123e4567-e89b-42d3-a456-426614174003`);
  mkdirSync(abandoned, { recursive: true });
  mkdirSync(lookalike, { recursive: true });
  mkdirSync(live, { recursive: true });
  mkdirSync(released, { recursive: true });
  mkdirSync(freshReleased, { recursive: true });
  writeFileSync(join(abandoned, "owner.json"), `${JSON.stringify({ token, pid: dead.pid })}\n`, "utf8");
  writeFileSync(join(live, "owner.json"), `${JSON.stringify({ token: liveToken, pid: process.pid })}\n`, "utf8");
  const old = new Date(Date.now() - 10 * 60 * 1000);
  utimesSync(live, old, old);
  utimesSync(released, old, old);

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "cleanup-game"], { encoding: "utf8" });

  assert.equal(result.status, 0);
  assert.equal(existsSync(abandoned), false);
  assert.equal(existsSync(lookalike), true);
  assert.equal(existsSync(live), true);
  assert.equal(existsSync(released), false);
  assert.equal(existsSync(freshReleased), true);
});

test("identity transform fails closed on a drifted required token", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const destination = join(root, "games", "drift-game");
  mkdirSync(destination, { recursive: true });
  writeFileSync(join(destination, "old.txt"), "old destination\n", "utf8");
  const options = join(root, "templates", "template", "cmake", "GameOptions.cmake");
  writeFileSync(options, "set(DRIFTED_GAME_TITLE \"Template\")\n", "utf8");
  execFileSync("git", ["add", "templates/template/cmake/GameOptions.cmake"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["commit", "-m", "drift identity token"], { cwd: root, stdio: "ignore" });
  const beforeCatalog = readFileSync(join(root, "ai_studio", "workspace", "catalog.json"));

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "drift-game", "--replace"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /identity-owned template token drifted in cmake[\\/]GameOptions\.cmake/);
  assert.equal(readFileSync(join(destination, "old.txt"), "utf8"), "old destination\n");
  assert.deepEqual(readFileSync(join(root, "ai_studio", "workspace", "catalog.json")), beforeCatalog);
  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), false);
});

test("late public registration failure restores destination and all external bytes", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const beforeCatalog = readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "rollback-game"], {
    encoding: "utf8", env: { ...process.env, NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "public-registration" },
  });

  assert.equal(result.status, 1);
  assert.equal(existsSync(join(root, "games", "rollback-game")), false);
  assert.equal(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"), beforeCatalog);
  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), false);
  assert.equal(existsSync(join(taskboardItems(root), "projects", "P001-rollback-game.md")), false);
  assert.deepEqual(readdirSync(join(root, "games")), []);
});

test("late public failure preserves concurrent catalog, IDE, Taskboard, and counter writes", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "rollback-game"], {
    encoding: "utf8",
    env: {
      ...process.env,
      NODE_ENV: "test",
      AI_STUDIO_NEW_GAME_TEST_CONCURRENT_SENTINEL: "1",
      AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "public-registration",
    },
  });

  assert.equal(result.status, 1);
  assert.equal(existsSync(join(root, "games", "rollback-game")), false);
  const catalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.equal(catalog.mounts.some((mount) => mount.root === "games/rollback-game"), false);
  assert.equal(catalog.mounts.some((mount) => mount.root === "games/concurrent-sentinel"), true);
  assert.match(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"), /concurrent-sentinel/);
  assert.equal(existsSync(join(taskboardItems(root), "projects", "P777-concurrent-sentinel.md")), true);
  assert.deepEqual(JSON.parse(readFileSync(join(taskboardItems(root), ".counters.json"), "utf8")), { project: 777, epic: 55, task: 9999 });
});

test("late public failure preserves external writes made before post-write capture", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "rollback-game"], {
    encoding: "utf8",
    env: {
      ...process.env,
      NODE_ENV: "test",
      AI_STUDIO_NEW_GAME_TEST_CONCURRENT_SENTINEL_AT: "before-post-capture",
      AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "public-registration",
    },
  });

  assert.equal(result.status, 1);
  assert.equal(existsSync(join(root, "games", "rollback-game")), false);
  const catalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.equal(catalog.mounts.some((mount) => mount.root === "games/rollback-game"), false);
  assert.equal(catalog.mounts.some((mount) => mount.root === "games/concurrent-sentinel"), true);
  assert.match(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"), /concurrent-sentinel/);
  assert.match(readFileSync(join(root, ".git", "info", "exclude"), "utf8"), /\/concurrent-sentinel\//);
});

test("failure after existing Taskboard title update CAS-restores only the owned mutation", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync(process.execPath, [script, "--root", root, "--id", "rollback-game", "--title", "Original Title"], { encoding: "utf8" });
  const projectPath = join(taskboardItems(root), "projects", readdirSync(join(taskboardItems(root), "projects")).find((name) => name.startsWith("P001-")));
  const beforeProject = readFileSync(projectPath);

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "rollback-game", "--title", "Changed Title", "--replace"], {
    encoding: "utf8",
    env: {
      ...process.env, NODE_ENV: "test",
      AI_STUDIO_NEW_GAME_TEST_CONCURRENT_SENTINEL: "1",
      AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "after-taskboard",
    },
  });

  assert.equal(result.status, 1);
  assert.deepEqual(readFileSync(projectPath), beforeProject);
  assert.equal(JSON.parse(readFileSync(join(root, "games", "rollback-game", "game.json"), "utf8")).title, "Original Title");
  const catalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.equal(catalog.mounts.some((mount) => mount.root === "games/concurrent-sentinel"), true);
  assert.equal(existsSync(join(taskboardItems(root), "projects", "P777-concurrent-sentinel.md")), true);
  assert.deepEqual(JSON.parse(readFileSync(join(taskboardItems(root), ".counters.json"), "utf8")), { project: 777, epic: 55, task: 9999 });
  assert.doesNotMatch(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"), /Changed Title/);
});

test("failure after creating a Taskboard project CAS-removes only that new file", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "created-rollback"], {
    encoding: "utf8",
    env: {
      ...process.env, NODE_ENV: "test",
      AI_STUDIO_NEW_GAME_TEST_CONCURRENT_SENTINEL: "1",
      AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "after-taskboard",
    },
  });

  assert.equal(result.status, 1);
  assert.equal(existsSync(join(root, "games", "created-rollback")), false);
  const projectNames = readdirSync(join(taskboardItems(root), "projects"));
  assert.equal(projectNames.some((name) => name.includes("created-rollback")), false);
  assert.equal(projectNames.includes("P777-concurrent-sentinel.md"), true);
  assert.deepEqual(JSON.parse(readFileSync(join(taskboardItems(root), ".counters.json"), "utf8")), { project: 777, epic: 55, task: 9999 });
});

test("rollback phases continue after an injected Taskboard inverse failure", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const beforeCatalog = readFileSync(join(root, "ai_studio", "workspace", "catalog.json"));

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "residual-game"], {
    encoding: "utf8",
    env: {
      ...process.env, NODE_ENV: "test",
      AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "after-taskboard",
      AI_STUDIO_NEW_GAME_TEST_FAIL_ROLLBACK_AT: "taskboard",
    },
  });

  assert.equal(result.status, 1);
  assert.equal(existsSync(join(root, "games", "residual-game")), false);
  assert.deepEqual(readFileSync(join(root, "ai_studio", "workspace", "catalog.json")), beforeCatalog);
  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), false);
  const residual = readdirSync(join(taskboardItems(root), "projects")).find((name) => name.includes("residual-game"));
  assert.ok(residual);
  assert.match(result.stderr, new RegExp(`rollback residual taskboard: .*${residual.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}`));
});

test("partial backup cleanup failure is post-commit and retains the published replacement", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const destination = join(root, "games", "cleanup-pending-game");
  mkdirSync(destination, { recursive: true });
  writeFileSync(join(destination, "old-a.txt"), "old a\n", "utf8");
  writeFileSync(join(destination, "old-b.txt"), "old b\n", "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "cleanup-pending-game", "--replace"], {
    encoding: "utf8",
    env: { ...process.env, NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "backup-cleanup-partial" },
  });

  assert.equal(result.status, 0);
  assert.equal(existsSync(join(destination, "game.json")), true);
  assert.equal(existsSync(join(destination, "old-a.txt")), false);
  const catalog = JSON.parse(readFileSync(join(root, "ai_studio", "workspace", "catalog.json"), "utf8"));
  assert.equal(catalog.mounts.some((mount) => mount.root === "games/cleanup-pending-game"), true);
  assert.match(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"), /cleanup-pending-game/);
  assert.ok(readdirSync(join(taskboardItems(root), "projects")).some((name) => name.includes("cleanup-pending-game")));
  const warning = result.stderr.match(/backup cleanup pending at (.+): injected partial backup cleanup failure/);
  assert.ok(warning);
  assert.equal(existsSync(warning[1]), true);
  assert.equal(readdirSync(warning[1]).length, 1);
});

test("replacement rollback restores the exact old destination", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const destination = join(root, "games", "rollback-game");
  mkdirSync(destination, { recursive: true });
  writeFileSync(join(destination, "old.txt"), "old bytes\n", "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "rollback-game", "--replace"], {
    encoding: "utf8", env: { ...process.env, NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "public-registration" },
  });

  assert.equal(result.status, 1);
  assert.equal(readFileSync(join(destination, "old.txt"), "utf8"), "old bytes\n");
  assert.equal(existsSync(join(destination, "game.json")), false);
});

test("late private preflight failure restores exclude/catalog and leaves no game", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const excludePath = join(root, ".git", "info", "exclude");
  const beforeExclude = existsSync(excludePath) ? readFileSync(excludePath, "utf8") : null;

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "secret-game", "--private"], {
    encoding: "utf8", env: { ...process.env, NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "private-preflight" },
  });

  assert.equal(result.status, 1);
  assert.equal(existsSync(join(root, "games", "secret-game")), false);
  assert.equal(existsSync(join(root, "ai_studio", "workspace", "catalog.local.json")), false);
  assert.equal(existsSync(excludePath) ? readFileSync(excludePath, "utf8") : null, beforeExclude);
});

test("private rollback preserves unrelated exclude bytes written before post-write capture", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const excludePath = join(root, ".git", "info", "exclude");
  const beforeExclude = "# local excludes\r\n\r\n/custom-cache/\r\n";
  writeFileSync(excludePath, beforeExclude, "utf8");

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "secret-game", "--private"], {
    encoding: "utf8",
    env: {
      ...process.env,
      NODE_ENV: "test",
      AI_STUDIO_NEW_GAME_TEST_CONCURRENT_SENTINEL_AT: "before-post-capture",
      AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "private-preflight",
    },
  });

  assert.equal(result.status, 1);
  assert.equal(existsSync(join(root, "games", "secret-game")), false);
  assert.equal(readFileSync(excludePath, "utf8"), `${beforeExclude}/concurrent-sentinel/\n`);
});

test("successful and failed runs leave no staging or backup siblings", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync(process.execPath, [script, "--root", root, "--id", "good-game"], { encoding: "utf8" });
  spawnSync(process.execPath, [script, "--root", root, "--id", "bad-game"], {
    encoding: "utf8", env: { ...process.env, NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "public-registration" },
  });
  const names = readdirSync(join(root, "games"));
  assert.deepEqual(names.filter((name) => name.includes(".new-") || name.includes(".backup-")), []);
});
