import assert from "node:assert/strict";
import { execFileSync, spawn } from "node:child_process";
import { once } from "node:events";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { addImage, createGroup, createProject, getProject, listProjects } from "../canvas/ops.mjs";
import { solidPng } from "../canvas/tests/png_fixture.mjs";
import { __seedCachePathsForTest, seedStyleLock } from "./seed.mjs";
import { validateStyleLockFile } from "./validate.mjs";

function writeJson(path, value) {
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "style-lock-seed-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = join(root, "canvas-projects");
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(root, { recursive: true, force: true });
  });
  for (const gameId of ["past-game", "new-game"]) {
    const artDir = join(root, "games", gameId, "design", "art");
    mkdirSync(artDir, { recursive: true });
    writeJson(join(root, "games", gameId, "game.json"), {
      schema: "ai_studio.game.v1", id: gameId, title: gameId, storageNamespace: gameId,
    });
    writeJson(join(root, "games", gameId, "dependencies.json"), {
      schema: "ai_studio.game.dependencies.v2",
      engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" },
      features: [],
      compatibility: "test",
    });
    writeJson(join(artDir, "art_contract.json"), { schema: "test.art-contract.v1", game_id: gameId });
  }
  const project = createProject(root, { title: "Past game style", ownership: { kind: "game", gameId: "past-game" } });
  const worldBytes = solidPng(64, 64, [180, 80, 40]);
  const guiBytes = solidPng(64, 64, [40, 100, 180]);
  const world = addImage(root, project.id, { name: "world.png", bytes: worldBytes, x: 0, y: 0 }).element;
  const gui = addImage(root, project.id, { name: "gui.png", bytes: guiBytes, x: 96, y: 0 }).element;
  const style = createGroup(root, { projectId: project.id, name: "style", fromElements: [world.id, gui.id] }).group;
  const lock = {
    schema: "ai_studio.game.style_lock.v1",
    id: "past-game-style-v2",
    game_id: "past-game",
    status: "accepted",
    canvas_ref: `canvas://${project.id}/group/${style.id}`,
    art_contract_ref: "design/art/art_contract.json",
    prompt_preamble: "Bold readable painted shapes.",
    negative_prompt: "No photorealism or muddy UI.",
    palette: ["#112233", "#EEDD44"],
    bg_rule: { mode: "chroma", key_color: "#FF00FF", description: "Flat magenta cutout plate." },
    exemplar_refs: [
      { ref: `canvas://${project.id}/element/${world.id}`, origin: "owned", domain: "world" },
      { ref: `canvas://${project.id}/element/${gui.id}`, origin: "owned", domain: "gui" },
    ],
    asset_size: { width: 1024, height: 1024 },
    technical_gate: {
      max_spill_edge_ratio: 0.05,
      max_halo_edge_ratio: 0.04,
      max_alpha_noise_ratio: 0.03,
      max_empty_margin_ratio: 0.5,
      max_aspect_relative_error: 0.02,
    },
    model_checkpoint: null,
  };
  const sourcePath = join(root, "games", "past-game", "design", "style_lock.json");
  writeJson(sourcePath, lock);
  return { root, sourcePath, lock, worldBytes, guiBytes, projectId: project.id, world };
}

