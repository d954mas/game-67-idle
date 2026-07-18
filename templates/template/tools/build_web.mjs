#!/usr/bin/env node

import { copyFileSync, existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, posix, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";

import { findStudioRoot } from "./lib/studio_root.mjs";
import { createRuntimeBuildRecord, validateRuntimeBuildRecord } from "./lib/runtime_build.mjs";

const PRESETS = new Set(["wasm-release", "wasm-debug", "wasm-devapi-debug"]);
const TARGETS = new Set(["local", "itch", "poki", "yandex", "playgama"]);

export function parseBuildArgs(argv) {
  const args = { preset: "wasm-release", target: "local", debugUi: "default" };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--preset") args.preset = argv[++index] || "";
    else if (arg === "--target") args.target = argv[++index] || "";
    else if (arg === "--debug-ui") args.debugUi = "on";
    else if (arg === "--no-debug-ui") args.debugUi = "off";
    else throw new Error(`unknown option: ${arg}`);
  }
  if (!PRESETS.has(args.preset)) throw new Error(`unknown preset: ${args.preset} (use ${[...PRESETS].join("|")})`);
  if (!TARGETS.has(args.target)) throw new Error(`unknown target: ${args.target} (use ${[...TARGETS].join("|")})`);
  if (args.preset === "wasm-release" && args.debugUi === "on") {
    throw new Error("debug UI is not allowed in wasm-release artifacts; use wasm-debug or --no-debug-ui");
  }
  return args;
}

export function resolveEmcmakePath(environment = {}, platform = process.platform) {
  if (environment.EMCMAKE) return environment.EMCMAKE;
  if (platform === "linux" && environment.EMSDK) return posix.join(environment.EMSDK, "upstream/emscripten/emcmake");
  return platform === "win32" ? "" : "emcmake";
}

function buildName(args) {
  return args.target === "local" ? args.preset : `${args.preset}-${args.target}`;
}

