import assert from "node:assert/strict";
import test, { after } from "node:test";
import { execFileSync, spawnSync } from "node:child_process";
import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";

import { main, parseArgs, resolveVisibility, usageText, validateRequestedIdentity } from "./new_game.mjs";
import { rollbackPrivateCanvasStoreTransfer } from "./new_game_canvas.mjs";

function write(root, rel, text) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, text, "utf8");
}

function buildFixtureRepo() {
  const root = mkdtempSync(join(tmpdir(), "new-game-fixture-"));
  const template = "templates/template";
  write(root, ".gitignore", "games/private/\nai_studio/assets/canvas/projects/\n");
  write(root, "ai_studio/studio.config.json", JSON.stringify({
    schema: "ai_studio.studio_config.v1",
    canvasProjectsRoot: "ai_studio/assets/canvas/projects",
  }));
  write(root, `${template}/template.json`, JSON.stringify({
    schema: "ai_studio.template.v1", id: "template", title: "Template", storageNamespace: "template",
  }));
  write(root, `${template}/game-dependencies.json`, JSON.stringify({
    schema: "ai_studio.game.dependencies.seed.v2",
    engine: { source: "external/neotolis-engine", version: "0.1.0", compatibility: "tested" },
    features: [{ id: "game-state", source: "features/game-state", version: "1.0.0", compatibility: "tested" }],
    compatibility: "tested template seed",
  }));
  write(root, `${template}/content/items.lock.json`, JSON.stringify({
    schema: "game_seed.items_lock", schema_version: 4,
    receipt: { schema: "items.release_receipt.v2", field_ids: { active: ["game.weapon.level.attack"], reserved: [] } },
    def_ids: { "tmpl.shipped": { storage: "stack", level_count: 0 } }, removed: {},
  }));
  write(root, `${template}/CMakeLists.txt`, "cmake_minimum_required(VERSION 3.20)\ninclude(cmake/GamePlatform.cmake)\n");
  write(root, `${template}/cmake/GameOptions.cmake`, "set(GAME_TITLE \"Template\" CACHE STRING \"Game window title base\")\n");
  write(root, `${template}/cmake/GamePlatform.cmake`, "target_compile_definitions(${GAME_TARGET} PRIVATE GAME_STORAGE_APP_ID=\"template\")\n");
  write(root, `${template}/cmake/GameTests.cmake`, ["storage", "save", "analytics", "composition"].map((suffix) => `target_compile_definitions(test_${suffix} PRIVATE GAME_STORAGE_APP_ID=\"template_${suffix}_test\")`).join("\n") + "\n");
  write(root, `${template}/src/game_save.c`, "#define GAME_STORAGE_APP_ID \"template\"\n");
  write(root, `${template}/src/main.c`, "config.app_name = \"Template\";\n#define GAME_WINDOW_TITLE \"Template\"\n");
  write(root, `${template}/tests/web_persistence_check.py`, "STORAGE_KEY = \"template/save/autosave\"\n");
  write(root, `${template}/assets/readme.txt`, "asset\n");
  write(root, `${template}/.gitignore`, "build/\nsrc/generated/\n.ai_studio/evidence/\n");
  write(root, `${template}/src/generated/stale.h`, "generated\n");
  write(root, `${template}/build/stale.obj`, "generated\n");
  write(root, `${template}/.ai_studio/evidence/private.txt`, "private\n");
  write(root, "features/game-state/feature.json", JSON.stringify({
    schema: "ai_studio.feature.v1", id: "game-state", version: "1.0.0",
  }));
  write(root, "features/game-state/README.md", "# game-state\n");

  const engine = join(root, "external", "neotolis-engine");
  write(engine, "engine/core/nt_core.h", [
    "#define NT_VERSION_MAJOR 0", "#define NT_VERSION_MINOR 1", "#define NT_VERSION_PATCH 0", "",
  ].join("\n"));
  write(engine, "README.md", "# engine\n");
  execFileSync("git", ["init"], { cwd: engine, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "tests@example.invalid"], { cwd: engine });
  execFileSync("git", ["config", "user.name", "Tests"], { cwd: engine });
  execFileSync("git", ["add", "."], { cwd: engine });
  execFileSync("git", ["commit", "-m", "fixture engine"], { cwd: engine, stdio: "ignore" });

  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "tests@example.invalid"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Tests"], { cwd: root });
  execFileSync("git", ["add", "."], { cwd: root });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });
  return root;
}

