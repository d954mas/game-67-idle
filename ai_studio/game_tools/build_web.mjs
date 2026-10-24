#!/usr/bin/env node

import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, posix, relative, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";

import { findStudioRoot } from "./lib/studio_root.mjs";
import { createRuntimeBuildRecord, validateRuntimeBuildRecord } from "./lib/runtime_build.mjs";
import { readGameAssetPacks, stageGameAssetPacks } from "./lib/game_asset_packs.mjs";
import { retainWasmSymbols } from "./lib/wasm_symbol_artifact.mjs";

// wasm-devapi-perf: DevAPI at an optimised build type, without the analytics and
// log mirror that cost more than the game in a busy frame; it measures what ships.
const PRESETS = new Set(["wasm-release", "wasm-release-asserts", "wasm-debug", "wasm-devapi-debug", "wasm-devapi-perf"]);
const BUILD_TYPES = { "wasm-release": "Release", "wasm-release-asserts": "Release", "wasm-devapi-perf": "RelWithDebInfo" };
const isReleasePreset = (preset) => preset === "wasm-release" || preset === "wasm-release-asserts";
const TOOLS_DIR = dirname(fileURLToPath(import.meta.url));
const STUDIO_ROOT = resolve(TOOLS_DIR, "..", "..");
const PUBLISH_TARGETS_DIR = join(STUDIO_ROOT, "features", "platform-sdk", "publish-targets");
// The host options a host tree was configured with, kept beside its cache: CMake
// keeps a cached option until it is changed or removed, never by omission.
export const HOST_ARGS_STAMP = "game-host-args.json";
const { compileProfileForTarget, targetNames } = await import(pathToFileURL(join(PUBLISH_TARGETS_DIR, "target_config.mjs")).href);
const TARGETS = new Set(targetNames());
const PREVIEW_SCHEMA = "ai_studio.runtime_build.preview.v1";

export function parseBuildArgs(argv) {
  const args = { preset: "wasm-release", target: "local", debugUi: "default", preview: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--preset") args.preset = argv[++index] || "";
    else if (arg === "--target") args.target = argv[++index] || "";
    else if (arg === "--debug-ui") args.debugUi = "on";
    else if (arg === "--no-debug-ui") args.debugUi = "off";
    else if (arg === "--preview") args.preview = true;
    else throw new Error(`unknown option: ${arg}`);
  }
  if (!PRESETS.has(args.preset)) throw new Error(`unknown preset: ${args.preset} (use ${[...PRESETS].join("|")})`);
  if (!TARGETS.has(args.target)) throw new Error(`unknown target: ${args.target} (use ${[...TARGETS].join("|")})`);
  if (isReleasePreset(args.preset) && args.debugUi === "on") {
    throw new Error("debug UI is not allowed in wasm-release artifacts; use wasm-debug or --no-debug-ui");
  }
  if (args.preview && !["wasm-debug", "wasm-devapi-debug"].includes(args.preset)) {
    throw new Error("preview requires wasm-debug or wasm-devapi-debug");
  }
  return args;
}

export function resolveEmcmakePath(environment = {}, platform = process.platform) {
  if (environment.EMCMAKE) return environment.EMCMAKE;
  if (platform === "linux" && environment.EMSDK) return posix.join(environment.EMSDK, "upstream/emscripten/emcmake");
  return platform === "win32" ? "" : "emcmake";
}

export function hostBuildDir(gameDir, name) {
  return join(gameDir, "build", `${name}-host`);
}

function buildName(args) {
  return args.target === "local" ? args.preset : `${args.preset}-${args.target}`;
}

export function nativeTreeFor(gameDir, args) {
  return hostBuildDir(gameDir, buildName(args));
}

export function compileFlagsForArgs(args) {
  return Object.freeze({
    debugUi: args.debugUi === "on",
    devapi: args.preset === "wasm-devapi-debug" || args.preset === "wasm-devapi-perf",
    analytics: args.preset === "wasm-devapi-debug",
    eventsLogMirror: args.preset === "wasm-debug" || args.preset === "wasm-devapi-debug",
  });
}

// Extra packs the target may ship beside game.ntpack (music, late-loaded fonts);
// a local build stages what itch would.
export function optionalPackPaths(target, read = (path) => readFileSync(path, "utf8")) {
  const manifest = JSON.parse(read(join(PUBLISH_TARGETS_DIR, `${target === "local" ? "itch" : target}.json`)));
  return (manifest.optional_files || []).filter((path) => /^assets\/[^/]+\.ntpack$/.test(path));
}