export function createBuildPlan(options) {
  const gameDir = resolve(options.gameDir);
  const args = options.args;
  const runtimeBuild = options.runtimeBuild ? validateRuntimeBuildRecord(options.runtimeBuild) : null;
  const platform = options.platform || process.platform;
  const inputEnv = options.env || {};
  const name = buildName(args);
  const nativeDir = join(gameDir, "build", "native-debug");
  const webDir = join(gameDir, "build", name);
  const emsdk = inputEnv.EMSDK || "";
  const toolchain = emsdk ? join(emsdk, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake") : "";
  if (emsdk && !options.toolchainExists) throw new Error(`EMSDK is set but its CMake toolchain is missing: ${toolchain}`);
  const concreteWindowsEmcmake = options.emcmakePath && /^[A-Za-z]:[\\/]/.test(options.emcmakePath);
  if (!emsdk && platform === "win32" && !concreteWindowsEmcmake) {
    throw new Error("EMSDK is required on Windows unless an absolute emcmake executable is provided");
  }
  const env = { ...inputEnv, EM_CACHE: inputEnv.EM_CACHE || join(gameDir, "build", "emscripten-cache") };
  const steps = [{ kind: "mkdir", path: env.EM_CACHE }];
  if (!options.nativeConfigured) {
    const nativeArgs = ["-S", gameDir, "-B", nativeDir, "-G", "Ninja", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_BUILD_TYPE=Debug"];
    if (platform === "linux") nativeArgs.push("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined");
    steps.push({ kind: "run", command: "cmake", args: nativeArgs });
  }
  steps.push({ kind: "run", command: "cmake", args: ["--build", nativeDir, "--target", "game_asset_packs"] });
  const configureArgs = ["-S", gameDir, "-B", webDir, "-G", "Ninja"];
  let configureCommand = "cmake";
  if (options.emcmakePath) {
    configureCommand = options.emcmakePath;
    configureArgs.unshift("cmake");
  } else if (toolchain) configureArgs.push(`-DCMAKE_TOOLCHAIN_FILE=${toolchain}`);
  else {
    configureCommand = "emcmake";
    configureArgs.unshift("cmake");
  }
  configureArgs.push(`-DCMAKE_BUILD_TYPE=${args.preset === "wasm-release" ? "Release" : "Debug"}`, `-DGAME_PUBLISH_TARGET=${args.target}`);
  if (runtimeBuild) configureArgs.push(`-DGAME_RUNTIME_BUILD_FINGERPRINT=${runtimeBuild.fingerprint}`);
  if (args.debugUi !== "default") configureArgs.push(`-DGAME_PLATFORM_SDK_DEBUG_UI=${args.debugUi === "on" ? "ON" : "OFF"}`);
  if (args.preset === "wasm-devapi-debug") configureArgs.push("-DGAME_DEVAPI_ENABLED=ON");
  steps.push({ kind: "run", command: configureCommand, args: configureArgs });
  steps.push({ kind: "run", command: "cmake", args: ["--build", webDir, "--target", "game"] });
  steps.push({ kind: "run", command: "cmake", args: ["--build", webDir, "--target", "platform_sdk_web_assets"] });
  steps.push({ kind: "copy", from: join(nativeDir, "bin", "assets", "game.ntpack"), to: join(webDir, "bin", "assets", "game.ntpack") });
  if (runtimeBuild) steps.push({ kind: "write", path: join(webDir, "bin", "runtime-build.json"), value: runtimeBuild });
  return {
    env, nativeDir, webDir, steps, verifyRuntimeBuild: options.verifyRuntimeBuild,
    message: `built ${name} (${args.target} -> platform-sdk); serve with: node tools/serve_web.mjs --preset ${args.preset} --target ${args.target}`,
  };
}

export function executeBuildPlan(plan, deps = {}) {
  const mkdir = deps.mkdir || ((path) => mkdirSync(path, { recursive: true }));
  const copy = deps.copy || ((from, to) => { mkdir(dirname(to)); copyFileSync(from, to); });
  const write = deps.write || ((path, value) => {
    mkdir(dirname(path));
    writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`);
  });
  const run = deps.run || ((command, args, options) => spawnSync(command, args, { ...options, stdio: "inherit", shell: false }));
  for (const step of plan.steps) {
    if (step.kind === "mkdir") mkdir(step.path);
    else if (step.kind === "copy") copy(step.from, step.to);
    else if (step.kind === "write") {
      (deps.verifyRuntimeBuild || plan.verifyRuntimeBuild)?.();
      write(step.path, step.value);
    } else {
      const result = run(step.command, step.args, { env: plan.env });
      if (result.error) throw result.error;
      if (result.status !== 0) throw new Error(`${step.command} exited ${result.status ?? 1}`);
    }
  }
  return plan.message;
}

export function main(argv = process.argv.slice(2), environment = process.env) {
  try {
    const gameDir = resolve(fileURLToPath(new URL("..", import.meta.url)));
    const args = parseBuildArgs(argv);
    const studioRoot = findStudioRoot(gameDir);
    const runtimeBuild = createRuntimeBuildRecord({ gameDir, studioRoot });
    const emsdk = environment.EMSDK || (process.platform === "win32" && existsSync("C:/develop/emsdk") ? "C:/develop/emsdk" : "");
    const toolchain = emsdk ? join(emsdk, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake") : "";
    const plan = createBuildPlan({
      gameDir,
      args,
      env: { ...environment, ...(emsdk ? { EMSDK: emsdk } : {}) },
      platform: process.platform,
      emcmakePath: resolveEmcmakePath({ ...environment, ...(emsdk ? { EMSDK: emsdk } : {}) }, process.platform),
      nativeConfigured: existsSync(join(gameDir, "build", "native-debug", "CMakeCache.txt")),
      toolchainExists: Boolean(toolchain && existsSync(toolchain)),
      runtimeBuild,
      verifyRuntimeBuild() {
        const after = createRuntimeBuildRecord({ gameDir, studioRoot });
        if (JSON.stringify(after) !== JSON.stringify(runtimeBuild)) {
          throw new Error("runtime build inputs changed while the web artifact was building");
        }
      },
    });
    console.log(executeBuildPlan(plan));
    return 0;
  } catch (error) {
    console.error(error?.message || String(error));
    return error?.message?.startsWith("unknown ") || error?.message?.startsWith("debug UI") ? 2 : 1;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  process.exitCode = main();
}