let fixtureRepo;
after(() => {
  if (fixtureRepo) rmSync(fixtureRepo, { recursive: true, force: true });
});

function tempRepo(t) {
  fixtureRepo ||= buildFixtureRepo();
  const root = mkdtempSync(join(tmpdir(), "new-game-"));
  cpSync(fixtureRepo, root, { recursive: true });
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return root;
}

function invokeNewGame(args, env = {}) {
  const stdout = [];
  const stderr = [];
  const previous = new Map(Object.keys(env).map((key) => [key, process.env[key]]));
  for (const [key, value] of Object.entries(env)) process.env[key] = value;
  try {
    const status = main(args, { log: (line) => stdout.push(line), error: (line) => stderr.push(line) });
    return { status, stdout: stdout.join("\n"), stderr: stderr.join("\n") };
  } finally {
    for (const [key, value] of previous) {
      if (value === undefined) delete process.env[key];
      else process.env[key] = value;
    }
  }
}

function create(root, id, visibility, extra = []) {
  const result = invokeNewGame(["--root", root, "--id", id, "--visibility", visibility, ...extra]);
  assert.equal(result.status, 0, result.stderr);
  return result.stdout;
}

function readCanvasLink(gameDir) {
  const text = readFileSync(join(gameDir, "design", "canvas.md"), "utf8");
  const value = (name) => new RegExp(`^${name}:\\s*(\\S+)\\s*$`, "m").exec(text)?.[1] || "";
  return { text, ref: value("canvas_ref"), browserUrl: value("browser_url") };
}

