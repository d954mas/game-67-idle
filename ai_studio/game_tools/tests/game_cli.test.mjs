import assert from "node:assert/strict";
import { cpSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { execFileSync, spawnSync } from "node:child_process";
import test from "node:test";

import { archiveSymbolMap, buildRemoteEndpoints, cleanBuild, ctestCatalogue, ctestParallelism, doctorGame, executeGameCommand, goldenEnvironment, nativeTestPlan, parseGameArgs, portalCheckPlan, runNativeTests, runTestSuites, selectTests, summarizeTestCapture } from "../game_cli.mjs";
import { findStudioRoot } from "../lib/studio_root.mjs";
import { createRuntimeBuildRecord, runtimeBuildWitness } from "../lib/runtime_build.mjs";

const gameModuleRoot = resolve(fileURLToPath(new URL("../../../templates/template", import.meta.url)));
const studioRoot = findStudioRoot(gameModuleRoot);

/* Read rather than pinned: the fixture asserts that a copied game with a
   matching dependency record packages, not which version the feature is on
   today, and a hardcoded number here fails every time the feature is bumped. */
function platformSdkVersion() {
  return JSON.parse(readFileSync(join(studioRoot, "features", "platform-sdk", "feature.json"), "utf8")).version;
}

function diagnosticsReporter(studio, storageNamespace, runtimeScript = "game.js") {
  const source = readFileSync(join(studio, "features", "platform-sdk", "web", "runtime-diagnostics.js"), "utf8").trimEnd();
  return `${source}\nwindow.__platformDiagnostics = installPlatformDiagnostics(window, { storagePrefix: '${storageNamespace}.crash', runtimeScript: '${runtimeScript}' });`;
}

function diagnosticsBootstrap() {
  return [
    '<script type="module">',
    "function startRuntime() {}",
    "function failLocalBootstrap(error) { window.__platformDiagnostics.report('boot.sdk_failed', error); window.__showRuntimeFatal(); }",
    "import('./platform-sdk.js').then(startRuntime, failLocalBootstrap);",
    "</script>",
  ].join("");
}

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

function uleb(value) {
  const bytes = [];
  do {
    let next = value & 0x7f;
    value >>>= 7;
    if (value) next |= 0x80;
    bytes.push(next);
  } while (value);
  return Buffer.from(bytes);
}

function runtimeBoundWasm(record) {
  const name = Buffer.from("runtime_build", "ascii");
  const marker = Buffer.from(runtimeBuildWitness(record), "ascii");
  const payload = Buffer.concat([uleb(name.length), name, marker]);
  return Buffer.concat([RELEASE_WASM, Buffer.from([0]), uleb(payload.length), payload]);
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
    only: [], tier: "", all: false, updateGoldens: false, verbose: false,
  });
  assert.deepEqual(parseGameArgs(["test", "--only", "test_game_save", "--update-goldens"]).only, ["test_game_save"]);
  assert.equal(parseGameArgs(["test", "--template-proof"]).templateProof, true);
  assert.equal(parseGameArgs(["doctor", "--template-proof"]).templateProof, true);
  assert.equal(parseGameArgs(["test", "--update-goldens"]).updateGoldens, true);
  assert.throws(() => parseGameArgs(["verify", "--update-goldens"]), /only for test/i);
  assert.equal(parseGameArgs(["test", "--tier", "taste"]).tier, "taste");
  assert.throws(() => parseGameArgs(["test", "--tier", "nope"]), /unknown test tier/);
  assert.throws(() => parseGameArgs(["test", "--all", "--tier", "core"]), /cannot be combined/);
  assert.throws(() => parseGameArgs(["test", "--only", "a b; rm -rf /"]), /CTest names/i);
  for (const command of ["doctor", "build", "run", "test", "playable", "package", "verify"]) {
    assert.equal(parseGameArgs([command]).command, command);
  }
  assert.throws(() => parseGameArgs(["unknown"]), /usage:/);
  assert.throws(() => parseGameArgs(["package", "--target", "bad"]), /unknown target/);
  assert.throws(() => parseGameArgs(["portal-check"]), /requires --target/);
  assert.throws(() => parseGameArgs(["portal-check", "--target", "local"]), /portal target/);
  assert.throws(() => parseGameArgs(["doctor", "--no-build"]), /not valid/i);
  assert.throws(() => parseGameArgs(["verify", "--skip-tests"]), /template-proof/i);
});

