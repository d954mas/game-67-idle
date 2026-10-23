import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import { createBuildPlan, executeBuildPlan, optionalPackPaths, parseBuildArgs, resolveEmcmakePath } from "../build_web.mjs";

const slash = (value) => value.replaceAll("\\", "/").replace(/^[A-Za-z]:/, "");

test("game-declared packs stage through the build plan and disappear after declaration removal", (t) => {
  const gameDir = mkdtempSync(join(tmpdir(), "build-pack-plan-"));
  t.after(() => rmSync(gameDir, { recursive: true, force: true }));
  const write = (path, value) => { mkdirSync(dirname(path), { recursive: true }); writeFileSync(path, value); };
  const options = {
    gameDir, args: { preset: "wasm-debug", target: "local", debugUi: "on", preview: true },
    env: { EMSDK: "/opt/emsdk" }, platform: "linux", nativeConfigured: true, toolchainExists: true,
  };
  write(join(gameDir, "assets", "runtime-packs.json"), JSON.stringify({ schema: "ai_studio.game.asset_packs.v1", packs: ["assets/forest.ntpack"] }));
  let plan = createBuildPlan(options);
  write(join(plan.nativeDir, "bin", "assets", "game.ntpack"), "core");
  write(join(plan.nativeDir, "bin", "assets", "forest.ntpack"), "forest");
  executeBuildPlan(plan, { run: () => ({ status: 0 }) });
  assert.ok(existsSync(join(plan.webDir, "bin", "assets", "forest.ntpack")));
  assert.ok(existsSync(join(plan.webDir, "game-asset-packs.json")));
  assert.ok(!existsSync(join(plan.webDir, "bin", "game-asset-packs.json")));
  rmSync(join(gameDir, "assets", "runtime-packs.json"));
  plan = createBuildPlan(options);
  executeBuildPlan(plan, { run: () => ({ status: 0 }) });
  assert.ok(!existsSync(join(plan.webDir, "bin", "assets", "forest.ntpack")));
  assert.ok(existsSync(join(plan.webDir, "bin", "assets", "game.ntpack")));
});

test("build web arguments preserve preset target and release debug validation", () => {
  assert.deepEqual(parseBuildArgs([]), { preset: "wasm-release", target: "local", debugUi: "default", preview: false });
  assert.deepEqual(parseBuildArgs(["--preset", "wasm-devapi-debug", "--target", "poki", "--no-debug-ui"]), {
    preset: "wasm-devapi-debug", target: "poki", debugUi: "off", preview: false,
  });
  assert.deepEqual(parseBuildArgs(["--preset", "wasm-debug", "--preview"]), {
    preset: "wasm-debug", target: "local", debugUi: "default", preview: true,
  });
  assert.throws(() => parseBuildArgs(["--preset", "bad"]), /unknown preset/);
  assert.throws(() => parseBuildArgs(["--target", "bad"]), /unknown target/);
  assert.throws(() => parseBuildArgs(["--debug-ui"]), /not allowed in wasm-release/);
  assert.throws(() => parseBuildArgs(["--preview"]), /preview requires wasm-debug or wasm-devapi-debug/);
});

test("preview plan writes an explicit non-proof record and clears the compiled fingerprint", () => {
  const plan = createBuildPlan({
    gameDir: "/repo/templates/template",
    args: { preset: "wasm-debug", target: "local", debugUi: "on", preview: true },
    env: { EMSDK: "/opt/emsdk" }, platform: "linux", nativeConfigured: true, toolchainExists: true,
  });
  const configure = plan.steps.find((step) => step.kind === "run" && step.args.some((arg) => arg.startsWith("-DGAME_PUBLISH_TARGET=")));
  assert.ok(configure.args.includes("-DGAME_RUNTIME_BUILD_FINGERPRINT="));
  assert.ok(configure.args.includes("-DGAME_RUNTIME_BUILD_PREVIEW=ON"));
  assert.equal(plan.steps[1].kind, "remove");
  assert.match(slash(plan.steps[1].path), /wasm-debug\/bin\/runtime-build\.json$/);
  const record = plan.steps.at(-1).value;
  assert.deepEqual(record, {
    schema: "ai_studio.runtime_build.preview.v1", proof: "none", preset: "wasm-debug", target: "local",
  });
  assert.match(plan.message, /NON-PROOF/i);
});

