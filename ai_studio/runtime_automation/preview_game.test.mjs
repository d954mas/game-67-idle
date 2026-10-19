import assert from "node:assert/strict";
import { existsSync, mkdtempSync, mkdirSync, readFileSync, realpathSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import test from "node:test";

import { listWorkspaceMounts } from "../workspace/catalog.mjs";
import { createPreviewPlan, executePreviewPlan, parsePreviewArgs, resolveGameDir, resolveGameModule } from "./preview_game.mjs";

const studioRoot = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function builder(plan) {
  return {
    parseBuildArgs(argv) {
      return { preset: argv[1], target: "local" };
    },
    createBuildPlan() { return plan; },
    executeBuildPlan() { return "built game"; },
  };
}

test("preview accepts only canonical ids and debug presets", () => {
  assert.deepEqual(parsePreviewArgs(["--game", "demo", "--preset", "wasm-debug"]), {
    gameId: "demo", buildArgs: ["--preset", "wasm-debug"],
  });
  assert.throws(() => parsePreviewArgs(["--game", "../demo"]), /canonical game id/i);
  assert.throws(() => createPreviewPlan({
    builder: builder({ steps: [], webDir: "/tmp/out" }), gameDir: "/tmp/game", buildArgs: ["--preset", "wasm-release"],
  }), /preview requires wasm-debug or wasm-devapi-debug/i);
});

test("catalog resolution requires the requested canonical identity", (t) => {
  const root = mkdtempSync(join(tmpdir(), "preview-catalog-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const game = join(root, "games", "private", "demo");
  mkdirSync(join(game, "tools"), { recursive: true });
  writeFileSync(join(game, "tools", "build_web.mjs"), "export {}\n");
  const catalog = () => [{ gameId: "demo", root: "games/private/demo" }];
  assert.equal(resolveGameDir(root, "demo", catalog), realpathSync(game));
  assert.throws(() => resolveGameDir(root, "demo", () => [{ gameId: "other", root: "games/private/demo" }]), /unique catalog mount/i);
});

test("catalog and build junctions cannot escape the selected game", (t) => {
  const root = mkdtempSync(join(tmpdir(), "preview-junction-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const outside = mkdtempSync(join(tmpdir(), "preview-outside-"));
  t.after(() => rmSync(outside, { recursive: true, force: true }));
  mkdirSync(join(outside, "tools"), { recursive: true });
  writeFileSync(join(outside, "tools", "build_web.mjs"), "export {}\n");
  mkdirSync(join(root, "games", "private"), { recursive: true });
  try { symlinkSync(outside, join(root, "games", "private", "demo"), "junction"); }
  catch (error) { t.skip(`junction fixtures unavailable: ${error.code || error.message}`); return; }
  const catalog = () => [{ gameId: "demo", root: "games/private/demo" }];
  assert.throws(() => resolveGameDir(root, "demo", catalog), /escapes the Studio root/i);

  const game = join(root, "game");
  mkdirSync(game, { recursive: true });
  symlinkSync(outside, join(game, "build"), "junction");
  assert.throws(() => executePreviewPlan({
    builder: { executeBuildPlan: () => "built" }, plan: {}, gameDir: game,
    artifactDir: join(game, "build", "bin"), previewRecord: {},
  }), /must not contain a symlink or junction/i);

  rmSync(join(game, "build"), { recursive: true, force: true });
  const deletedTarget = join(root, "deleted-build");
  mkdirSync(deletedTarget);
  symlinkSync(deletedTarget, join(game, "build"), "junction");
  rmSync(deletedTarget, { recursive: true, force: true });
  let ran = false;
  assert.throws(() => executePreviewPlan({
    builder: { executeBuildPlan: () => { ran = true; return "built"; } }, plan: {}, gameDir: game,
    artifactDir: join(game, "build", "bin"), previewRecord: {},
  }), /must not contain a symlink or junction/i);
  assert.equal(ran, false);
});

test("preview clears the CMake fingerprint and marks only the configure step", () => {
  const plan = {
    webDir: "/tmp/game/build/out",
    steps: [
      { kind: "run", args: ["--build", "/tmp/native"] },
      { kind: "run", args: ["-S", "/tmp/game", "-B", "/tmp/game/build/out", "-G", "Ninja", "-DGAME_RUNTIME_BUILD_PREVIEW=OFF", "-DGAME_RUNTIME_BUILD_FINGERPRINT=old"] },
      { kind: "run", args: ["tool", "--artifact", "/tmp/game/build/out/site"] },
    ],
  };
  const prepared = createPreviewPlan({ builder: builder(plan), gameDir: "/tmp/game", buildArgs: ["--preset", "wasm-devapi-debug"] });
  assert.deepEqual(plan.steps[1].args.slice(-3), ["-DGAME_RUNTIME_BUILD_PROFILE=wasm-devapi-debug", "-DGAME_RUNTIME_BUILD_PREVIEW=ON", "-DGAME_RUNTIME_BUILD_FINGERPRINT="]);
  assert.equal(plan.steps[1].args.filter((arg) => arg.startsWith("-DGAME_RUNTIME_BUILD_PREVIEW=")).length, 1);
  assert.equal(prepared.artifactDir, resolve("/tmp/game/build/out/site"));
  assert.deepEqual(prepared.previewRecord, {
    schema: "ai_studio.runtime_build.preview.v1", proof: "none", preset: "wasm-devapi-debug", target: "local",
  });
});

test("preview refuses injected proofs and artifact paths outside the game build root", () => {
  const plan = { webDir: "/tmp/game/build/out", steps: [{ kind: "run", args: ["-S", "/tmp/game", "-B", "/tmp/game/build/out"] }] };
  let received;
  const gameBuilder = { ...builder(plan), createBuildPlan(options) { received = options; return plan; } };
  createPreviewPlan({ builder: gameBuilder, gameDir: "/tmp/game", buildArgs: ["--preset", "wasm-debug"], options: { runtimeBuild: { fake: true }, verifyRuntimeBuild: () => {} } });
  assert.equal(received.runtimeBuild, null);
  assert.equal(received.verifyRuntimeBuild, null);
  assert.throws(() => createPreviewPlan({
    builder: builder({ webDir: "/tmp/game/build/out", steps: [{ kind: "run", args: ["-S", "/tmp/game", "-B", "/tmp/game/build/out"] }, { kind: "run", args: ["tool", "--artifact", "/outside"] }] }),
    gameDir: "/tmp/game", buildArgs: ["--preset", "wasm-debug"],
  }), /escapes its build root/i);
});

test("preview removes an old proof before execution and writes only the non-proof record", (t) => {
  const root = mkdtempSync(join(tmpdir(), "preview-game-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const artifactDir = join(root, "build", "bin");
  mkdirSync(artifactDir, { recursive: true });
  const record = join(artifactDir, "runtime-build.json");
  writeFileSync(record, '{"schema":"ai_studio.runtime_build.v1"}\n');
  let sawNoOldRecord = false;
  const gameBuilder = {
    executeBuildPlan() { sawNoOldRecord = !existsSync(record); return "built game"; },
  };
  const previewRecord = { schema: "ai_studio.runtime_build.preview.v1", proof: "none" };
  assert.match(executePreviewPlan({ builder: gameBuilder, plan: {}, gameDir: root, artifactDir, previewRecord }), /NON-PROOF/i);
  assert.ok(sawNoOldRecord);
  assert.deepEqual(JSON.parse(readFileSync(record, "utf8")), previewRecord);
});

test("catalogued private games expose a debug preview plan without a source proof", async (t) => {
  const mounts = listWorkspaceMounts(studioRoot, { includePrivate: true, kinds: ["game"] })
    .filter((mount) => mount.visibility === "private");
  if (mounts.length === 0) t.skip("no private games are available in this checkout");
  for (const mount of mounts) {
    const gameDir = resolve(studioRoot, mount.root);
    const gameBuilder = await import(pathToFileURL(join(gameDir, "tools", "build_web.mjs")).href);
    const prepared = createPreviewPlan({
      builder: gameBuilder,
      gameDir,
      buildArgs: ["--preset", "wasm-devapi-debug"],
      environment: { EMSDK: "/opt/emsdk" },
      platform: "linux",
      options: { emcmakePath: "/opt/emsdk/upstream/emscripten/emcmake", nativeConfigured: true, toolchainExists: true },
    });
    const configure = prepared.plan.steps.find((step) => step.kind === "run" && step.args.includes("-S"));
    assert.ok(configure.args.includes("-DGAME_RUNTIME_BUILD_PREVIEW=ON"));
    assert.ok(configure.args.includes("-DGAME_RUNTIME_BUILD_FINGERPRINT="));
    assert.equal(prepared.previewRecord.proof, "none");
  }
});

test("a game with shim tooling previews through the Studio builder, a copied one through its own", (t) => {
  const root = mkdtempSync(join(tmpdir(), "preview-module-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const game = join(root, "games", "fixture");
  mkdirSync(join(game, "tools"), { recursive: true });
  writeFileSync(join(game, "tools", "build_web.mjs"), "throw new Error('a shim builds on import');\n");
  mkdirSync(join(root, "ai_studio", "game_tools"), { recursive: true });
  writeFileSync(join(root, "ai_studio", "game_tools", "build_web.mjs"), "export {}\n");
  assert.equal(resolveGameModule(game, root), realpathSync(join(root, "ai_studio", "game_tools", "build_web.mjs")));

  writeFileSync(join(game, "tools", "package_web.mjs"), "export {}\n");
  assert.equal(resolveGameModule(game, root), realpathSync(join(game, "tools", "build_web.mjs")));
});
