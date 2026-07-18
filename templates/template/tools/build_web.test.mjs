import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import test from "node:test";

import { createBuildPlan, executeBuildPlan, parseBuildArgs, resolveEmcmakePath } from "./build_web.mjs";

const slash = (value) => value.replaceAll("\\", "/").replace(/^[A-Za-z]:/, "");

test("build web arguments preserve preset target and release debug validation", () => {
  assert.deepEqual(parseBuildArgs([]), { preset: "wasm-release", target: "local", debugUi: "default" });
  assert.deepEqual(parseBuildArgs(["--preset", "wasm-devapi-debug", "--target", "poki", "--no-debug-ui"]), {
    preset: "wasm-devapi-debug", target: "poki", debugUi: "off",
  });
  assert.throws(() => parseBuildArgs(["--preset", "bad"]), /unknown preset/);
  assert.throws(() => parseBuildArgs(["--target", "bad"]), /unknown target/);
  assert.throws(() => parseBuildArgs(["--debug-ui"]), /not allowed in wasm-release/);
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
  });
  assert.equal(slash(plan.env.EM_CACHE), "/repo/templates/template/build/emscripten-cache");
  assert.equal(slash(plan.nativeDir), "/repo/templates/template/build/native-debug");
  assert.equal(slash(plan.webDir), "/repo/templates/template/build/wasm-devapi-debug-yandex");
  assert.deepEqual(plan.steps.map((step) => step.kind), ["mkdir", "run", "run", "run", "run", "run", "copy", "write"]);
  const runSteps = plan.steps.filter((step) => step.kind === "run");
  assert.equal(runSteps[0].args.slice(-3).join(" "), "-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug");
  assert.deepEqual(runSteps.slice(1).map((step) => step.args.slice(-2).join(" ")), [
    "--target game_asset_packs",
    "-DGAME_PLATFORM_SDK_DEBUG_UI=ON -DGAME_DEVAPI_ENABLED=ON",
    "--target game",
    "--target platform_sdk_web_assets",
  ]);
  assert.equal(plan.steps.at(-1).kind, "write");
  assert.equal(slash(plan.steps.at(-2).from), "/repo/templates/template/build/native-debug/bin/assets/game.ntpack");
  assert.equal(slash(plan.steps.at(-2).to), "/repo/templates/template/build/wasm-devapi-debug-yandex/bin/assets/game.ntpack");
  assert.equal(slash(plan.steps.at(-1).path), "/repo/templates/template/build/wasm-devapi-debug-yandex/bin/runtime-build.json");
  assert.deepEqual(plan.steps.at(-1).value, runtimeBuild);
  const configure = plan.steps.find((step) => step.kind === "run" && step.args.includes("-DGAME_PUBLISH_TARGET=yandex"));
  assert.ok(configure.args.includes(`-DGAME_RUNTIME_BUILD_FINGERPRINT=${runtimeBuild.fingerprint}`));
  assert.equal(plan.message, "built wasm-devapi-debug-yandex (yandex -> platform-sdk); serve with: node tools/serve_web.mjs --preset wasm-devapi-debug --target yandex");
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

test("plan covers all three presets across all five targets with exact debug flags", () => {
  const presets = ["wasm-release", "wasm-debug", "wasm-devapi-debug"];
  const targets = ["local", "itch", "poki", "yandex", "playgama"];
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
      const configure = plan.steps.find((step) => step.kind === "run" && step.args.includes("-G"));
      assert.ok(configure.args.includes(`-DCMAKE_BUILD_TYPE=${preset === "wasm-release" ? "Release" : "Debug"}`));
      assert.ok(configure.args.includes(`-DGAME_PUBLISH_TARGET=${target}`));
      assert.equal(configure.args.includes("-DGAME_DEVAPI_ENABLED=ON"), preset === "wasm-devapi-debug");
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