test("doctor requires the game shim scaffold and exact dependency record", (t) => {
  const gameDir = mkdtempSync(join(tmpdir(), "game-doctor-"));
  t.after(() => rmSync(gameDir, { recursive: true, force: true }));
  assert.throws(() => doctorGame({ gameDir }), /missing/i);

  for (const rel of ["CMakeLists.txt", "tools/game.mjs", "tools/build_web.mjs", "tools/lib/studio_root.mjs", "release/README.md", ".github/workflows/game-verify.yml"]) {
    write(join(gameDir, rel), "fixture\n");
  }
  write(join(gameDir, "game.json"), JSON.stringify({ schema: "ai_studio.game.v1", id: "fixture", title: "Fixture", storageNamespace: "fixture" }));
  write(join(gameDir, "dependencies.json"), JSON.stringify({
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "external/neotolis-engine", version: "11111111", revision: "1".repeat(40), compatibility: "tested" },
    features: [], compatibility: "tested",
  }));
  assert.equal(doctorGame({ gameDir }).gameId, "fixture");

  rmSync(join(gameDir, "game.json"), { force: true });
  rmSync(join(gameDir, "dependencies.json"), { force: true });
  write(join(gameDir, "template.json"), JSON.stringify({ schema: "ai_studio.template.v1", id: "Bad ID", title: "Template", storageNamespace: "template" }));
  write(join(gameDir, "game-dependencies.json"), JSON.stringify({ schema: "ai_studio.game.dependencies.seed.v3" }));
  assert.throws(() => doctorGame({ gameDir, templateProof: true }), /template identity is invalid/i);
});