function spawnSeed(fx, mode) {
  const helper = fileURLToPath(new URL("./seed_process_fixture.mjs", import.meta.url));
  return spawn(process.execPath, [helper, mode, fx.root, "new-game", fx.sourcePath], {
    env: { ...process.env },
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function createPrivateTarget(root, gameId = "private-game") {
  const gameRoot = join(root, "games", "private", gameId);
  mkdirSync(join(gameRoot, ".ai_studio", "canvas", "projects"), { recursive: true });
  mkdirSync(join(gameRoot, "design", "art"), { recursive: true });
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["init"], { cwd: gameRoot, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), "games/private/\n");
  writeJson(join(gameRoot, "game.json"), { schema: "ai_studio.game.v1", id: gameId, title: gameId, storageNamespace: gameId });
  writeJson(join(gameRoot, "dependencies.json"), {
    schema: "ai_studio.game.dependencies.v2",
    engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" },
    features: [],
    compatibility: "test",
  });
  writeJson(join(gameRoot, "design", "art", "art_contract.json"), { schema: "test.art-contract.v1", game_id: gameId });
  return gameRoot;
}

test("--from accepted past-game lock seeds a draft lock and owned Canvas exemplar copies", (t) => {
  const fx = fixture(t);
  const result = seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath });

  const seeded = JSON.parse(readFileSync(result.lockPath, "utf8"));
  assert.equal(seeded.id, "new-game-style-v1");
  assert.equal(seeded.game_id, "new-game");
  assert.equal(seeded.status, "draft");
  assert.equal(seeded.prompt_preamble, fx.lock.prompt_preamble);
  assert.equal(seeded.negative_prompt, fx.lock.negative_prompt);
  assert.deepEqual(seeded.palette, fx.lock.palette);
  assert.deepEqual(seeded.technical_gate, fx.lock.technical_gate);
  assert.match(seeded.canvas_ref, new RegExp(`^canvas://${result.projectId}/group/`));
  assert.equal(validateStyleLockFile(result.lockPath, { workspaceRoot: fx.root }).game_id, "new-game");

  const project = getProject(fx.root, result.projectId);
  assert.deepEqual(project.ownership, { kind: "game", gameId: "new-game" });
  assert.equal(project.groups.find((group) => group.id === result.groupId)?.name, "style");
  const passport = project.groups.find((group) => group.id === result.passportId);
  assert.equal(passport?.name, "passport");
  assert.equal(passport?.parentId, result.groupId);
  assert.equal(passport?.style.prompt, fx.lock.prompt_preamble);
  assert.equal(passport?.style.ref, project.elements[0].id);
  assert.deepEqual(
    project.groups.filter((group) => group.parentId === result.groupId).map((group) => group.name).sort(),
    ["do-dont", "palette", "passport", "references"],
  );
  assert.equal(project.elements.length, 2);
  assert.ok(project.elements.every((element) => element.groupId === result.passportId));
  assert.deepEqual(project.elements.map((element) => element.meta.style_seed.domain).sort(), ["gui", "world"]);
  assert.ok(project.elements.every((element) => element.meta.style_seed.source_lock === "games/past-game/design/style_lock.json"));
  assert.deepEqual(
    project.elements.map((element) => readFileSync(join(process.env.CANVAS_PROJECTS_ROOT, result.projectId, element.src))),
    [fx.worldBytes, fx.guiBytes],
  );
  assert.equal(result.copiedExemplars, 2);
});

test("seeding fails closed before Canvas mutation for draft sources, self-seeds, and existing targets", (t) => {
  const fx = fixture(t);
  const before = listProjects(fx.root).length;
  const draft = { ...fx.lock, status: "draft" };
  writeJson(fx.sourcePath, draft);
  assert.throws(() => seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath }), /accepted past-game style lock/);
  assert.equal(listProjects(fx.root).length, before);

  writeJson(fx.sourcePath, fx.lock);
  assert.throws(() => seedStyleLock(fx.root, { gameId: "past-game", from: fx.sourcePath }), /different target game/);
  const targetPath = join(fx.root, "games", "new-game", "design", "style_lock.json");
  writeJson(targetPath, { occupied: true });
  assert.throws(() => seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath }), /already exists/);
  assert.equal(listProjects(fx.root).length, before);
});

test("seeding refuses accepted canon bytes that no longer match the content-addressed source ref", (t) => {
  const fx = fixture(t);
  const source = join(process.env.CANVAS_PROJECTS_ROOT, fx.projectId, fx.world.src);
  writeFileSync(source, Buffer.from("tampered accepted pixels"));
  const before = listProjects(fx.root).length;
  assert.throws(
    () => seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath }),
    /no longer match the accepted content-addressed Canvas reference/,
  );
  assert.equal(listProjects(fx.root).length, before);
});

test("transaction rollback is loud and removes a failed live Canvas project", (t) => {
  const fx = fixture(t);
  const before = listProjects(fx.root).length;
  assert.throws(
    () => seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath }, {
      afterProjectCreated() { throw new Error("injected project failure"); },
    }),
    /injected project failure/,
  );
  assert.equal(listProjects(fx.root).length, before);
  assert.equal(existsSync(join(fx.root, "games", "new-game", "design", "style_lock.json")), false);
});