function canvasProjectId(ref) {
  return ref.split("/").filter(Boolean).at(-1);
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function activeCanvasProjectIds(root) {
  const projectsRoot = join(root, "ai_studio", "assets", "canvas", "projects");
  if (!existsSync(projectsRoot)) return [];
  return readdirSync(projectsRoot, { withFileTypes: true })
    .filter((entry) => entry.isDirectory() && !entry.name.startsWith("."))
    .map((entry) => entry.name);
}

test("argument contract keeps explicit visibility and strict identities", () => {
  assert.equal(resolveVisibility(parseArgs(["--id", "demo"])), "public");
  assert.equal(resolveVisibility(parseArgs(["--id", "demo", "--private"])), "private");
  assert.throws(() => resolveVisibility(parseArgs(["--id", "demo", "--require-visibility"])), /missing visibility choice/);
  assert.throws(() => validateRequestedIdentity(parseArgs(["--id", "Bad"])), /lowercase kebab-case/);
  assert.match(usageText(), /--visibility public\|private/);
});

test("public creation is discovered from its folder and updates public integrations", (t) => {
  const root = tempRepo(t);
  const output = create(root, "test-game", "public");
  const game = join(root, "games", "test-game");
  assert.match(output, /discovered assets: games\/test-game\/assets/);
  assert.equal(existsSync(join(game, "game.json")), true);
  assert.equal(existsSync(join(game, "src", "generated", "stale.h")), false);
  assert.equal(existsSync(join(game, "build", "stale.obj")), false);
  assert.equal(existsSync(join(game, ".ai_studio", "evidence", "private.txt")), false);
  assert.equal(existsSync(join(game, ".ai_studio", "taskboard", "items")), false);
  const lock = JSON.parse(readFileSync(join(game, "content", "items.lock.json"), "utf8"));
  assert.deepEqual(lock.def_ids, {});
  const tasks = JSON.parse(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"));
  assert.equal(tasks.tasks.some((task) => task.label === "Game: test-game: build native debug"), true);
  assert.equal(existsSync(join(root, "ai_studio", "taskboard", "items", "projects", "P001.md")), true);
  const canvas = readCanvasLink(game);
  const canvasId = canvasProjectId(canvas.ref);
  assert.match(canvas.ref, /^canvas:\/\/test-game-canvas-[0-9a-f]{6}$/);
  assert.equal(canvas.browserUrl, `http://127.0.0.1:8765/canvas?project=${canvasId}&store=studio`);
  assert.match(output, new RegExp(`canvas ref: ${escapeRegExp(canvas.ref)}`));
  assert.match(output, new RegExp(`canvas browser: ${escapeRegExp(canvas.browserUrl)}`));
  const canvasProject = JSON.parse(readFileSync(join(root, "ai_studio", "assets", "canvas", "projects", canvasId, "project.json"), "utf8"));
  assert.equal(canvasProject.title, "test-game Canvas");
  assert.deepEqual(canvasProject.ownership, { kind: "game", gameId: "test-game" });
});

test("private creation uses games/private, installs parent preflight, and stays invisible to parent git", (t) => {
  const root = tempRepo(t);
  const output = create(root, "secret-game", "private", ["--title", "Secret Title", "--public-alias", "Private Slot"]);
  const game = join(root, "games", "private", "secret-game");
  assert.match(output, /games\/private\/secret-game/);
  assert.equal(existsSync(join(game, ".git")), true);
  const identity = JSON.parse(readFileSync(join(game, "game.json"), "utf8"));
  assert.deepEqual(identity.aliases, ["Private Slot"]);
  const hook = readFileSync(join(root, ".git", "hooks", "pre-commit"), "utf8");
  assert.match(hook, /workspace[\\/]games\.mjs.*preflight/);
  assert.equal(execFileSync("git", ["status", "--short", "--untracked-files=all"], { cwd: root, encoding: "utf8" }).trim(), "");
  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), false);
  assert.equal(existsSync(join(root, "ai_studio", "taskboard", "items", "projects", "P001.md")), false);

  const canvas = readCanvasLink(game);
  const canvasId = canvasProjectId(canvas.ref);
  assert.match(canvas.ref, /^canvas:\/\/game\/secret-game\/secret-title-secret-game-canvas-[0-9a-f]{6}$/);
  assert.equal(canvas.browserUrl, `http://127.0.0.1:8765/canvas?project=${canvasId}&store=game%3Asecret-game`);
  const canvasProject = JSON.parse(readFileSync(join(game, ".ai_studio", "canvas", "projects", canvasId, "project.json"), "utf8"));
  assert.equal(canvasProject.title, "Secret Title [secret-game] Canvas");
  assert.deepEqual(canvasProject.ownership, { kind: "game", gameId: "secret-game" });

  write(root, "README.md", "Secret Title\n");
  execFileSync("git", ["add", "README.md"], { cwd: root });
  const commit = spawnSync("git", ["commit", "-m", "leak"], { cwd: root, encoding: "utf8" });
  assert.notEqual(commit.status, 0);
  assert.match(`${commit.stdout}\n${commit.stderr}`, /private game preflight failed/);
});

test("private preflight failure rolls publication back cleanly", (t) => {
  const root = tempRepo(t);
  write(root, "README.md", "secret-game\n");
  execFileSync("git", ["add", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "mention"], { cwd: root, stdio: "ignore" });
  const result = invokeNewGame(["--root", root, "--id", "secret-game", "--visibility", "private"]);
  assert.equal(result.status, 1);
  assert.match(result.stderr, /private game preflight failed/);
  assert.equal(existsSync(join(root, "games", "private", "secret-game")), false);
});

test("public failure after Taskboard mutation restores game, VS Code, and Taskboard state", (t) => {
  const root = tempRepo(t);
  const result = invokeNewGame(
    ["--root", root, "--id", "rollback-game", "--visibility", "public"],
    { NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "after-taskboard" },
  );
  assert.equal(result.status, 1);
  assert.equal(existsSync(join(root, "games", "rollback-game")), false);
  assert.equal(existsSync(join(root, ".vscode", "tasks.json")), false);
  assert.equal(existsSync(join(root, "ai_studio", "taskboard", "items", "projects", "P001.md")), false);
  assert.deepEqual(activeCanvasProjectIds(root), []);
});