test("native iteration builds a newly registered test after refreshing CMake", () => {
  const dir = mkdtempSync(join(tmpdir(), "native-catalogue-refresh-"));
  try {
    mkdirSync(join(dir, "build", "native-debug"), { recursive: true });
    writeFileSync(join(dir, "build", "native-debug", "CMakeCache.txt"), "configured");
    writeFileSync(join(dir, "build", "native-debug", "build.ninja"), "ninja graph");
    let refreshed = false;
    const calls = [];
    runNativeTests(dir, { selection: { mode: "tier", tier: "core" } }, {
      run: (command, args) => {
        calls.push([command, ...args]);
        if (command === "cmake" && args[0] === "--build" && args[3] === "build.ninja") refreshed = true;
      },
      catalogue: () => (refreshed
        ? [{ name: "test_new", tier: "core", targets: ["test_new"] }]
        : [{ name: "test_old", tier: "core", targets: ["test_old"] }]),
    });
    const build = calls.find((call) => call[0] === "cmake" && call[1] === "--build" && !call.includes("build.ninja"));
    assert.ok(build.includes("test_new"));
    assert.equal(build.includes("test_old"), false);
    assert.deepEqual(calls[0].slice(0, 5), ["cmake", "--build", join(dir, "build", "native-debug"), "--target", "build.ninja"]);
    assert.equal(calls.some((call) => call[0] === "cmake" && call[1] === "-S"), false);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("native game test plan configures, builds, and runs CTest without a clean rebuild", () => {
  const gameDir = "C:\\repo\\games\\example";
  const windows = nativeTestPlan(gameDir, "win32");
  assert.deepEqual(windows.map((command) => command[0]), ["cmake", "cmake", "ctest"]);
  assert.ok(windows[0].includes("-DCMAKE_BUILD_TYPE=Debug"));
  assert.equal(windows[0].some((arg) => arg.includes("sanitize")), false);
  assert.deepEqual(windows[1].slice(0, 2), ["cmake", "--build"]);
  assert.ok(windows[2].includes("--output-on-failure"));
  assert.ok(windows[2].includes("-j"));
  const configured = nativeTestPlan(gameDir, "win32", true);
  assert.deepEqual(configured.map((command) => command[0]), ["cmake", "ctest"]);
  assert.deepEqual(configured[0].slice(0, 2), ["cmake", "--build"]);
  assert.ok(nativeTestPlan("/repo/games/example", "linux")[0].includes("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined"));
  const focused = nativeTestPlan(gameDir, "win32", true, {
    mode: "only", names: ["test_game_save", "test_game_input"], targets: ["game", "test_game_save", "test_game_input"],
  });
  assert.deepEqual(focused[0], ["cmake", "--build", join(gameDir, "build", "native-debug"),
    "--target", "game", "test_game_save", "test_game_input"]);
  assert.deepEqual(focused[1].slice(-2), ["-R", "^(test_game_save|test_game_input)$"]);
  const tiered = nativeTestPlan(gameDir, "win32", true, { mode: "tier", tier: "taste", targets: ["game", "test_planet_layout"] });
  assert.deepEqual(tiered[0].slice(-3), ["--target", "game", "test_planet_layout"]);
  assert.deepEqual(tiered[1].slice(-2), ["-L", "^taste$"]);
  // Without a tier every test runs, so nothing narrows the build either.
  assert.deepEqual(nativeTestPlan(gameDir, "win32", true, { mode: "all" })[0].slice(-1),
    [join(gameDir, "build", "native-debug")]);
});

test("every executable a test runs from the build tree is a target it needs built", () => {
  const spawn = () => ({ status: 0, stdout: JSON.stringify({ tests: [
    { name: "test_fresh", properties: [{ name: "LABELS", value: ["core"] }] },
    { name: "test_built", command: ["C:/b/tests/test_built.exe"], properties: [{ name: "LABELS", value: ["core"] }] },
    { name: "node_contract", command: ["node", "tests/x.mjs"], properties: [{ name: "LABELS", value: ["core"] }] },
    // Named after what it proves, driving a tool of another name.
    { name: "tool_smoke", command: ["C:\\b\\tools\\stand.exe", "--duration", "300"], properties: [{ name: "LABELS", value: ["core"] }] },
    // A contract handed the binary it checks.
    { name: "tool_contract", command: ["node", "tests/y.mjs", "C:/b/tools/stand.exe"], properties: [{ name: "LABELS", value: ["slow"] }] },
    // A data file read out of the build tree is not a target.
    { name: "pack_contract", command: ["node", "tests/z.mjs", "C:/b/bin/game.ntpack"], properties: [{ name: "LABELS", value: ["core"] }] },
    // Reported through a link, outside the tree asked about.
    { name: "test_linked", command: ["D:/real/b/tests/test_linked.exe"], properties: [{ name: "LABELS", value: ["core"] }] },
  ] }) });
  assert.deepEqual(ctestCatalogue("C:/b", spawn).map((entry) => entry.targets),
    [["test_fresh"], ["test_built"], [], ["stand"], ["stand"], [], ["test_linked"]]);
});

test("a tier selects its tests and only the targets they need", () => {
  const catalogue = [
    { name: "test_logic", tier: "core", targets: ["test_logic"] },
    { name: "test_slow_sim", tier: "slow", targets: ["test_slow_sim"] },
    { name: "layout_contract", tier: "taste", targets: [] },
  ];
  assert.deepEqual(selectTests(catalogue, { mode: "tier", tier: "core" }),
    { names: ["test_logic"], targets: ["game", "test_logic"] });
  assert.deepEqual(selectTests(catalogue, { mode: "tier", tier: "taste" }),
    { names: ["layout_contract"], targets: ["game"] });
  assert.equal(selectTests(catalogue, { mode: "all" }).names.length, 3);
  assert.throws(() => selectTests(catalogue, { mode: "only", names: ["nope"] }), /unknown test name/);
});

test("golden banks are addressed absolutely and recorded only on request", () => {
  const gameDir = "C:\\repo\\games\\example";
  const compare = goldenEnvironment(gameDir, false, { GAME_UPDATE_GOLDENS: "1", PATH: "keep" });
  assert.equal(compare.GAME_GOLDENS_DIR, join(gameDir, "tests", "goldens"));
  assert.equal(compare.GAME_UPDATE_GOLDENS, undefined);
  assert.equal(compare.PATH, "keep");
  assert.equal(goldenEnvironment(gameDir, true, {}).GAME_UPDATE_GOLDENS, "1");
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
    smoke: ({ zipPath, expectedTarget, frameExpectation }) => {
      assert.equal(frameExpectation, "blank");
      calls.push(["smoke", zipPath, expectedTarget]);
    },
  });
  assert.deepEqual(calls, [
    "doctor", "dependencies", "node", "native",
    ["package", false, "local"], ["smoke", "release/template-local.zip", "local"],
  ]);
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
      verifyDependencies: (metadata, opts) => calls.push(opts?.cleanliness === "warn" ? "dependencies:warn" : "dependencies"),
      nodeTest: () => calls.push("node"),
      nativeTest: () => calls.push("native"),
      package: () => { calls.push("package"); return { zipPath: "example.zip" }; },
      smoke: ({ frameExpectation }) => {
        assert.equal(frameExpectation, "rendered");
        calls.push("smoke");
      },
    });
    assert.deepEqual(calls, command === "test"
      ? ["dependencies:warn", "node", "native"]
      : ["dependencies", "node", "native", "package", "smoke"]);
  }
});