function gameConfigureArgs(options, args) {
  const extra = options.configureArgs ? options.configureArgs(args) : {};
  for (const key of ["host", "web"]) {
    if (extra[key] !== undefined && (!Array.isArray(extra[key]) || extra[key].some((arg) => typeof arg !== "string" || !arg.startsWith("-D")))) {
      throw new Error(`game configure args.${key} must be a list of -D options`);
    }
  }
  return { host: extra.host || [], web: extra.web || [] };
}

export function createBuildPlan(options) {
  const gameDir = resolve(options.gameDir);
  const args = options.args;
  if (isReleasePreset(args.preset) && args.debugUi === "on") {
    throw new Error("debug UI is not allowed in wasm-release artifacts; use wasm-debug or --no-debug-ui");
  }
  if (args.preview && !["wasm-debug", "wasm-devapi-debug"].includes(args.preset)) {
    throw new Error("preview requires wasm-debug or wasm-devapi-debug");
  }
  if (args.preview && options.runtimeBuild) {
    throw new Error("preview must not receive a runtime build proof");
  }
  const compileProfile = compileProfileForTarget(args.target, args.preset, compileFlagsForArgs(args));
  const runtimeBuild = options.runtimeBuild ? validateRuntimeBuildRecord(options.runtimeBuild) : null;
  if (runtimeBuild?.schema === "ai_studio.runtime_build.v2" && JSON.stringify(runtimeBuild.profile) !== JSON.stringify(compileProfile)) {
    throw new Error("runtime build profile does not match the configured compiler flags");
  }
  const platform = options.platform || process.platform;
  const inputEnv = options.env || {};
  const name = buildName(args);
  // Each web build packs assets in its own native host tree, so it never shares
  // a ninja tree with `game.mjs test` or with a concurrent build of another preset.
  const nativeDir = hostBuildDir(gameDir, name);
  const webDir = join(gameDir, "build", name);
  const emsdk = inputEnv.EMSDK || "";
  const toolchain = emsdk ? join(emsdk, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake") : "";
  if (emsdk && !options.toolchainExists) throw new Error(`EMSDK is set but its CMake toolchain is missing: ${toolchain}`);
  const concreteWindowsEmcmake = options.emcmakePath && /^[A-Za-z]:[\\/]/.test(options.emcmakePath);
  if (!emsdk && platform === "win32" && !concreteWindowsEmcmake) {
    throw new Error("EMSDK is required on Windows unless an absolute emcmake executable is provided");
  }
  const env = { ...inputEnv, EM_CACHE: inputEnv.EM_CACHE || join(gameDir, "build", "emscripten-cache") };
  const extra = gameConfigureArgs(options, args);
  if (isReleasePreset(args.preset) && extra.web.some((arg) => /^-DNT_ASSERT_MODE(?::[^=]+)?=/.test(arg))) {
    throw new Error("release assert mode is owned by the Studio preset");
  }
  const buildType = BUILD_TYPES[args.preset] || "Debug";
  const steps = [{ kind: "mkdir", path: env.EM_CACHE }];
  if (args.preview) steps.push({ kind: "remove", path: join(webDir, "bin", "runtime-build.json") });
  // Both trees compile the Studio that fingerprints and proves the build.
  const repoRootArg = `-DGAME_REPO_ROOT=${resolve(options.studioRoot || STUDIO_ROOT).replace(/\\/g, "/")}`;
  const hostArgs = [repoRootArg, ...extra.host];
  const recordedHostArgs = options.hostArgsRecord || [];
  if (!options.nativeConfigured || JSON.stringify(recordedHostArgs) !== JSON.stringify(hostArgs)) {
    const optionName = (arg) => arg.slice(2).split("=")[0].split(":")[0];
    const current = new Set(hostArgs.map(optionName));
    const removed = [...new Set(recordedHostArgs.map(optionName))].filter((name) => !current.has(name)).map((name) => `-U${name}`);
    const nativeArgs = ["-S", gameDir, "-B", nativeDir, "-G", "Ninja", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_BUILD_TYPE=Debug"];
    if (platform === "linux") nativeArgs.push("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined");
    nativeArgs.push(...removed, ...hostArgs);
    steps.push({ kind: "run", command: "cmake", args: nativeArgs });
    steps.push({ kind: "stamp", path: join(nativeDir, HOST_ARGS_STAMP), value: hostArgs });
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
  configureArgs.push(repoRootArg, `-DCMAKE_BUILD_TYPE=${buildType}`, `-DGAME_PUBLISH_TARGET=${args.target}`);
  configureArgs.push(`-DGAME_RUNTIME_BUILD_PROFILE=${compileProfile.preset}`);
  configureArgs.push("-DGAME_AUDIO_BROWSER_SMOKE=OFF");
  // Games that generate code into the build tree read the host's output from here.
  configureArgs.push(`-DGAME_HOST_GENERATED_DIR=${join(nativeDir, "generated", "game").replace(/\\/g, "/")}`);
  configureArgs.push(`-DGAME_ANALYTICS_ENABLED=${compileProfile.analytics ? "ON" : "OFF"}`);
  configureArgs.push(`-DGAME_EVENTS_LOG_MIRROR=${compileProfile.eventsLogMirror ? "ON" : "OFF"}`);
  configureArgs.push(`-DGAME_RUNTIME_BUILD_DEBUG_UI=${Number(compileProfile.debugUi)}`);
  configureArgs.push(`-DGAME_RUNTIME_BUILD_DEVAPI=${Number(compileProfile.devapi)}`);
  configureArgs.push(`-DGAME_RUNTIME_BUILD_ANALYTICS=${Number(compileProfile.analytics)}`);
  configureArgs.push(`-DGAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR=${Number(compileProfile.eventsLogMirror)}`);
  configureArgs.push(`-DGAME_RUNTIME_BUILD_PREVIEW=${args.preview ? "ON" : "OFF"}`);
  configureArgs.push(`-DGAME_RUNTIME_BUILD_FINGERPRINT=${runtimeBuild ? runtimeBuild.fingerprint : ""}`);
  configureArgs.push(`-DGAME_PLATFORM_SDK_DEBUG_UI=${compileProfile.debugUi ? "ON" : "OFF"}`);
  configureArgs.push(`-DGAME_DEVAPI_ENABLED=${compileProfile.devapi ? "ON" : "OFF"}`);
  if (isReleasePreset(args.preset)) {
    configureArgs.push(`-DNT_ASSERT_MODE=${args.preset === "wasm-release-asserts" ? 2 : 1}`);
    configureArgs.push("-DCMAKE_EXE_LINKER_FLAGS=--emit-symbol-map");
  }
  configureArgs.push(...extra.web);
  steps.push({ kind: "run", command: configureCommand, args: configureArgs });
  if (isReleasePreset(args.preset)) {
    steps.push({ kind: "check_assert_mode", path: join(webDir, "CMakeCache.txt"), expected: args.preset === "wasm-release-asserts" ? 2 : 1 });
  }
  steps.push({ kind: "run", command: "cmake", args: ["--build", webDir, "--target", "game"] });
  // A game that links with a symbol map gets it beside the output, but bin/ is
  // the exact file set the packager allows; the map is kept one level up.
  steps.push({ kind: "retain_symbols", emittedPath: join(webDir, "bin", "game.js.symbols"),
    retainedPath: join(webDir, "game.js.symbols"), wasmPath: join(webDir, "bin", "game.wasm"),
    digestPath: join(webDir, "game.js.symbols.wasm.sha256") });
  steps.push({ kind: "run", command: "cmake", args: ["--build", webDir, "--target", "platform_sdk_web_assets"] });
  steps.push({ kind: "copy", from: join(nativeDir, "bin", "assets", "game.ntpack"), to: join(webDir, "bin", "assets", "game.ntpack") });
  const declaredPacks = readGameAssetPacks(gameDir);
  if (declaredPacks.length || existsSync(join(webDir, "game-asset-packs.json"))) {
    steps.push({ kind: "stage_game_packs", sourceDir: join(nativeDir, "bin"), artifactDir: join(webDir, "bin"), packs: declaredPacks });
  }
  for (const path of options.optionalPacks || optionalPackPaths(args.target)) {
    if (declaredPacks.includes(path)) continue;
    steps.push({ kind: "copy_optional", from: join(nativeDir, "bin", ...path.split("/")), to: join(webDir, "bin", ...path.split("/")) });
  }
  // Download sizes reach the shell before minification and only once the packs
  // are staged: they exist nowhere but on disk, and no CDN header can replace
  // them once the transfer is compressed.
  steps.push({
    kind: "run",
    command: process.execPath,
    args: [join(TOOLS_DIR, "inject_asset_bytes.mjs"), "--artifact", join(webDir, "bin")],
  });
  if (isReleasePreset(args.preset)) {
    steps.push({
      kind: "run",
      command: process.execPath,
      args: [join(TOOLS_DIR, "minify_web_release.mjs"), "--artifact", join(webDir, "bin")],
    });
  }
  const previewRecord = args.preview
    ? { schema: PREVIEW_SCHEMA, proof: "none", preset: args.preset, target: args.target }
    : null;
  if (runtimeBuild || previewRecord) steps.push({ kind: "write", path: join(webDir, "bin", "runtime-build.json"), value: runtimeBuild || previewRecord });
  return {
    env, nativeDir, webDir, steps, verifyRuntimeBuild: args.preview ? null : options.verifyRuntimeBuild,
    message: `${args.preview ? "built NON-PROOF preview" : "built"} ${name} (${args.target} -> platform-sdk); serve with: node ${posix.normalize(relative(gameDir, join(TOOLS_DIR, "serve_web.mjs")).replaceAll("\\", "/"))} --preset ${args.preset} --target ${args.target}`,
  };
}

function readHostArgsStamp(path) {
  if (!existsSync(path)) return null;
  try {
    const value = JSON.parse(readFileSync(path, "utf8"));
    return Array.isArray(value) && value.every((arg) => typeof arg === "string") ? value : null;
  } catch {
    return null;
  }
}

export function executeBuildPlan(plan, deps = {}) {
  const mkdir = deps.mkdir || ((path) => mkdirSync(path, { recursive: true }));
  const copy = deps.copy || ((from, to) => { mkdir(dirname(to)); copyFileSync(from, to); });
  const write = deps.write || ((path, value) => {
    mkdir(dirname(path));
    writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`);
  });
  const remove = deps.remove || ((path) => rmSync(path, { force: true }));
  const exists = deps.exists || existsSync;
  const read = deps.read || readFileSync;
  // A rebuild that relinks nothing re-emits nothing: a missing source means the
  // file already sits where it belongs, or the game does not make one.
  const retainSymbols = deps.retainSymbols || retainWasmSymbols;
  const run = deps.run || ((command, args, options) => spawnSync(command, args, { ...options, stdio: "inherit", shell: false }));
  for (const step of plan.steps) {
    if (step.kind === "mkdir") mkdir(step.path);
    else if (step.kind === "remove") remove(step.path);
    else if (step.kind === "copy") copy(step.from, step.to);
    // A pack the host no longer makes must not linger from an earlier build.
    else if (step.kind === "copy_optional") { if (exists(step.from)) copy(step.from, step.to); else remove(step.to); }
    else if (step.kind === "stage_game_packs") (deps.stageGamePacks || stageGameAssetPacks)(step);
    else if (step.kind === "retain_symbols") retainSymbols(step);
    else if (step.kind === "check_assert_mode") {
      const match = /^NT_ASSERT_MODE:STRING=([12])$/m.exec(read(step.path, "utf8"));
      if (!match || Number(match[1]) !== step.expected) {
        throw new Error(`release assert mode mismatch: expected ${step.expected} in ${step.path}`);
      }
    }
    else if (step.kind === "stamp") write(step.path, step.value);
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

// configureArgs: the game's own CMake options per build, (args) => ({ host, web }).
export function main(argv = process.argv.slice(2), environment = process.env, { gameDir: requestedGameDir = process.cwd(), configureArgs } = {}) {
  try {
    const gameDir = resolve(requestedGameDir);
    const args = parseBuildArgs(argv);
    const studioRoot = findStudioRoot(gameDir);
    const compileProfile = compileProfileForTarget(args.target, args.preset, compileFlagsForArgs(args));
    const runtimeBuild = args.preview ? null : createRuntimeBuildRecord({ gameDir, studioRoot, compileProfile });
    const emsdk = environment.EMSDK || (process.platform === "win32" && existsSync("C:/develop/emsdk") ? "C:/develop/emsdk" : "");
    const toolchain = emsdk ? join(emsdk, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake") : "";
    const plan = createBuildPlan({
      gameDir,
      args,
      env: { ...environment, ...(emsdk ? { EMSDK: emsdk } : {}) },
      platform: process.platform,
      emcmakePath: resolveEmcmakePath({ ...environment, ...(emsdk ? { EMSDK: emsdk } : {}) }, process.platform),
      nativeConfigured: existsSync(join(nativeTreeFor(gameDir, args), "CMakeCache.txt")),
      hostArgsRecord: readHostArgsStamp(join(nativeTreeFor(gameDir, args), HOST_ARGS_STAMP)),
      studioRoot,
      toolchainExists: Boolean(toolchain && existsSync(toolchain)),
      runtimeBuild,
      configureArgs,
      verifyRuntimeBuild: args.preview ? null : () => {
        const after = createRuntimeBuildRecord({ gameDir, studioRoot, compileProfile });
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
