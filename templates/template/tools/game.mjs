#!/usr/bin/env node

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";

import { main as buildWeb } from "./build_web.mjs";
import {
  packageWebArtifact,
  validateDependencies,
  validateWebArtifact,
  verifyDependencySources,
} from "./package_web.mjs";

const GAME_DIR = resolve(fileURLToPath(new URL("..", import.meta.url)));
const PACKAGE_TARGETS = new Set(["itch", "poki", "yandex", "playgama"]);
const BUILD_TARGETS = new Set(["local", ...PACKAGE_TARGETS]);
const COMMANDS = new Set(["doctor", "build", "run", "test", "playable", "package", "verify"]);
const USAGE = "usage: node tools/game.mjs <doctor|build|run|test|playable|package|verify> [--target local|itch|poki|yandex|playgama] [--no-build] [--out <dir>] [--template-proof] [--skip-tests]";

function readJson(path, label) {
  let value;
  try { value = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, "")); }
  catch (error) { throw new Error(`${label} is not valid JSON: ${error.message}`); }
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value;
}

export function parseGameArgs(argv) {
  const command = argv[0] || "";
  if (!COMMANDS.has(command)) throw new Error(USAGE);
  const args = {
    command,
    target: ["playable", "package", "verify"].includes(command) ? "itch" : "local",
    build: true,
    templateProof: false,
    skipTests: false,
    outDir: "",
  };
  for (let index = 1; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--target") args.target = argv[++index] || "";
    else if (arg === "--no-build") args.build = false;
    else if (arg === "--template-proof") args.templateProof = true;
    else if (arg === "--skip-tests") args.skipTests = true;
    else if (arg === "--out") args.outDir = argv[++index] || "";
    else throw new Error(`unknown option: ${arg}\n${USAGE}`);
  }
  if (!BUILD_TARGETS.has(args.target) || (["package", "verify"].includes(command) && !PACKAGE_TARGETS.has(args.target))) {
    throw new Error(`unknown target for ${command}: ${args.target}`);
  }
  if (!args.build && !["playable", "package", "verify"].includes(command)) throw new Error(`--no-build is not valid for ${command}`);
  if (args.templateProof && !["package", "verify"].includes(command)) throw new Error(`--template-proof is not valid for ${command}`);
  if (args.skipTests && command !== "verify") throw new Error(`--skip-tests is not valid for ${command}`);
  if (args.skipTests && !args.templateProof) throw new Error("--skip-tests is valid only with --template-proof");
  if (args.outDir && !["package", "verify"].includes(command)) throw new Error(`--out is not valid for ${command}`);
  return args;
}

function requiredScaffold(gameDir) {
  return [
    "CMakeLists.txt",
    "tools/game.mjs",
    "tools/build_web.mjs",
    "tools/package_web.mjs",
    "tools/lib/zip_store.mjs",
    "tools/serve_web.mjs",
    "release/README.md",
    ".github/workflows/game-verify.yml",
  ].map((path) => join(gameDir, ...path.split("/")));
}

function validateGameIdentity(value) {
  if (value.schema !== "ai_studio.game.v1" || !/^[a-z][a-z0-9-]*$/.test(value.id || "")
      || !String(value.title || "").trim() || !/^[a-z][a-z0-9-]*$/.test(value.storageNamespace || "")) {
    throw new Error("game identity is invalid");
  }
  return value;
}

function validateTemplateIdentity(value) {
  if (JSON.stringify(Object.keys(value).sort()) !== JSON.stringify(["id", "schema", "storageNamespace", "title"])) {
    throw new Error("template identity has unexpected fields");
  }
  if (value.schema !== "ai_studio.template.v1" || !/^[a-z][a-z0-9-]*$/.test(value.id || "")
      || !String(value.title || "").trim() || !/^[a-z][a-z0-9-]*$/.test(value.storageNamespace || "")) {
    throw new Error("template identity is invalid");
  }
  return value;
}