test("preview refuses a runtime proof that could masquerade as current output", () => {
  const inputs = [{ id: "game", source: ".", files: 1, sha256: "1".repeat(64) }];
  const runtimeBuild = {
    schema: "ai_studio.runtime_build.v1",
    fingerprint: createHash("sha256").update(JSON.stringify(inputs)).digest("hex"),
    inputs,
  };
  assert.throws(() => createBuildPlan({
    gameDir: "/repo/templates/template",
    args: { preset: "wasm-debug", target: "local", debugUi: "on", preview: true },
    env: { EMSDK: "/opt/emsdk" }, platform: "linux", nativeConfigured: true, toolchainExists: true, runtimeBuild,
  }), /preview must not receive a runtime build proof/i);
});

test("plan preserves build dirs local cache native pack web targets and final message", () => {
  const inputs = [
    { id: "game", source: ".", files: 1, sha256: "1".repeat(64) },
    { id: "engine", source: "external/neotolis-engine", files: 1, sha256: "2".repeat(64) },
  ];
  const runtimeBuild = {
    schema: "ai_studio.runtime_build.v1",
    fingerprint: createHash("sha256").update(JSON.stringify(inputs)).digest("hex"),
    inputs,
  };
  const plan = createBuildPlan({
    gameDir: "/repo/templates/template",
    args: { preset: "wasm-devapi-debug", target: "yandex", debugUi: "on" },
    env: { EMSDK: "/opt/emsdk" },
    platform: "win32",
    nativeConfigured: false,
    toolchainExists: true,
    runtimeBuild,
    optionalPacks: [],
  });
  assert.equal(slash(plan.env.EM_CACHE), "/repo/templates/template/build/emscripten-cache");
  assert.equal(slash(plan.nativeDir), "/repo/templates/template/build/wasm-devapi-debug-yandex-host");
  assert.equal(slash(plan.webDir), "/repo/templates/template/build/wasm-devapi-debug-yandex");
  assert.deepEqual(plan.steps.map((step) => step.kind), ["mkdir", "run", "stamp", "run", "run", "run", "move", "run", "copy", "run", "write"]);
  const runSteps = plan.steps.filter((step) => step.kind === "run");
  assert.equal(runSteps[0].args.slice(-4, -1).join(" "), "-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug");
  assert.match(runSteps[0].args.at(-1), /^-DGAME_REPO_ROOT=/);
  assert.deepEqual(runSteps.slice(1, -1).map((step) => step.args.slice(-2).join(" ")), [
    "--target game_asset_packs",
    "-DGAME_PLATFORM_SDK_DEBUG_UI=ON -DGAME_DEVAPI_ENABLED=ON",
    "--target game",
    "--target platform_sdk_web_assets",
  ]);
  assert.equal(plan.steps.at(-1).kind, "write");
  const packCopy = plan.steps.find((step) => step.kind === "copy" && String(step.to).endsWith("game.ntpack"));
  assert.equal(slash(packCopy.from), "/repo/templates/template/build/wasm-devapi-debug-yandex-host/bin/assets/game.ntpack");
  assert.equal(slash(packCopy.to), "/repo/templates/template/build/wasm-devapi-debug-yandex/bin/assets/game.ntpack");
  assert.equal(slash(plan.steps.at(-1).path), "/repo/templates/template/build/wasm-devapi-debug-yandex/bin/runtime-build.json");
  assert.deepEqual(plan.steps.at(-1).value, runtimeBuild);
  const configure = plan.steps.find((step) => step.kind === "run" && step.args.includes("-DGAME_PUBLISH_TARGET=yandex"));
  assert.ok(configure.args.includes(`-DGAME_RUNTIME_BUILD_FINGERPRINT=${runtimeBuild.fingerprint}`));
  assert.match(plan.message, /^built wasm-devapi-debug-yandex \(yandex -> platform-sdk\); serve with: node \S*ai_studio\/game_tools\/serve_web\.mjs --preset wasm-devapi-debug --target yandex$/);
});