test("restart recovers child-process crashes after project creation and lock temp staging", async (t) => {
  for (const [mode, exitCode] of [["crash-after-project", 81], ["crash-after-temp", 82]]) {
    const fx = fixture(t);
    const child = spawnSeed(fx, mode);
    assert.equal((await once(child, "exit"))[0], exitCode);
    const result = seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath });
    assert.equal(validateStyleLockFile(result.lockPath, { workspaceRoot: fx.root }).status, "draft");
    assert.equal(listProjects(fx.root).filter((project) => project.ownership?.gameId === "new-game").length, 1);
    assert.equal(readdirSync(join(fx.root, "games", "new-game", "design")).some((name) => name.endsWith(".tmp")), false);
  }
});

test("restart preserves an atomically committed pair after process death", async (t) => {
  const fx = fixture(t);
  const child = spawnSeed(fx, "crash-after-commit");
  assert.equal((await once(child, "exit"))[0], 83);
  const lockPath = join(fx.root, "games", "new-game", "design", "style_lock.json");
  assert.equal(validateStyleLockFile(lockPath, { workspaceRoot: fx.root }).status, "draft");
  assert.throws(() => seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath }), /already exists/);
  assert.equal(listProjects(fx.root).filter((project) => project.ownership?.gameId === "new-game").length, 1);
  assert.equal(existsSync(__seedCachePathsForTest(fx.root, "new-game").marker), false);
});

test("target-scoped cross-process lock lets only one concurrent seed commit", async (t) => {
  const fx = fixture(t);
  const first = spawnSeed(fx, "hold-after-project");
  let firstOutput = "";
  first.stdout.setEncoding("utf8");
  first.stdout.on("data", (chunk) => { firstOutput += chunk; });
  while (!firstOutput.includes("holding")) await new Promise((resolveWait) => setTimeout(resolveWait, 10));
  const second = spawnSeed(fx, "normal");
  const [[firstCode], [secondCode]] = await Promise.all([once(first, "exit"), once(second, "exit")]);
  assert.equal(firstCode, 0);
  assert.equal(secondCode, 1);
  assert.equal(listProjects(fx.root).filter((project) => project.ownership?.gameId === "new-game").length, 1);
  assert.equal(validateStyleLockFile(join(fx.root, "games", "new-game", "design", "style_lock.json"), { workspaceRoot: fx.root }).status, "draft");
});

test("atomic final link preserves a competing target lock and rolls back its own project", (t) => {
  const fx = fixture(t);
  const lockPath = join(fx.root, "games", "new-game", "design", "style_lock.json");
  const competing = "{\"competing\":true}\n";
  const before = listProjects(fx.root).length;
  assert.throws(
    () => seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath }, {
      afterLockTempWritten() { writeFileSync(lockPath, competing); },
    }),
    /EEXIST/,
  );
  assert.equal(readFileSync(lockPath, "utf8"), competing);
  assert.equal(listProjects(fx.root).length, before);
});

test("three processes reclaim one dead stale lock without overlapping transactions", async (t) => {
  const fx = fixture(t);
  const paths = __seedCachePathsForTest(fx.root, "new-game");
  mkdirSync(paths.dir, { recursive: true });
  writeFileSync(paths.lock, JSON.stringify({
    pid: 2147483647,
    startedAt: Date.now() - 60000,
    token: "00000000-0000-4000-8000-000000000000",
  }));
  const children = [spawnSeed(fx, "normal"), spawnSeed(fx, "normal"), spawnSeed(fx, "normal")];
  const codes = await Promise.all(children.map(async (child) => (await once(child, "exit"))[0]));
  assert.deepEqual(codes.sort(), [0, 1, 1]);
  assert.equal(listProjects(fx.root).filter((project) => project.ownership?.gameId === "new-game").length, 1);
  assert.equal(existsSync(paths.reclaim), false);
  assert.equal(validateStyleLockFile(join(fx.root, "games", "new-game", "design", "style_lock.json"), { workspaceRoot: fx.root }).status, "draft");
});

