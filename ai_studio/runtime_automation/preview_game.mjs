#!/usr/bin/env node

import { existsSync, lstatSync, realpathSync, rmSync, writeFileSync } from "node:fs";
import { fileURLToPath, pathToFileURL } from "node:url";
import { dirname, isAbsolute, join, relative, resolve, sep } from "node:path";

import { listWorkspaceMounts } from "../workspace/catalog.mjs";

const PREVIEW_SCHEMA = "ai_studio.runtime_build.preview.v1";
const PREVIEW_PRESETS = new Set(["wasm-debug", "wasm-devapi-debug"]);

export function parsePreviewArgs(argv) {
  let gameId = "";
  const buildArgs = [];
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--game") gameId = argv[++index] || "";
    else buildArgs.push(arg);
  }
  if (!/^[a-z][a-z0-9-]*$/.test(gameId)) throw new Error("--game must be a canonical game id");
  return { gameId, buildArgs };
}

function inside(root, candidate) {
  const rel = relative(root, candidate);
  return rel === "" || (!rel.startsWith("..") && !isAbsolute(rel));
}

export function resolveGameDir(studioRoot, gameId, listMounts = listWorkspaceMounts) {
  const mounts = listMounts(studioRoot, { includePrivate: true, activeGameId: gameId, kinds: ["game"] });
  const matches = mounts.filter((mount) => mount.gameId === gameId);
  if (matches.length !== 1) throw new Error(`game ${gameId} has no unique catalog mount`);
  const physicalStudio = realpathSync(studioRoot);
  const gameDir = realpathSync(join(studioRoot, matches[0].root));
  if (!inside(physicalStudio, gameDir)) throw new Error("game catalog mount escapes the Studio root");
  const moduleCandidate = join(gameDir, "tools", "build_web.mjs");
  if (!existsSync(moduleCandidate)) throw new Error(`game ${gameId} has no supported web build tool`);
  const modulePath = realpathSync(moduleCandidate);
  if (!inside(gameDir, modulePath)) throw new Error("game web build module escapes its catalog mount");
  return gameDir;
}