test("executor uses injected filesystem and runner seams", () => {
  const events = [];
  const plan = {
    env: { EM_CACHE: "cache" },
    steps: [
      { kind: "mkdir", path: "a" },
      { kind: "run", command: "cmake", args: ["--build", "a"] },
      { kind: "copy", from: "a/pack", to: "b/pack" },
      { kind: "write", path: "b/runtime-build.json", value: { fingerprint: "a" } },
    ],
    message: "done",
  };
  const result = executeBuildPlan(plan, {
    mkdir: (path) => events.push(["mkdir", path]),
    copy: (from, to) => events.push(["copy", from, to]),
    write: (path, value) => events.push(["write", path, value.fingerprint]),
    run: (command, args, options) => { events.push(["run", command, args, options.env.EM_CACHE]); return { status: 0 }; },
  });
  assert.equal(result, "done");
  assert.deepEqual(events, [
    ["mkdir", "a"],
    ["run", "cmake", ["--build", "a"], "cache"],
    ["copy", "a/pack", "b/pack"],
    ["write", "b/runtime-build.json", "a"],
  ]);
});

test("executor rechecks runtime inputs immediately before publishing the build record", () => {
  const events = [];
  assert.throws(() => executeBuildPlan({
    env: {},
    steps: [{ kind: "write", path: "bin/runtime-build.json", value: { fingerprint: "a" } }],
    verifyRuntimeBuild() { events.push("verify"); throw new Error("inputs changed"); },
  }, {
    write: () => events.push("write"),
  }), /inputs changed/i);
  assert.deepEqual(events, ["verify"]);
});

test("plan covers every preset across every publish target with exact debug flags", () => {
  const presets = ["wasm-release", "wasm-debug", "wasm-devapi-debug", "wasm-devapi-perf"];
  const buildTypes = { "wasm-release": "Release", "wasm-devapi-perf": "RelWithDebInfo" };
  const targets = ["local", "itch", "poki", "yandex", "playgama", "crazygames", "wavedash"];
  for (const preset of presets) {
    for (const target of targets) {
      const plan = createBuildPlan({
        gameDir: "/repo/templates/template",
        args: { preset, target, debugUi: preset === "wasm-release" ? "off" : "on" },
        env: { EMSDK: "/opt/emsdk" },
        platform: "linux",
        nativeConfigured: true,
        toolchainExists: true,
      });
      const expectedName = target === "local" ? preset : `${preset}-${target}`;
      assert.ok(slash(plan.webDir).endsWith(`/build/${expectedName}`));
      const configure = plan.steps.find((step) => step.kind === "run" && step.args.some((arg) => arg.startsWith("-DGAME_PUBLISH_TARGET=")));
      assert.ok(configure.args.includes(`-DCMAKE_BUILD_TYPE=${buildTypes[preset] || "Debug"}`));
      assert.ok(configure.args.includes(`-DGAME_PUBLISH_TARGET=${target}`));
      assert.ok(configure.args.includes("-DGAME_AUDIO_BROWSER_SMOKE=OFF"));
      assert.ok(configure.args.includes(`-DGAME_ANALYTICS_ENABLED=${preset === "wasm-devapi-debug" ? "ON" : "OFF"}`));
      assert.ok(configure.args.includes(`-DGAME_EVENTS_LOG_MIRROR=${["wasm-release", "wasm-devapi-perf"].includes(preset) ? "OFF" : "ON"}`));
      assert.ok(configure.args.includes(`-DGAME_DEVAPI_ENABLED=${preset.startsWith("wasm-devapi") ? "ON" : "OFF"}`));
      assert.ok(configure.args.includes(`-DGAME_PLATFORM_SDK_DEBUG_UI=${preset === "wasm-release" ? "OFF" : "ON"}`));
    }
  }
});

