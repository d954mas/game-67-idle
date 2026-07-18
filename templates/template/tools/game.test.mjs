import assert from "node:assert/strict";
import { cpSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { execFileSync, spawnSync } from "node:child_process";
import test from "node:test";

import { doctorGame, executeGameCommand, nativeTestPlan, parseGameArgs } from "./game.mjs";
import { findStudioRoot } from "./lib/studio_root.mjs";
import { createRuntimeBuildRecord } from "./lib/runtime_build.mjs";

const gameModuleRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const studioRoot = findStudioRoot(gameModuleRoot);
const RELEASE_WASM = Buffer.from([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
  0x03, 0x02, 0x01, 0x00,
  0x05, 0x03, 0x01, 0x00, 0x01,
  0x07, 0x10, 0x02,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00,
  0x03, 0x72, 0x75, 0x6e, 0x00, 0x00,
  0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

function runtimeBoundWasm(record) {
  const name = Buffer.from("runtime_build", "ascii");
  const marker = Buffer.from(`ai_studio.runtime_build:${record.fingerprint}`, "ascii");
  const payloadSize = 1 + name.length + marker.length;
  assert.ok(payloadSize < 128);
  return Buffer.concat([RELEASE_WASM, Buffer.from([0, payloadSize, name.length]), name, marker]);
}

function write(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, value);
}

function git(cwd, args) {
  return execFileSync("git", args, {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  }).trim();
}

test("game CLI exposes game-owned lifecycle commands with fail-closed arguments", () => {
  assert.deepEqual(parseGameArgs(["verify", "--target", "poki", "--no-build", "--template-proof", "--skip-tests"]), {
    command: "verify", target: "poki", build: false, templateProof: true, skipTests: true, outDir: "",
  });
  for (const command of ["doctor", "build", "run", "test", "playable", "package", "verify"]) {
    assert.equal(parseGameArgs([command]).command, command);
  }
  assert.throws(() => parseGameArgs(["unknown"]), /usage:/);
  assert.throws(() => parseGameArgs(["package", "--target", "bad"]), /unknown target/);
  assert.throws(() => parseGameArgs(["doctor", "--no-build"]), /not valid/i);
  assert.throws(() => parseGameArgs(["verify", "--skip-tests"]), /template-proof/i);
});

test("doctor requires the copied game-owned scaffold and exact dependency record", (t) => {
  const gameDir = mkdtempSync(join(tmpdir(), "game-doctor-"));
  t.after(() => rmSync(gameDir, { recursive: true, force: true }));
  assert.throws(() => doctorGame({ gameDir }), /missing/i);

  for (const rel of ["CMakeLists.txt", "tools/game.mjs", "tools/build_web.mjs", "tools/package_web.mjs", "tools/lib/studio_root.mjs", "tools/lib/zip_store.mjs", "tools/lib/runtime_build.mjs", "tools/serve_web.mjs", "release/README.md", ".github/workflows/game-verify.yml"]) {
    write(join(gameDir, rel), "fixture\n");
  }
  write(join(gameDir, "game.json"), JSON.stringify({ schema: "ai_studio.game.v1", id: "fixture", title: "Fixture", storageNamespace: "fixture" }));
  write(join(gameDir, "dependencies.json"), JSON.stringify({
    schema: "ai_studio.game.dependencies.v2",
    engine: { source: "external/neotolis-engine", version: "0.1.0", revision: "1".repeat(40), compatibility: "tested" },
    features: [], compatibility: "tested",
  }));
  assert.equal(doctorGame({ gameDir }).gameId, "fixture");

  rmSync(join(gameDir, "game.json"), { force: true });
  rmSync(join(gameDir, "dependencies.json"), { force: true });
  write(join(gameDir, "template.json"), JSON.stringify({ schema: "ai_studio.template.v1", id: "Bad ID", title: "Template", storageNamespace: "template" }));
  write(join(gameDir, "game-dependencies.json"), JSON.stringify({ schema: "ai_studio.game.dependencies.seed.v2" }));
  assert.throws(() => doctorGame({ gameDir, templateProof: true }), /template identity is invalid/i);
});

test("native game test plan configures, builds, and runs CTest without a clean rebuild", () => {
  const gameDir = "C:\\repo\\games\\example";
  const windows = nativeTestPlan(gameDir, "win32");
  assert.deepEqual(windows.map((command) => command[0]), ["cmake", "cmake", "ctest"]);
  assert.ok(windows[0].includes("-DCMAKE_BUILD_TYPE=Debug"));
  assert.equal(windows[0].some((arg) => arg.includes("sanitize")), false);
  assert.deepEqual(windows[1].slice(0, 2), ["cmake", "--build"]);
  assert.ok(windows[2].includes("--output-on-failure"));
  assert.ok(nativeTestPlan("/repo/games/example", "linux")[0].includes("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined"));
});

test("verify composes doctor tests and one package without discovering workspace games", async () => {
  const calls = [];
  const result = await executeGameCommand({
    command: "verify", target: "local", build: false, templateProof: true, skipTests: false, outDir: "",
  }, {
    gameDir: "/repo/templates/template",
    doctor: () => { calls.push("doctor"); return { gameId: "template" }; },
    loadMetadata: () => ({ dependencies: {}, proof: "reference-template" }),
    verifyDependencies: () => { calls.push("dependencies"); },
    nodeTest: () => { calls.push("node"); },
    nativeTest: () => { calls.push("native"); },
    package: (options) => { calls.push(["package", options.build, options.target]); return { zipPath: "release/template-local.zip" }; },
  });
  assert.deepEqual(calls, ["doctor", "dependencies", "node", "native", ["package", false, "local"]]);
  assert.match(result.message, /template-local\.zip/);
});

test("game test and plain verify require dependency proof plus Node and native CTest gates", async () => {
  for (const command of ["test", "verify"]) {
    const calls = [];
    await executeGameCommand({
      command, target: "itch", build: false, templateProof: false, skipTests: false, outDir: "",
    }, {
      gameDir: "/repo/games/example",
      doctor: () => ({ gameId: "example" }),
      loadMetadata: () => ({ dependencies: {}, proof: "game" }),
      verifyDependencies: () => calls.push("dependencies"),
      nodeTest: () => calls.push("node"),
      nativeTest: () => calls.push("native"),
      package: () => { calls.push("package"); return { zipPath: "example.zip" }; },
    });
    assert.deepEqual(calls, command === "test"
      ? ["dependencies", "node", "native"]
      : ["dependencies", "node", "native", "package"]);
  }
});

test("template test bypass validates identity before the parent proof can skip tests", async () => {
  const calls = [];
  await assert.rejects(
    executeGameCommand({
      command: "verify", target: "itch", build: false, templateProof: true, skipTests: true, outDir: "",
    }, {
      gameDir: "/repo/templates/template",
      doctor: () => { calls.push("doctor"); throw new Error("template identity is invalid"); },
      loadMetadata: () => ({ dependencies: {}, proof: "reference-template" }),
      verifyDependencies: () => calls.push("dependencies"),
      package: () => { calls.push("package"); return { zipPath: "template.zip" }; },
    }),
    /template identity is invalid/,
  );
  assert.deepEqual(calls, ["doctor"]);
});

test("copied game CLI executes doctor and final package from a real games/private/<id> dependency layout", (t) => {
  const root = mkdtempSync(join(tmpdir(), "copied-game-cli-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const gameDir = join(root, "games", "private", "copied-game");
  cpSync(join(gameModuleRoot, "tools"), join(gameDir, "tools"), { recursive: true });
  cpSync(join(gameModuleRoot, ".github"), join(gameDir, ".github"), { recursive: true });
  mkdirSync(join(gameDir, "release"), { recursive: true });
  cpSync(join(gameModuleRoot, "release", "README.md"), join(gameDir, "release", "README.md"));
  cpSync(join(studioRoot, "features", "platform-sdk"), join(root, "features", "platform-sdk"), { recursive: true });
  write(join(gameDir, "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.25)\n");

  const engineRoot = join(root, "external", "neotolis-engine");
  write(join(engineRoot, "engine", "core", "nt_core.h"), [
    "#define NT_VERSION_MAJOR 0", "#define NT_VERSION_MINOR 1", "#define NT_VERSION_PATCH 0", "",
  ].join("\n"));
  for (const cwd of [engineRoot, root]) {
    git(cwd, ["init", "-q"]);
  }
  git(engineRoot, ["add", "."]);
  git(engineRoot, ["-c", "user.email=tests@example.invalid", "-c", "user.name=Tests", "commit", "-qm", "engine fixture"]);
  const engineRevision = git(engineRoot, ["rev-parse", "HEAD"]);
  git(root, ["add", "features/platform-sdk", "external/neotolis-engine"]);
  git(root, ["-c", "user.email=tests@example.invalid", "-c", "user.name=Tests", "commit", "-qm", "dependency fixture"]);
  const revision = git(root, ["rev-parse", "HEAD"]);

  write(join(gameDir, "game.json"), `${JSON.stringify({ schema: "ai_studio.game.v1", id: "copied-game", title: "Copied Game", storageNamespace: "copied-game" }, null, 2)}\n`);
  write(join(gameDir, "dependencies.json"), `${JSON.stringify({
    schema: "ai_studio.game.dependencies.v2",
    engine: { source: "external/neotolis-engine", version: "0.1.0", revision: engineRevision, compatibility: "tested" },
    features: [{ id: "platform-sdk", source: "features/platform-sdk", version: "1.1.0", revision, compatibility: "tested" }],
    compatibility: "copied layout fixture",
  }, null, 2)}\n`);
  const runtimeBuild = createRuntimeBuildRecord({
    gameDir,
    studioRoot: root,
    dependencies: JSON.parse(readFileSync(join(gameDir, "dependencies.json"), "utf8")),
  });
  const artifact = join(gameDir, "build", "wasm-release-itch", "bin");
  write(join(artifact, "index.html"), `<!doctype html><script>window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true, runtimeBuildFingerprint: '${runtimeBuild.fingerprint}' });</script><script src='game.js'></script>\n`);
  write(join(artifact, "game.js"), [
    "var wasmBinaryFile;",
    "function findWasmBinary() { return locateFile('game.wasm'); }",
    "async function instantiateAsync(binaryFile, imports) {",
    "  const response = fetch(binaryFile);",
    "  return WebAssembly.instantiateStreaming(response, imports);",
    "}",
    "async function createWasm() {",
    "  wasmBinaryFile ??= findWasmBinary();",
    "  return instantiateAsync(wasmBinaryFile, {});",
    "}",
    "createWasm();",
    "",
  ].join("\n"));
  write(join(artifact, "game.wasm"), runtimeBoundWasm(runtimeBuild));
  write(join(artifact, "assets", "game.ntpack"), "pack");
  write(join(artifact, "runtime-build.json"), `${JSON.stringify(runtimeBuild, null, 2)}\n`);
  for (const [from, to] of [
    ["platform-sdk.js", "platform-sdk.js"],
    ["platform-sdk-core.js", "platform-sdk-core.js"],
    ["adapters/mock.js", "platform-sdk-adapter.js"],
  ]) cpSync(join(root, "features", "platform-sdk", "web", from), join(artifact, to));

  const doctor = spawnSync(process.execPath, ["tools/game.mjs", "doctor"], { cwd: gameDir, encoding: "utf8" });
  assert.equal(doctor.status, 0, doctor.stderr);
  assert.match(doctor.stdout, /doctor passed \(copied-game\)/);
  const packaged = spawnSync(process.execPath, ["tools/game.mjs", "package", "--target", "itch", "--no-build"], { cwd: gameDir, encoding: "utf8" });
  assert.equal(packaged.status, 0, packaged.stderr);
  assert.match(packaged.stdout, /package passed:.*copied-game-itch-[0-9a-f]{16}\.zip/i);
  assert.equal(readdirSync(join(gameDir, "release", "artifacts")).filter((name) => /\.(?:zip|manifest\.json)$/.test(name)).length, 2);
});

test("copied CI restores the exact Studio revision and mounts the standalone game under games/<id>", () => {
  const workflow = readFileSync(join(gameModuleRoot, ".github", "workflows", "game-verify.yml"), "utf8");
  assert.match(workflow, /STUDIO_REPOSITORY: 'd954mas\/game-67-idle'/);
  assert.match(workflow, /studio_revision=.*revisions\[0\]/);
  assert.match(workflow, /repository: \$\{\{ env\.STUDIO_REPOSITORY \}\}/);
  assert.match(workflow, /path: \$\{\{ steps\.identity\.outputs\.game_path \}\}/);
  assert.match(workflow, /working-directory: \$\{\{ steps\.identity\.outputs\.game_path \}\}[\s\S]*node tools\/game\.mjs verify --target itch/);
});