test("replace reuses the linked Canvas project instead of creating a duplicate", (t) => {
  const root = tempRepo(t);
  create(root, "repeat-game", "public", ["--title", "Repeat Game"]);
  const game = join(root, "games", "repeat-game");
  const before = readCanvasLink(game);
  writeFileSync(join(game, "design", "canvas.md"), before.text.replace(before.ref, `${before.ref} — Repeat board`), "utf8");

  const output = create(root, "repeat-game", "public", ["--title", "Renamed Game", "--replace"]);
  const after = readCanvasLink(game);

  assert.equal(after.ref, before.ref);
  assert.deepEqual(activeCanvasProjectIds(root), [canvasProjectId(before.ref)]);
  assert.match(output, /existing canvas project:/);
  const project = JSON.parse(readFileSync(join(root, "ai_studio", "assets", "canvas", "projects", canvasProjectId(before.ref), "project.json"), "utf8"));
  assert.equal(project.title, "Renamed Game [repeat-game] Canvas");
});

test("a Canvas link write failure compensates the newly created public project", (t) => {
  const root = tempRepo(t);
  write(root, "templates/template/design/canvas.md/block.txt", "blocks the canvas.md file\n");
  execFileSync("git", ["add", "templates/template/design/canvas.md/block.txt"], { cwd: root });
  execFileSync("git", ["commit", "-m", "blocked canvas link fixture"], { cwd: root, stdio: "ignore" });

  const result = invokeNewGame(["--root", root, "--id", "blocked-link", "--visibility", "public"]);

  assert.equal(result.status, 1);
  assert.match(result.stderr, /EISDIR|illegal operation on a directory/);
  assert.equal(existsSync(join(root, "games", "blocked-link")), false);
  assert.deepEqual(activeCanvasProjectIds(root), []);
});

test("a replace link write failure restores the reused Canvas title", (t) => {
  const root = tempRepo(t);
  create(root, "rename-rollback", "public", ["--title", "Original Title"]);
  const game = join(root, "games", "rename-rollback");
  const before = readCanvasLink(game);
  const projectPath = join(root, "ai_studio", "assets", "canvas", "projects", canvasProjectId(before.ref), "project.json");
  write(root, "templates/template/design/canvas.md/block.txt", "blocks the canvas.md file\n");
  execFileSync("git", ["add", "templates/template/design/canvas.md/block.txt"], { cwd: root });
  execFileSync("git", ["commit", "-m", "blocked replace canvas link fixture"], { cwd: root, stdio: "ignore" });

  const result = invokeNewGame([
    "--root", root, "--id", "rename-rollback", "--visibility", "public",
    "--title", "New Title", "--replace",
  ]);

  assert.equal(result.status, 1);
  assert.equal(readCanvasLink(game).ref, before.ref);
  assert.equal(JSON.parse(readFileSync(projectPath, "utf8")).title, "Original Title [rename-rollback] Canvas");
});

test("a linked Canvas ref from another store is rejected without replacing the game", (t) => {
  const root = tempRepo(t);
  create(root, "wrong-store", "public");
  const game = join(root, "games", "wrong-store");
  const before = readCanvasLink(game);
  write(root, "games/wrong-store/keep.txt", "keep\n");
  writeFileSync(
    join(game, "design", "canvas.md"),
    before.text.replace(before.ref, `canvas://game/wrong-store/${canvasProjectId(before.ref)}`),
    "utf8",
  );

  const result = invokeNewGame(["--root", root, "--id", "wrong-store", "--visibility", "public", "--replace"]);

  assert.equal(result.status, 1);
  assert.match(result.stderr, /invalid project id|does not belong to studio/);
  assert.equal(readFileSync(join(game, "keep.txt"), "utf8"), "keep\n");
  assert.deepEqual(activeCanvasProjectIds(root), [canvasProjectId(before.ref)]);
});