test("template test uses reference metadata while retaining both test gates", async () => {
  const calls = [];
  await executeGameCommand({
    command: "test", target: "local", build: true, templateProof: true, skipTests: false, outDir: "",
    only: [], tier: "", all: false, updateGoldens: false,
  }, {
    gameDir: "/repo/templates/template",
    doctor: ({ templateProof }) => { calls.push(templateProof ? "template-doctor" : "game-doctor"); return { gameId: "template" }; },
    loadMetadata: (_gameDir, templateProof) => ({ dependencies: {}, proof: templateProof ? "reference-template" : "game" }),
    verifyDependencies: (_metadata, opts) => calls.push(opts?.cleanliness === "warn" ? "dependencies:warn" : "dependencies"),
    nodeTest: () => calls.push("node"),
    nativeTest: () => calls.push("native"),
  });
  assert.deepEqual(calls, ["template-doctor", "dependencies:warn", "node", "native"]);
});

test("focused native test skips unrelated Node suites", async () => {
  const calls = [];
  await executeGameCommand({
    command: "test", target: "local", build: false, templateProof: true, skipTests: false, outDir: "",
    only: ["test_game_save"], tier: "", all: false, updateGoldens: false,
  }, {
    gameDir: "/repo/templates/template",
    doctor: () => ({ gameId: "template" }),
    loadMetadata: () => ({ dependencies: {}, proof: "reference-template" }),
    verifyDependencies: () => calls.push("dependencies"),
    nodeTest: () => calls.push("node"),
    nativeTest: (_gameDir, options) => calls.push(["native", options.selection]),
  });
  assert.deepEqual(calls, ["dependencies", ["native", { mode: "only", names: ["test_game_save"] }]]);
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

// A game's tools/game.mjs loads the CLI from the Studio it sits in.
function copyGameToolsCore(root) {
  cpSync(join(studioRoot, "ai_studio", "game_tools"), join(root, "ai_studio", "game_tools"), {
    recursive: true, filter: (source) => !/[\\/]tests([\\/]|$)/.test(source.slice(studioRoot.length)),
  });
  cpSync(join(studioRoot, "ai_studio", "core_harness", "tool_lib"), join(root, "ai_studio", "core_harness", "tool_lib"), { recursive: true });
}

test("copied game CLI executes doctor and final package from a real games/private/<id> dependency layout", (t) => {
  const root = mkdtempSync(join(tmpdir(), "copied-game-cli-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const gameDir = join(root, "games", "private", "copied-game");
  cpSync(join(gameModuleRoot, "tools"), join(gameDir, "tools"), { recursive: true });
  cpSync(join(gameModuleRoot, ".github"), join(gameDir, ".github"), { recursive: true });
  // The package audit loads only assets/manifests + its licenses import;
  // copying the whole 6 GB assets tree here is what made this test cost ~34s.
  cpSync(join(studioRoot, "ai_studio", "assets", "manifests"), join(root, "ai_studio", "assets", "manifests"), { recursive: true });
  cpSync(join(studioRoot, "ai_studio", "assets", "licenses"), join(root, "ai_studio", "assets", "licenses"), { recursive: true });
  mkdirSync(join(gameDir, "release"), { recursive: true });
  cpSync(join(gameModuleRoot, "release", "README.md"), join(gameDir, "release", "README.md"));
  cpSync(join(studioRoot, "features", "platform-sdk"), join(root, "features", "platform-sdk"), { recursive: true });
  copyGameToolsCore(root);
  write(join(gameDir, "CMakeLists.txt"), [
    "cmake_minimum_required(VERSION 3.25)",
    "add_subdirectory(\"${STUDIO_ROOT}/features/platform-sdk\" platform-sdk)",
    "",
  ].join("\n"));
  write(join(gameDir, "assets", "release_inputs.json"), JSON.stringify({
    schema: "ai_studio.game_release_assets.v1", inputs: [],
  }));
  write(join(gameDir, "src", "build_packs.c"), "/* copied release fixture has no binary asset inputs */\n");

  const engineRoot = join(root, "external", "neotolis-engine");
  write(join(engineRoot, "engine", "version", "nt_version.h"), "const char *nt_version_engine(void);\n");
  for (const cwd of [engineRoot, root]) {
    git(cwd, ["init", "-q"]);
  }
  git(engineRoot, ["add", "."]);
  git(engineRoot, ["-c", "user.email=tests@example.invalid", "-c", "user.name=Tests", "commit", "-qm", "engine fixture"]);
  const engineRevision = git(engineRoot, ["rev-parse", "HEAD"]);
  git(root, [
    "add",
    "features/platform-sdk",
    "external/neotolis-engine",
    "games/private/copied-game/assets/release_inputs.json",
    "games/private/copied-game/src/build_packs.c",
  ]);
  git(root, ["-c", "user.email=tests@example.invalid", "-c", "user.name=Tests", "commit", "-qm", "dependency fixture"]);
  const revision = git(root, ["rev-parse", "HEAD"]);

  write(join(gameDir, "game.json"), `${JSON.stringify({ schema: "ai_studio.game.v1", id: "copied-game", title: "Copied Game", storageNamespace: "copied-game" }, null, 2)}\n`);
  write(join(gameDir, "dependencies.json"), `${JSON.stringify({
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "external/neotolis-engine", version: engineRevision.slice(0, 8), revision: engineRevision, compatibility: "tested" },
    features: [{ id: "platform-sdk", source: "features/platform-sdk", version: platformSdkVersion(), compatibility: "tested" }],
    compatibility: "copied layout fixture",
  }, null, 2)}\n`);
  const runtimeBuild = createRuntimeBuildRecord({
    gameDir,
    studioRoot: root,
    dependencies: JSON.parse(readFileSync(join(gameDir, "dependencies.json"), "utf8")),
    compileProfile: { target: "itch", adapter: "mock", preset: "wasm-release", debugUi: false, devapi: false, analytics: false, eventsLogMirror: false },
  });
  const artifact = join(gameDir, "build", "wasm-release-itch", "bin");
  write(join(artifact, "index.html"), `<!doctype html><script>window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true, runtimeBuildFingerprint: '${runtimeBuild.fingerprint}' });</script><script id="platform-diagnostics">${diagnosticsReporter(root, "copied-game")}</script>${diagnosticsBootstrap()}<script src="game.js"></script>\n`);
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

test("copied CI restores the Studio layout and mounts the standalone game under games/<id>", () => {
  const workflow = readFileSync(join(gameModuleRoot, ".github", "workflows", "game-verify.yml"), "utf8");
  assert.match(workflow, /STUDIO_REPOSITORY: 'd954mas\/game-67-idle'/);
  // Schema v3 pins features by declared version, so the checkout takes the
  // Studio default branch: no ref line, and no revision derived from features.
  assert.doesNotMatch(workflow, /studio_revision/);
  assert.doesNotMatch(workflow, /ref: \$\{\{ steps\.identity/);
  assert.match(workflow, /d\.schema!=='ai_studio\.game\.dependencies\.v3'/);
  assert.match(workflow, /repository: \$\{\{ env\.STUDIO_REPOSITORY \}\}/);
  assert.match(workflow, /path: \$\{\{ steps\.identity\.outputs\.game_path \}\}/);
  assert.match(workflow, /working-directory: \$\{\{ steps\.identity\.outputs\.game_path \}\}[\s\S]*node tools\/game\.mjs verify --target itch/);
});

test("portal-check asks the artifact, the store draft and the SDK proof that exists", () => {
  const gameDir = "/repo/games/example";
  const studio = "/repo";
  // Every portal has an artifact contract; only some have a store spec, and
  // only Yandex ships a dev server a game can be driven inside.
  const present = new Set([
    "/repo/ai_studio/store_kit/yandex_spec.json",
    "/repo/features/platform-sdk/scripts/yandex_sdk_probe.mjs",
    "/repo/games/example/release/store/yandex",
    "/repo/games/example/build/wasm-release-yandex/bin",
  ]);
  const exists = (path) => present.has(path.replaceAll("\\", "/"));

  const yandex = portalCheckPlan(studio, gameDir, "yandex", { exists });
  assert.deepEqual(yandex.map((step) => step.id), ["artifact", "store", "sdk"]);
  assert.equal(yandex.every((step) => !step.skip), true);

  const itch = portalCheckPlan(studio, gameDir, "itch", { exists });
  assert.deepEqual(itch.map((step) => step.id), ["artifact"]);

  // A declared spec with no folder behind it is a failing row, never a silent
  // pass: an unwritten store draft is exactly what this command is for.
  const missingDraft = portalCheckPlan(studio, gameDir, "yandex", {
    exists: (path) => exists(path) && !path.replaceAll("\\", "/").endsWith("/release/store/yandex"),
  });
  assert.equal(missingDraft.find((step) => step.id === "store").skip.includes("no store draft"), true);
});

test("a green test run prints one summary line and keeps the output in a log", async (t) => {
  const gameDir = mkdtempSync(join(tmpdir(), "game-quiet-"));
  t.after(() => rmSync(gameDir, { recursive: true, force: true }));
  const nodeTest = (_dir, capture) => capture.steps.push({ label: "node", command: "node --test", output: "ok 1\n# pass 6\n# fail 0\n" });
  const nativeTest = (_dir, options) => options.capture.steps.push({ label: "native", command: "ctest", output: "noise\n100% tests passed, 0 tests failed out of 125\n" });
  const summary = await runTestSuites(gameDir, { nodeTest, nativeTest, selection: { mode: "tier", tier: "core" } });
  assert.match(summary, /^node 6 passed, native 125 passed in [\d.]+s; log: /);
  assert.match(readFileSync(join(gameDir, "build", "logs", "game-test.log"), "utf8"), /\$ ctest\nnoise/);
  assert.equal(await runTestSuites(gameDir, { nodeTest: () => {}, nativeTest: () => {}, selection: {}, verbose: true }), "");
});

test("a red test run shows the failing step's tail and where the full log is", async (t) => {
  const gameDir = mkdtempSync(join(tmpdir(), "game-quiet-fail-"));
  t.after(() => rmSync(gameDir, { recursive: true, force: true }));
  const nativeTest = () => {
    const error = new Error("native game tests exited 8");
    error.output = `${"earlier line\n".repeat(200)}The following tests FAILED:\n  12 - test_save (Failed)\n`;
    throw error;
  };
  await assert.rejects(
    runTestSuites(gameDir, { nodeTest: () => {}, nativeTest, selection: {} }),
    (error) => /exited 8\n[\s\S]*test_save \(Failed\)\nfull log: /.test(error.message) && error.message.split("\n").length < 90,
  );
  assert.deepEqual(summarizeTestCapture({ steps: [{ output: "ℹ pass 3\n" }, { output: "ℹ pass 2\n90% tests passed, 1 tests failed out of 10\n" }] }), { nodePassed: 5, nativePassed: 9 });
});

test("clean drops build variants and keeps the native debug tree unless --all", (t) => {
  const gameDir = mkdtempSync(join(tmpdir(), "game-clean-"));
  t.after(() => rmSync(gameDir, { recursive: true, force: true }));
  for (const rel of ["native-debug/bin/game.exe", "wasm-release-poki/bin/game.wasm", "wasm-debug/bin/game.wasm", "logs/game-test.log"]) {
    write(join(gameDir, "build", rel), "x".repeat(10));
  }
  const removed = cleanBuild(gameDir);
  assert.deepEqual(removed.map((row) => row.name).sort(), ["logs", "wasm-debug", "wasm-release-poki"]);
  assert.equal(removed.every((row) => row.bytes === 10), true);
  assert.deepEqual(readdirSync(join(gameDir, "build")), ["native-debug"]);
  assert.deepEqual(cleanBuild(gameDir, { all: true }).map((row) => row.name), ["native-debug"]);
  assert.deepEqual(cleanBuild(join(gameDir, "missing")), []);

  assert.equal(parseGameArgs(["clean", "--all"]).all, true);
  assert.throws(() => parseGameArgs(["build", "--all"]), /--all is valid only for test and clean/);
});

test("a game without CI passes doctor only when it says so", (t) => {
  const gameDir = mkdtempSync(join(tmpdir(), "game-doctor-"));
  t.after(() => rmSync(gameDir, { recursive: true, force: true }));
  for (const rel of ["CMakeLists.txt", "tools/game.mjs", "tools/build_web.mjs", "tools/lib/studio_root.mjs", "release/README.md"]) {
    write(join(gameDir, rel), "fixture\n");
  }
  write(join(gameDir, "game.json"), JSON.stringify({ schema: "ai_studio.game.v1", id: "fixture", title: "Fixture", storageNamespace: "fixture" }));
  write(join(gameDir, "dependencies.json"), JSON.stringify({
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "external/neotolis-engine", version: "11111111", revision: "1".repeat(40), compatibility: "tested" },
    features: [], compatibility: "tested",
  }));
  assert.throws(() => doctorGame({ gameDir }), /game-verify\.yml/);
  assert.equal(doctorGame({ gameDir, ciWorkflow: false }).gameId, "fixture");
});

test("verify admits only the endpoints the build recorded beside the target build", async () => {
  const reads = [];
  const read = (path) => { reads.push(path.replaceAll("\\", "/")); return '{ "allowedRemoteUrls": ["https://stats.example.com"] }'; };
  assert.deepEqual(buildRemoteEndpoints("/repo/games/example", "itch", { read, exists: () => true }), ["https://stats.example.com"]);
  assert.match(reads[0], /\/build\/wasm-release-itch\/remote_endpoints\.json$/);
  assert.deepEqual(buildRemoteEndpoints("/g", "itch", { exists: () => false }), []);
  assert.throws(() => buildRemoteEndpoints("/g", "itch", { exists: () => true, read: () => "{" }), /reconfigure the build/);
  assert.throws(() => buildRemoteEndpoints("/g", "itch", { exists: () => true, read: () => '{ "allowedRemoteUrls": [1] }' }), /malformed/);

  let seen = null;
  await executeGameCommand({ command: "verify", target: "itch", build: false, templateProof: false, skipTests: true, outDir: "" }, {
    gameDir: "/repo/games/example",
    doctor: () => ({ gameId: "example" }),
    loadMetadata: () => ({ dependencies: {}, proof: "game" }),
    verifyDependencies: () => {},
    package: () => ({ zipPath: "example.zip" }),
    remoteEndpoints: (gameDir, target) => [`https://${target}.example.com`],
    smoke: ({ allowedRemoteUrls }) => { seen = allowedRemoteUrls; },
  });
  assert.deepEqual(seen, ["https://itch.example.com"]);
});

test("a built package keeps the link's symbol map beside its ZIP", () => {
  const copies = [];
  const target = archiveSymbolMap("/g/build/wasm-release-itch/bin", "/g/release/artifacts/g-itch-abc.zip", {
    exists: () => true, copy: (from, to) => copies.push([from.replaceAll("\\", "/"), to]),
  });
  assert.equal(target, "/g/release/artifacts/g-itch-abc.symbols");
  assert.deepEqual(copies, [["/g/build/wasm-release-itch/game.js.symbols", "/g/release/artifacts/g-itch-abc.symbols"]]);
  assert.equal(archiveSymbolMap("/g/build/x/bin", "/g/a.zip", { exists: () => false, copy: () => assert.fail("no map to copy") }), "");
});

test("the CLI builds and proves against the one Studio it runs from", (t) => {
  const root = mkdtempSync(join(tmpdir(), "studio-root-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "external", "neotolis-engine"), { recursive: true });
  const detached = join(root, "a", "b", "c", "game");
  assert.equal(findStudioRoot(detached, { env: { GAME_REPO_ROOT: root }, ownRoot: root }), root);
  assert.equal(findStudioRoot(detached, { env: { GAME_REPO_ROOT: studioRoot } }), resolve(studioRoot));
  // Another Studio, named or found, would split the compiled bytes from their proof.
  assert.throws(() => findStudioRoot(detached, { env: { GAME_REPO_ROOT: root } }), /not the Studio this game CLI runs from/);
  assert.throws(() => findStudioRoot(join(root, "games", "x"), { env: {} }), /not the Studio this game CLI runs from/);
  assert.throws(() => findStudioRoot(detached, { env: { GAME_REPO_ROOT: join(root, "missing") } }), /GAME_REPO_ROOT.*neotolis-engine/);
  assert.throws(() => findStudioRoot(detached, { env: {} }), /cannot find Studio root/);
  assert.throws(() => findStudioRoot(detached, { env: { STUDIO_ROOT: root } }), /cannot find Studio root/, "STUDIO_ROOT is not an alias");
});

test("a game adds its own test tiers without widening the shared ones", () => {
  assert.equal(parseGameArgs(["test", "--tier", "net"], { tiers: ["net"] }).tier, "net");
  assert.throws(() => parseGameArgs(["test", "--tier", "net"]), /unknown test tier/);
});

test("CTest parallelism follows CTEST_PARALLEL_LEVEL when it is set", () => {
  assert.equal(ctestParallelism({ CTEST_PARALLEL_LEVEL: "3" }), 3);
  assert.ok(ctestParallelism({}) >= 1);
  assert.throws(() => ctestParallelism({ CTEST_PARALLEL_LEVEL: "-1" }), /non-negative/);
});

test("a game that settles its own native tree is not configured a second time", () => {
  const calls = [];
  runNativeTests("/repo/games/example", { selection: { mode: "tier", tier: "core" } }, {
    settle: (runStep) => runStep("cmake", ["-C", "seed.cmake", "-S", "/repo/games/example"], "native configure"),
    run: (command, args, cwd, label) => calls.push([label, command, args[0]]),
    catalogue: () => [{ name: "unit", tier: "core", targets: ["unit"] }],
  });
  assert.deepEqual(calls.map((call) => call[0]), ["native configure", "native game tests", "native game tests"]);
  assert.deepEqual(calls.slice(1).map((call) => call[1]), ["cmake", "ctest"]);
  assert.equal(calls[1][2], "--build");
});

test("a game with its own web layout names its artifact, validator and portal proof", async () => {
  let validated = null;
  await executeGameCommand({ command: "playable", target: "itch", build: false, templateProof: false, skipTests: false, outDir: "" }, {
    gameDir: gameModuleRoot,
    doctor: () => ({ gameId: "example" }),
    loadMetadata: () => ({ dependencies: {}, proof: "game" }),
    verifyDependencies: () => {},
    artifactDir: (gameDir, target) => `${gameDir}/build/${target}-site`,
    validateArtifact: (options) => { validated = options; },
  });
  assert.equal(validated.artifactDir, `${resolve(gameModuleRoot)}/build/itch-site`);
  const steps = portalCheckPlan("/studio", "/repo/games/example", "itch", {
    exists: () => true,
    artifactDir: (gameDir, target) => `${gameDir}/build/${target}-site`,
    gameArtifact: { args: (gameDir, target) => ["game.mjs", "playable", "--target", target, "--no-build"], replacesFeatureInspect: "the site layout has no assets/game.ntpack" },
  });
  assert.deepEqual(steps[0].args, ["game.mjs", "playable", "--target", "itch", "--no-build"]);
  assert.match(steps[0].note, /replaces features\/platform-sdk artifact_tools inspect: the site layout/);
  // Without a stated reason the feature inspect always runs, and the game's check runs after it.
  const both = portalCheckPlan("/studio", "/repo/games/example", "itch", {
    exists: () => true, gameArtifact: { args: () => ["game.mjs", "playable"] },
  });
  assert.deepEqual(both.slice(0, 2).map((step) => step.id), ["artifact", "game-artifact"]);
  assert.match(both[0].args[0].replaceAll("\\", "/"), /artifact_tools\.mjs$/);
  assert.throws(() => portalCheckPlan("/studio", "/g", "itch", { exists: () => true, gameArtifact: { args: () => [], replacesFeatureInspect: " " } }), /must say why/);
});

test("focused and non-core CTest runs do not start the unrelated Node suite", async () => {
  for (const [options, node] of [[{ only: ["sim_movement"] }, false], [{ tier: "slow" }, false], [{ tier: "core" }, true], [{ all: true }, true]]) {
    const calls = [];
    await executeGameCommand({ command: "test", only: [], tier: "", all: false, verbose: true, ...options }, {
      gameDir: "/repo/games/example",
      doctor: () => ({ gameId: "example" }),
      loadMetadata: () => ({ dependencies: {}, proof: "game" }),
      verifyDependencies: () => {},
      nodeTest: () => calls.push("node"),
      nativeTest: () => calls.push("native"),
    });
    assert.deepEqual(calls, node ? ["node", "native"] : ["native"]);
  }
});

test("a game whose playable target has another name builds that one", () => {
  const catalogue = [{ name: "unit", tier: "core", targets: ["unit"] }];
  assert.deepEqual(selectTests(catalogue, { mode: "tier", tier: "core" }).targets, ["game", "unit"]);
  assert.deepEqual(selectTests(catalogue, { mode: "tier", tier: "core" }, { gameTarget: "plw_client" }).targets, ["plw_client", "unit"]);
});

test("the Node suite runs beside the native checks, and a native failure is reported first", async (t) => {
  const gameDir = mkdtempSync(join(tmpdir(), "game-beside-"));
  t.after(() => rmSync(gameDir, { recursive: true, force: true }));
  const order = [];
  let finishNode;
  const nodeTest = () => { order.push("node started"); return new Promise((resolveNode) => { finishNode = resolveNode; }); };
  const nativeTest = () => { order.push("native ran"); finishNode(); };
  await runTestSuites(gameDir, { nodeTest, nativeTest, selection: {}, verbose: true });
  assert.deepEqual(order, ["node started", "native ran"]);

  const nativeFails = () => { throw new Error("native game tests exited 1"); };
  await assert.rejects(runTestSuites(gameDir, { nodeTest: () => Promise.reject(new Error("game tests exited 1")), nativeTest: nativeFails, selection: {}, verbose: true }), /^Error: native game tests exited 1; also game tests exited 1$/m);
  await assert.rejects(runTestSuites(gameDir, { nodeTest: () => Promise.reject(new Error("game tests exited 1")), nativeTest: () => {}, selection: {}, verbose: true }), /^Error: game tests exited 1$/m);
});