function assertArtifactDir(gameDir, artifactDir, requirePhysical) {
  const buildRoot = resolve(gameDir, "build");
  const candidate = resolve(artifactDir);
  if (!inside(buildRoot, candidate)) throw new Error("game web artifact directory escapes its build root");
  let component = buildRoot;
  for (const part of relative(buildRoot, candidate).split(sep).filter(Boolean)) {
    try {
      if (lstatSync(component).isSymbolicLink()) throw new Error("game web artifact path must not contain a symlink or junction");
    } catch (error) {
      if (error?.code !== "ENOENT") throw error;
    }
    component = join(component, part);
  }
  try {
    if (lstatSync(component).isSymbolicLink()) throw new Error("game web artifact path must not contain a symlink or junction");
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
  if (!existsSync(gameDir)) return;
  const physicalGame = realpathSync(gameDir);
  let ancestor = candidate;
  while (!existsSync(ancestor) && ancestor !== buildRoot) ancestor = dirname(ancestor);
  if (!existsSync(ancestor)) {
    if (requirePhysical) throw new Error("game web artifact directory is missing after build");
    return;
  }
  const physicalAncestor = realpathSync(ancestor);
  if (!inside(physicalGame, physicalAncestor)) throw new Error("game web artifact directory escapes its physical game root");
  if (!existsSync(buildRoot)) {
    if (requirePhysical) throw new Error("game web build root is missing after build");
    return;
  }
  const physicalBuild = realpathSync(buildRoot);
  if (!inside(physicalGame, physicalBuild)) throw new Error("game web build root escapes its physical game root");
  if (!existsSync(candidate)) {
    if (requirePhysical) throw new Error("game web artifact directory is missing after build");
    return;
  }
  const physicalArtifact = realpathSync(candidate);
  if (!inside(physicalBuild, physicalArtifact)) throw new Error("game web artifact directory escapes its physical build root");
}

// A game with its own copied packager still carries its own builder; a current
// game's tools/build_web.mjs is a shim that builds on import, so the Studio
// builder is loaded instead.
export function resolveGameModule(gameDir, studioRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..")) {
  if (!existsSync(join(gameDir, "tools", "package_web.mjs"))) {
    return realpathSync(join(studioRoot, "ai_studio", "game_tools", "build_web.mjs"));
  }
  const modulePath = realpathSync(join(gameDir, "tools", "build_web.mjs"));
  if (!inside(gameDir, modulePath)) throw new Error("game web build module escapes its catalog mount");
  return modulePath;
}

function supportedBuilder(builder) {
  if (typeof builder.parseBuildArgs !== "function" || typeof builder.createBuildPlan !== "function" || typeof builder.executeBuildPlan !== "function") {
    throw new Error("game web build tool does not expose the supported preview API");
  }
}

function previewArtifactDir(plan) {
  for (const step of [...plan.steps].reverse()) {
    if (step.kind !== "run") continue;
    const index = step.args.indexOf("--artifact");
    if (index >= 0 && typeof step.args[index + 1] === "string") return resolve(step.args[index + 1]);
  }
  if (typeof plan.webDir !== "string") throw new Error("game web build plan does not expose an artifact directory");
  return join(plan.webDir, "bin");
}

function markPreviewConfigure(plan, preset) {
  let found = false;
  for (const step of plan.steps) {
    if (step.kind !== "run" || !step.args.includes("-S") || !step.args.includes("-B")) continue;
    step.args = step.args.filter((arg) => !arg.startsWith("-DGAME_RUNTIME_BUILD_PREVIEW=")
      && !arg.startsWith("-DGAME_RUNTIME_BUILD_FINGERPRINT=")
      && !arg.startsWith("-DGAME_RUNTIME_BUILD_PROFILE="));
    step.args.push(`-DGAME_RUNTIME_BUILD_PROFILE=${preset}`, "-DGAME_RUNTIME_BUILD_PREVIEW=ON", "-DGAME_RUNTIME_BUILD_FINGERPRINT=");
    found = true;
  }
  if (!found) throw new Error("game web build plan does not expose a CMake configure step");
}

export function createPreviewPlan({ builder, gameDir, buildArgs, environment = process.env, platform = process.platform, options = {} }) {
  supportedBuilder(builder);
  const args = builder.parseBuildArgs(buildArgs);
  if (!PREVIEW_PRESETS.has(args.preset)) throw new Error("preview requires wasm-debug or wasm-devapi-debug");
  const plan = builder.createBuildPlan({
    gameDir,
    args,
    env: environment,
    platform,
    ...options,
    runtimeBuild: null,
    verifyRuntimeBuild: null,
  });
  markPreviewConfigure(plan, args.preset);
  const artifactDir = previewArtifactDir(plan);
  assertArtifactDir(gameDir, artifactDir, false);
  return {
    plan,
    gameDir,
    artifactDir,
    previewRecord: { schema: PREVIEW_SCHEMA, proof: "none", preset: args.preset, target: args.target },
  };
}

export function executePreviewPlan({ builder, plan, gameDir, artifactDir, previewRecord }) {
  assertArtifactDir(gameDir, artifactDir, false);
  const recordPath = join(artifactDir, "runtime-build.json");
  rmSync(recordPath, { force: true });
  const message = builder.executeBuildPlan(plan);
  assertArtifactDir(gameDir, artifactDir, true);
  try {
    if (lstatSync(recordPath).isSymbolicLink()) throw new Error("game web runtime build record must not be a symlink");
  } catch (error) {
    if (error?.code !== "ENOENT") throw error;
  }
  writeFileSync(recordPath, `${JSON.stringify(previewRecord, null, 2)}\n`);
  return `built NON-PROOF preview: ${message}`;
}

export async function main(argv = process.argv.slice(2), environment = process.env) {
  try {
    const { gameId, buildArgs } = parsePreviewArgs(argv);
    const studioRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
    const gameDir = resolveGameDir(studioRoot, gameId);
    const builder = await import(pathToFileURL(resolveGameModule(gameDir, studioRoot)).href);
    supportedBuilder(builder);
    const emsdk = environment.EMSDK || (process.platform === "win32" && existsSync("C:/develop/emsdk") ? "C:/develop/emsdk" : "");
    const toolchain = emsdk ? join(emsdk, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake") : "";
    const prepared = createPreviewPlan({
      builder,
      gameDir,
      buildArgs,
      environment: { ...environment, ...(emsdk ? { EMSDK: emsdk } : {}) },
      options: {
        emcmakePath: typeof builder.resolveEmcmakePath === "function"
          ? builder.resolveEmcmakePath({ ...environment, ...(emsdk ? { EMSDK: emsdk } : {}) }, process.platform)
          : undefined,
        nativeConfigured: existsSync(join(gameDir, "build", "native-debug", "CMakeCache.txt")),
        toolchainExists: Boolean(toolchain && existsSync(toolchain)),
      },
    });
    console.log(executePreviewPlan({ builder, ...prepared }));
    return 0;
  } catch (error) {
    console.error(error?.message || String(error));
    return 1;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exitCode = await main();
}