export function doctorGame({ gameDir = GAME_DIR, templateProof = false } = {}) {
  const root = resolve(gameDir);
  const missing = requiredScaffold(root).filter((path) => !existsSync(path));
  if (missing.length) throw new Error(`game scaffold is missing: ${missing.map((path) => path.slice(root.length + 1)).join(", ")}`);
  if (templateProof) {
    const template = validateTemplateIdentity(readJson(join(root, "template.json"), "template identity"));
    const seed = readJson(join(root, "game-dependencies.json"), "template dependency seed");
    if (template.schema !== "ai_studio.template.v1" || seed.schema !== "ai_studio.game.dependencies.seed.v2") throw new Error("reference-template proof metadata is invalid");
    return { gameId: template.id, kind: "reference-template" };
  }
  const identity = validateGameIdentity(readJson(join(root, "game.json"), "game identity"));
  validateDependencies(readJson(join(root, "dependencies.json"), "dependencies"));
  return { gameId: identity.id, kind: "game" };
}

function run(command, args, cwd, label) {
  const result = spawnSync(command, args, { cwd, shell: false, stdio: "inherit", env: process.env });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${label} exited ${result.status ?? 1}`);
}

function runNodeTests(gameDir) {
  const tools = join(gameDir, "tools");
  const files = readdirSync(tools).filter((name) => name.endsWith(".test.mjs")).sort().map((name) => join("tools", name));
  if (files.length === 0) throw new Error("game scaffold has no Node tests");
  run(process.execPath, ["--test", "--test-concurrency=1", ...files], gameDir, "game tests");
}

export function nativeTestPlan(gameDir, platform = process.platform) {
  const buildDir = join(gameDir, "build", "native-debug");
  return [
    ["cmake", "-S", gameDir, "-B", buildDir, "-G", "Ninja", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_BUILD_TYPE=Debug",
      ...(platform === "linux" ? ["-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined"] : [])],
    ["cmake", "--build", buildDir],
    ["ctest", "--test-dir", buildDir, "--output-on-failure"],
  ];
}

function runNativeTests(gameDir) {
  for (const [command, ...args] of nativeTestPlan(gameDir)) run(command, args, gameDir, "native game tests");
}

function buildGame(gameDir, target) {
  const code = buildWeb(["--preset", "wasm-release", "--target", target, "--no-debug-ui"], process.env);
  if (code !== 0) throw new Error(`web build exited ${code}`);
}

function artifactDir(gameDir, target) {
  return join(gameDir, "build", target === "local" ? "wasm-release" : `wasm-release-${target}`, "bin");
}

function gitRevision(cwd, label) {
  const safe = resolve(cwd).replaceAll("\\", "/");
  const result = spawnSync("git", ["-c", `safe.directory=${safe}`, "rev-parse", "HEAD"], { cwd, encoding: "utf8", shell: false });
  const revision = result.status === 0 ? result.stdout.trim().toLowerCase() : "";
  if (!/^[0-9a-f]{40}$/.test(revision)) throw new Error(`${label} requires an exact Git revision`);
  return revision;
}

function referenceTemplatePackageMetadata(gameDir) {
  const template = validateTemplateIdentity(readJson(join(gameDir, "template.json"), "template identity"));
  const seed = readJson(join(gameDir, "game-dependencies.json"), "template dependency seed");
  const studioRoot = resolve(gameDir, "..", "..");
  const repoRevision = gitRevision(studioRoot, "reference-template proof");
  const engineRevision = gitRevision(join(studioRoot, seed.engine?.source || ""), "reference-template engine proof");
  const dependencies = {
    schema: "ai_studio.game.dependencies.v2",
    engine: { ...seed.engine, revision: engineRevision },
    features: (seed.features || []).map((feature) => ({ ...feature, revision: repoRevision })),
    compatibility: `${seed.compatibility}; reference-template proof at exact Studio revision ${repoRevision}`,
  };
  validateDependencies(dependencies);
  return {
    identity: {
      schema: "ai_studio.game.v1", id: template.id, title: template.title, storageNamespace: template.storageNamespace,
    },
    dependencies,
    proof: "reference-template",
  };
}

function gamePackageMetadata(gameDir, templateProof) {
  if (templateProof) return referenceTemplatePackageMetadata(gameDir);
  return { dependencies: validateDependencies(readJson(join(gameDir, "dependencies.json"), "dependencies")), proof: "game" };
}

function packageGame(options, dependencies = {}, metadata = null) {
  const gameDir = resolve(dependencies.gameDir || GAME_DIR);
  if (options.build) (dependencies.build || buildGame)(gameDir, options.target);
  const proof = metadata || gamePackageMetadata(gameDir, options.templateProof);
  return packageWebArtifact({
    gameDir,
    artifactDir: artifactDir(gameDir, options.target),
    target: options.target,
    studioRoot: resolve(gameDir, "..", ".."),
    ...(options.outDir ? { outDir: resolve(gameDir, options.outDir) } : {}),
    ...proof,
    ...(metadata ? { dependencyVerifier: () => {} } : {}),
  });
}

export async function executeGameCommand(args, dependencies = {}) {
  const gameDir = resolve(dependencies.gameDir || GAME_DIR);
  const doctor = dependencies.doctor || doctorGame;
  const nodeTest = dependencies.nodeTest || runNodeTests;
  const nativeTest = dependencies.nativeTest || runNativeTests;
  const loadMetadata = dependencies.loadMetadata || gamePackageMetadata;
  const verifyDependencies = dependencies.verifyDependencies || ((metadata) => verifyDependencySources({ studioRoot: resolve(gameDir, "..", ".."), dependencies: metadata.dependencies }));
  const packageGameOwned = dependencies.package || ((options, metadata) => packageGame(options, { gameDir }, metadata));
  const prepare = (templateProof = false) => {
    doctor({ gameDir, templateProof });
    const metadata = loadMetadata(gameDir, templateProof);
    verifyDependencies(metadata);
    return metadata;
  };
  if (args.command === "doctor") {
    const result = doctor({ gameDir, templateProof: false });
    verifyDependencies(loadMetadata(gameDir, false));
    return { message: `doctor passed (${result.gameId})` };
  }
  if (args.command === "build") {
    prepare(false);
    (dependencies.build || buildGame)(gameDir, args.target);
    return { message: `built wasm-release-${args.target}` };
  }
  if (args.command === "run") {
    run(process.execPath, [join("tools", "serve_web.mjs"), "--preset", "wasm-release", "--target", args.target], gameDir, "game server");
    return { message: "game server stopped" };
  }
  if (args.command === "test") {
    prepare(false);
    nodeTest(gameDir);
    nativeTest(gameDir);
    return { message: "game tests passed" };
  }
  if (args.command === "playable") {
    prepare(false);
    if (args.build) (dependencies.build || buildGame)(gameDir, args.target);
    validateWebArtifact({ gameDir, artifactDir: artifactDir(gameDir, args.target), target: args.target, studioRoot: resolve(gameDir, "..", "..") });
    return { message: `playable proof passed (${args.target})` };
  }
  if (args.command === "package") {
    const metadata = prepare(args.templateProof);
    const result = packageGameOwned(args, metadata);
    return { message: `package passed: ${result.zipPath}`, result };
  }
  const metadata = prepare(args.templateProof);
  if (!args.skipTests) {
    nodeTest(gameDir);
    nativeTest(gameDir);
  }
  const result = packageGameOwned(args, metadata);
  return { message: `verify passed: ${result.zipPath}`, result };
}

export async function main(argv = process.argv.slice(2), dependencies = {}) {
  try {
    const args = parseGameArgs(argv);
    const result = await executeGameCommand(args, dependencies);
    console.log(result.message);
    return 0;
  } catch (error) {
    console.error(error?.message || String(error));
    return String(error?.message || "").startsWith("usage:") || String(error?.message || "").startsWith("unknown option") ? 2 : 1;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  process.exitCode = await main();
}