test("private replace preserves and reuses the linked game-local Canvas project", (t) => {
  const root = tempRepo(t);
  create(root, "repeat-secret", "private", ["--title", "Repeat Secret"]);
  const game = join(root, "games", "private", "repeat-secret");
  const before = readCanvasLink(game);
  const projectId = canvasProjectId(before.ref);
  write(root, `games/private/repeat-secret/.ai_studio/canvas/projects/${projectId}/files/keep.txt`, "keep\n");

  const output = create(root, "repeat-secret", "private", ["--title", "Repeat Secret", "--replace"]);
  const after = readCanvasLink(game);

  assert.equal(after.ref, before.ref);
  assert.equal(readFileSync(join(game, ".ai_studio", "canvas", "projects", projectId, "files", "keep.txt"), "utf8"), "keep\n");
  assert.match(output, /existing canvas project:/);
});

test("private replace rollback restores the previous game-local Canvas store", (t) => {
  const root = tempRepo(t);
  create(root, "stable-secret", "private", ["--title", "Stable Secret"]);
  const game = join(root, "games", "private", "stable-secret");
  const before = readCanvasLink(game);
  const projectId = canvasProjectId(before.ref);
  write(root, "games/private/stable-secret/keep.txt", "game\n");
  write(root, `games/private/stable-secret/.ai_studio/canvas/projects/${projectId}/files/keep.txt`, "canvas\n");

  const result = invokeNewGame(
    ["--root", root, "--id", "stable-secret", "--visibility", "private", "--title", "Stable Secret", "--replace"],
    { NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "private-preflight" },
  );

  assert.equal(result.status, 1);
  assert.match(result.stderr, /injected test failure at private-preflight/);
  assert.equal(readFileSync(join(game, "keep.txt"), "utf8"), "game\n");
  assert.equal(readFileSync(join(game, ".ai_studio", "canvas", "projects", projectId, "files", "keep.txt"), "utf8"), "canvas\n");
});

test("private Canvas transfer rollback never deletes an unexpected source or target", (t) => {
  const root = tempRepo(t);
  const source = join(root, "backup", "projects");
  const target = join(root, "published", "projects");
  write(root, "backup/projects/source.txt", "source\n");
  write(root, "published/projects/target.txt", "target\n");

  assert.throws(() => rollbackPrivateCanvasStoreTransfer({ source, target }), /rollback target already exists/);
  assert.equal(readFileSync(join(source, "source.txt"), "utf8"), "source\n");
  assert.equal(readFileSync(join(target, "target.txt"), "utf8"), "target\n");
});

test("replace rollback restores the previous game directory", (t) => {
  const root = tempRepo(t);
  create(root, "stable-game", "public");
  write(root, "games/stable-game/keep.txt", "keep\n");
  const result = invokeNewGame(
    ["--root", root, "--id", "stable-game", "--visibility", "public", "--replace"],
    { NODE_ENV: "test", AI_STUDIO_NEW_GAME_TEST_FAIL_AT: "public-registration" },
  );
  assert.equal(result.status, 1);
  assert.equal(readFileSync(join(root, "games", "stable-game", "keep.txt"), "utf8"), "keep\n");
});

test("dependency seed drift is rejected before publication", (t) => {
  const root = tempRepo(t);
  const path = join(root, "templates", "template", "game-dependencies.json");
  const seed = JSON.parse(readFileSync(path, "utf8"));
  seed.features[0].version = "1.1.0";
  writeFileSync(path, JSON.stringify(seed), "utf8");
  execFileSync("git", ["add", "templates/template/game-dependencies.json"], { cwd: root });
  execFileSync("git", ["commit", "-m", "drift"], { cwd: root, stdio: "ignore" });
  const result = invokeNewGame(["--root", root, "--id", "bad-game", "--visibility", "public"]);
  assert.equal(result.status, 1);
  assert.match(result.stderr, /dependency seed version 1\.1\.0/);
  assert.equal(existsSync(join(root, "games", "bad-game")), false);
});