test("a later seed recovers an abandoned reclaim after its stale breaker is killed", async (t) => {
  const fx = fixture(t);
  const paths = __seedCachePathsForTest(fx.root, "new-game");
  mkdirSync(paths.dir, { recursive: true });
  writeFileSync(paths.lock, JSON.stringify({
    pid: 2147483647,
    startedAt: Date.now() - 60000,
    token: "00000000-0000-4000-8000-000000000000",
  }));

  const breaker = spawnSeed(fx, "hold-after-reclaim-unlink");
  let output = "";
  breaker.stdout.setEncoding("utf8");
  breaker.stdout.on("data", (chunk) => { output += chunk; });
  while (!output.includes("reclaimed")) await new Promise((resolveWait) => setTimeout(resolveWait, 10));
  assert.equal(existsSync(paths.lock), false);
  assert.equal(existsSync(paths.reclaim), true);
  breaker.kill("SIGKILL");
  const [code, signal] = await once(breaker, "exit");
  assert.ok(code !== 0 || signal !== null);

  const result = seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath });
  assert.equal(validateStyleLockFile(result.lockPath, { workspaceRoot: fx.root }).status, "draft");
  assert.equal(listProjects(fx.root).filter((project) => project.ownership?.gameId === "new-game").length, 1);
  assert.equal(existsSync(paths.reclaim), false);
});

test("a later seed recovers when a stale breaker is killed before unlinking the old lock", async (t) => {
  const fx = fixture(t);
  const paths = __seedCachePathsForTest(fx.root, "new-game");
  mkdirSync(paths.dir, { recursive: true });
  writeFileSync(paths.lock, JSON.stringify({
    pid: 2147483647,
    startedAt: Date.now() - 60000,
    token: "00000000-0000-4000-8000-000000000000",
  }));

  const breaker = spawnSeed(fx, "hold-after-reclaim-link");
  let output = "";
  breaker.stdout.setEncoding("utf8");
  breaker.stdout.on("data", (chunk) => { output += chunk; });
  while (!output.includes("claimed")) await new Promise((resolveWait) => setTimeout(resolveWait, 10));
  assert.equal(existsSync(paths.lock), true);
  assert.equal(existsSync(paths.reclaim), true);
  breaker.kill("SIGKILL");
  const [code, signal] = await once(breaker, "exit");
  assert.ok(code !== 0 || signal !== null);

  const result = seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath });
  assert.equal(validateStyleLockFile(result.lockPath, { workspaceRoot: fx.root }).status, "draft");
  assert.equal(listProjects(fx.root).filter((project) => project.ownership?.gameId === "new-game").length, 1);
  assert.equal(existsSync(paths.reclaim), false);
});

test("a reclaim error releases its owned claim before surfacing", (t) => {
  const fx = fixture(t);
  const paths = __seedCachePathsForTest(fx.root, "new-game");
  mkdirSync(paths.dir, { recursive: true });
  writeFileSync(paths.lock, JSON.stringify({
    pid: 2147483647,
    startedAt: Date.now() - 60000,
    token: "00000000-0000-4000-8000-000000000000",
  }));

  assert.throws(
    () => seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath }, {
      lockHooks: { afterReclaimLinked() { throw new Error("injected reclaim failure"); } },
    }),
    /injected reclaim failure/,
  );
  assert.equal(existsSync(paths.reclaim), false);
  const result = seedStyleLock(fx.root, { gameId: "new-game", from: fx.sourcePath });
  assert.equal(validateStyleLockFile(result.lockPath, { workspaceRoot: fx.root }).status, "draft");
});

test("public past lock seeds store-canonical refs into a private target Canvas", (t) => {
  const fx = fixture(t);
  const gameRoot = createPrivateTarget(fx.root);
  const result = seedStyleLock(fx.root, { gameId: "private-game", from: fx.sourcePath });
  assert.equal(result.lockPath, join(gameRoot, "design", "style_lock.json"));
  const lock = validateStyleLockFile(result.lockPath, { workspaceRoot: fx.root });
  assert.match(lock.canvas_ref, new RegExp(`^canvas://game/private-game/${result.projectId}/group/`));
  assert.ok(lock.exemplar_refs.every((entry) => entry.ref.startsWith(`canvas://game/private-game/${result.projectId}/element/`)));
  const projectPath = join(gameRoot, ".ai_studio", "canvas", "projects", result.projectId, "project.json");
  assert.equal(JSON.parse(readFileSync(projectPath, "utf8")).ownership.gameId, "private-game");
});