test("EMSDK and platform fallback rules fail loudly and remain injectable", () => {
  const base = {
    gameDir: "/repo/templates/template",
    args: { preset: "wasm-release", target: "local", debugUi: "off" },
    nativeConfigured: true,
  };
  assert.throws(() => createBuildPlan({ ...base, env: { EMSDK: "/broken/emsdk" }, platform: "linux", toolchainExists: false }), /toolchain is missing/);
  assert.throws(() => createBuildPlan({ ...base, env: {}, platform: "win32", toolchainExists: false }), /EMSDK is required on Windows/);

  const linux = createBuildPlan({ ...base, env: {}, platform: "linux", toolchainExists: false });
  assert.equal(linux.steps.find((step) => step.kind === "run" && step.args[0] === "cmake").command, "emcmake");

  const windows = createBuildPlan({ ...base, env: {}, platform: "win32", emcmakePath: "C:\\tools\\emcmake.exe", toolchainExists: false });
  assert.equal(windows.steps.find((step) => step.kind === "run" && step.args[0] === "cmake").command, "C:\\tools\\emcmake.exe");
});

test("Linux resolves absolute emcmake from EMSDK while EMCMAKE remains injectable", () => {
  assert.equal(resolveEmcmakePath({ EMSDK: "/opt/emsdk" }, "linux"), "/opt/emsdk/upstream/emscripten/emcmake");
  assert.equal(resolveEmcmakePath({ EMSDK: "/opt/emsdk", EMCMAKE: "/custom/emcmake" }, "linux"), "/custom/emcmake");
  assert.equal(resolveEmcmakePath({}, "linux"), "emcmake");
  assert.equal(resolveEmcmakePath({ EMSDK: "C:\\emsdk" }, "win32"), "");
  const plan = createBuildPlan({
    gameDir: "/repo/templates/template",
    args: { preset: "wasm-release", target: "local", debugUi: "off" },
    env: { EMSDK: "/opt/emsdk" },
    platform: "linux",
    emcmakePath: resolveEmcmakePath({ EMSDK: "/opt/emsdk" }, "linux"),
    nativeConfigured: false,
    toolchainExists: true,
  });
  const configure = plan.steps.find((step) => step.kind === "run" && step.args[0] === "cmake");
  const nativeConfigure = plan.steps.find((step) => step.kind === "run" && step.args.includes("-DCMAKE_C_COMPILER=clang"));
  assert.ok(nativeConfigure.args.includes("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined"));
  assert.equal(configure.command, "/opt/emsdk/upstream/emscripten/emcmake");
  assert.equal(configure.args.some((arg) => arg.startsWith("-DCMAKE_TOOLCHAIN_FILE=")), false);
});

test("release build minifies the staged web shell but debug builds do not", () => {
  const base = {
    gameDir: "/repo/templates/template",
    env: { EMSDK: "/opt/emsdk" },
    platform: "linux",
    toolchainExists: true,
    nativeConfigured: true,
  };
  const release = createBuildPlan({
    ...base,
    args: { preset: "wasm-release", target: "poki", debugUi: "off" },
  });
  const minify = release.steps.find((step) => step.kind === "run"
    && step.args && step.args.some((arg) => String(arg).endsWith("minify_web_release.mjs")));
  assert.ok(minify);
  assert.equal(minify.command, process.execPath);
  assert.ok(minify.args.some((arg) => slash(String(arg)) === "/repo/templates/template/build/wasm-release-poki/bin"));

  const debug = createBuildPlan({
    ...base,
    args: { preset: "wasm-debug", target: "poki", debugUi: "off" },
  });
  assert.equal(debug.steps.some((step) => step.kind === "run"
    && step.args && step.args.some((arg) => String(arg).endsWith("minify_web_release.mjs"))), false);
});


test("plan rejects a signed profile that differs from compiler flags", () => {
  const inputs = [
    { id: "game", source: ".", files: 1, sha256: "1".repeat(64) },
    { id: "engine", source: "external/neotolis-engine", files: 1, sha256: "2".repeat(64) },
  ];
  const profile = { target: "poki", adapter: "poki", preset: "wasm-debug", debugUi: false, devapi: false, analytics: false, eventsLogMirror: true };
  const runtimeBuild = {
    schema: "ai_studio.runtime_build.v2",
    fingerprint: createHash("sha256").update(JSON.stringify({ inputs, profile })).digest("hex"),
    inputs,
    profile,
  };
  assert.throws(() => createBuildPlan({
    gameDir: "/repo/templates/template",
    args: { preset: "wasm-debug", target: "poki", debugUi: "on" },
    env: { EMSDK: "/opt/emsdk" }, platform: "linux", nativeConfigured: true,
    toolchainExists: true, runtimeBuild,
  }), /profile does not match the configured compiler flags/);
});

test("plan refuses debug UI in a release even when called without CLI parsing", () => {
  assert.throws(() => createBuildPlan({
    gameDir: "/repo/templates/template",
    args: { preset: "wasm-release", target: "poki", debugUi: "on" },
    env: { EMSDK: "/opt/emsdk" }, platform: "linux", nativeConfigured: true, toolchainExists: true,
  }), /debug UI is not allowed/i);
});

test("a game adds its own CMake options to the host and web configure", () => {
  const plan = createBuildPlan({
    gameDir: "/repo/games/example",
    args: { preset: "wasm-devapi-perf", target: "itch", debugUi: "off" },
    env: { EMSDK: "/opt/emsdk" },
    platform: "linux",
    nativeConfigured: false,
    toolchainExists: true,
    optionalPacks: [],
    configureArgs: (args) => (args.preset === "wasm-devapi-perf" ? { host: ["-DGAME_TESTBED=OFF"], web: ["-DNT_ASSERT_MODE=2"] } : {}),
  });
  const runs = plan.steps.filter((step) => step.kind === "run");
  assert.equal(runs[0].args.at(-1), "-DGAME_TESTBED=OFF");
  const web = runs.find((step) => step.args.includes("-DGAME_PUBLISH_TARGET=itch"));
  assert.equal(web.args.at(-1), "-DNT_ASSERT_MODE=2");
  assert.ok(web.args.includes("-DGAME_ANALYTICS_ENABLED=OFF"));
  assert.throws(() => createBuildPlan({
    gameDir: "/g", args: { preset: "wasm-release", target: "itch", debugUi: "off" }, env: { EMSDK: "/e" }, platform: "linux",
    toolchainExists: true, optionalPacks: [], configureArgs: () => ({ web: ["--fresh"] }),
  }), /-D options/);
});

test("the target's optional packs are staged only when the host built them", () => {
  assert.ok(optionalPackPaths("itch").includes("assets/music.ntpack"));
  assert.ok(optionalPackPaths("local").every((path) => /^assets\/[^/]+\.ntpack$/.test(path)));
  const plan = createBuildPlan({
    gameDir: "/repo/games/example",
    args: { preset: "wasm-release", target: "itch", debugUi: "off" },
    env: { EMSDK: "/opt/emsdk" },
    platform: "linux",
    nativeConfigured: true,
    toolchainExists: true,
    optionalPacks: ["assets/music.ntpack"],
  });
  const optional = plan.steps.filter((step) => step.kind === "copy_optional");
  assert.deepEqual(optional.map((step) => slash(step.to)), ["/repo/games/example/build/wasm-release-itch/bin/assets/music.ntpack"]);
  const events = [];
  executeBuildPlan({
    env: {},
    steps: [
      { kind: "copy_optional", from: "host/music.ntpack", to: "bin/music.ntpack" },
      { kind: "copy_optional", from: "host/absent.ntpack", to: "bin/absent.ntpack" },
      { kind: "move", from: "bin/game.js.symbols", to: "game.js.symbols" },
      { kind: "move", from: "bin/absent.symbols", to: "absent.symbols" },
    ],
    message: "done",
  }, {
    mkdir: () => {},
    exists: (path) => !path.includes("absent"),
    copy: (from, to) => events.push(["copy", from, to]),
    move: (from, to) => events.push(["move", from, to]),
  });
  assert.deepEqual(events, [["copy", "host/music.ntpack", "bin/music.ntpack"], ["move", "bin/game.js.symbols", "game.js.symbols"]]);
});

test("both trees build from the Studio that proves them", () => {
  const plan = createBuildPlan({
    gameDir: "/repo/games/example", studioRoot: "/repo",
    args: { preset: "wasm-release", target: "itch", debugUi: "off" },
    env: { EMSDK: "/opt/emsdk" }, platform: "linux", nativeConfigured: false, toolchainExists: true, optionalPacks: [],
  });
  const configures = plan.steps.filter((step) => step.kind === "run" && step.args.includes("-S"));
  assert.equal(configures.length, 2);
  for (const configure of configures) assert.ok(configure.args.some((arg) => /^-DGAME_REPO_ROOT=(?:[A-Za-z]:)?\/repo$/.test(arg)), configure.args.join(" "));
});

test("a host tree is reconfigured when its game options change, and a dropped option is unset", () => {
  const plan = (hostArgsRecord, host) => createBuildPlan({
    gameDir: "/repo/games/example", studioRoot: "/repo",
    args: { preset: "wasm-release", target: "itch", debugUi: "off" },
    env: { EMSDK: "/opt/emsdk" }, platform: "linux", nativeConfigured: true, toolchainExists: true, optionalPacks: [],
    hostArgsRecord, configureArgs: () => ({ host }),
  });
  const repoRoot = plan([], []).steps.find((step) => step.kind === "stamp").value[0];
  const hostConfigure = (built) => built.steps.find((step) => step.kind === "run" && step.args.includes("-S") && !step.args.some((arg) => arg.startsWith("-DGAME_PUBLISH_TARGET=")));

  const unchanged = plan([repoRoot, "-DGAME_TESTBED=OFF"], ["-DGAME_TESTBED=OFF"]);
  assert.equal(hostConfigure(unchanged), undefined);
  assert.equal(unchanged.steps.some((step) => step.kind === "stamp"), false);

  const added = plan([repoRoot], ["-DGAME_TESTBED=OFF"]);
  assert.ok(hostConfigure(added).args.includes("-DGAME_TESTBED=OFF"));
  assert.deepEqual(added.steps.find((step) => step.kind === "stamp").value, [repoRoot, "-DGAME_TESTBED=OFF"]);

  const dropped = plan([repoRoot, "-DGAME_TESTBED=OFF"], []);
  assert.ok(hostConfigure(dropped).args.includes("-UGAME_TESTBED"));
  assert.ok(!hostConfigure(dropped).args.includes("-DGAME_TESTBED=OFF"));
});

test("an optional pack the host no longer makes is removed from the staged build", () => {
  const events = [];
  executeBuildPlan({ env: {}, steps: [{ kind: "copy_optional", from: "host/music.ntpack", to: "bin/music.ntpack" }], message: "done" }, {
    mkdir: () => {}, exists: () => false, copy: () => assert.fail("nothing to copy"), remove: (path) => events.push(["remove", path]),
  });
  assert.deepEqual(events, [["remove", "bin/music.ntpack"]]);
});
