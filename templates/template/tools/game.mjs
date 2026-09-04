#!/usr/bin/env node

import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, readdirSync } from "node:fs";
import { availableParallelism } from "node:os";
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
import { smokePackagedWebArtifact } from "./package_web_smoke.mjs";
import { createRuntimeBuildRecord } from "./lib/runtime_build.mjs";
import { findStudioRoot } from "./lib/studio_root.mjs";

const GAME_DIR = resolve(fileURLToPath(new URL("..", import.meta.url)));
const PACKAGE_TARGETS = new Set(["itch", "poki", "yandex", "playgama"]);
const BUILD_TARGETS = new Set(["local", ...PACKAGE_TARGETS]);
const COMMANDS = new Set(["doctor", "build", "run", "test", "playable", "package", "verify"]);
// The tier vocabulary is shared with cmake/GameTests.cmake; CTest labels carry it.
export const TEST_TIERS = ["core", "slow", "taste"];
export const TEST_TIER_DEFAULT = "core";
const USAGE = "usage: node tools/game.mjs <doctor|build|run|test|playable|package|verify> [--target local|itch|poki|yandex|playgama] [--no-build] [--out <dir>] [--template-proof] [--skip-tests] [test: --tier core|slow|taste | --all | --only <test>] [--update-goldens]";

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
    only: [],
    tier: "",
    all: false,
    updateGoldens: false,
  };
  for (let index = 1; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--target") args.target = argv[++index] || "";
    else if (arg === "--no-build") args.build = false;
    else if (arg === "--template-proof") args.templateProof = true;
    else if (arg === "--skip-tests") args.skipTests = true;
    else if (arg === "--out") args.outDir = argv[++index] || "";
    else if (arg === "--only") args.only.push(argv[++index] || "");
    else if (arg === "--tier") args.tier = argv[++index] || "";
    else if (arg === "--all") args.all = true;
    else if (arg === "--update-goldens") args.updateGoldens = true;
    else throw new Error(`unknown option: ${arg}\n${USAGE}`);
  }
  if (args.only.some((name) => !/^[A-Za-z0-9_.-]+$/.test(name))) throw new Error(`--only takes CTest names\n${USAGE}`);
  if ((args.only.length > 0 || args.updateGoldens || args.tier || args.all) && command !== "test") {
    throw new Error(`--only, --tier, --all and --update-goldens are valid only for test\n${USAGE}`);
  }
  if (args.tier && !TEST_TIERS.includes(args.tier)) throw new Error(`unknown test tier: ${args.tier}\n${USAGE}`);
  if ((args.tier ? 1 : 0) + (args.all ? 1 : 0) + (args.only.length > 0 ? 1 : 0) > 1) {
    throw new Error(`--tier, --all and --only cannot be combined\n${USAGE}`);
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
    "tools/package_web_smoke.mjs",
    "tools/minify_web_release.mjs",
    "tools/lib/studio_root.mjs",
    "tools/lib/runtime_build.mjs",
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

function run(command, args, cwd, label, env = process.env) {
  const result = spawnSync(command, args, { cwd, shell: false, stdio: "inherit", env });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${label} exited ${result.status ?? 1}`);
}

function runNodeTests(gameDir) {
  const tools = join(gameDir, "tools");
  const files = readdirSync(tools).filter((name) => name.endsWith(".test.mjs")).sort().map((name) => join("tools", name));
  if (files.length === 0) throw new Error("game scaffold has no Node tests");
  // Concurrency mirrors the studio gate; every heavy fixture is mkdtemp-isolated.
  run(process.execPath, ["--test", "--test-concurrency=4", ...files], gameDir, "game tests");
}

// CTest labels are the only record of which tier a test belongs to, so the
// runner reads them back instead of keeping a second list that drifts.
export function ctestCatalogue(buildDir, spawn = spawnSync) {
  const result = spawn("ctest", ["--test-dir", buildDir, "--show-only=json-v1"], {
    encoding: "utf8", shell: false, stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`CTest discovery exited ${result.status ?? 1}`);
  const catalogue = JSON.parse(result.stdout).tests.map((entry) => {
    const labels = (entry.properties || []).find((property) => property.name === "LABELS");
    const command = Array.isArray(entry.command) ? entry.command[0] || "" : "";
    const executable = command.replace(/\\/g, "/").split("/").pop() || "";
    return {
      name: entry.name,
      tier: labels ? String([].concat(labels.value)[0] || "") : "",
      // A native test runs its own executable; a Node or Python contract runs an
      // interpreter and owns no build target. CTest omits the command while the
      // executable is still unbuilt, which is exactly the test that needs building.
      target: !command || executable.replace(/\.exe$/i, "") === entry.name ? entry.name : "",
    };
  });
  return catalogue;
}

// A test with no tier would belong to no tier and quietly stop running. The
// check lives after the build, where CMake has regenerated the labels.
export function assertTiersLabelled(catalogue) {
  const unlabelled = catalogue.filter((entry) => !entry.tier);
  if (unlabelled.length > 0) {
    throw new Error(`tests without a tier label: ${unlabelled.map((entry) => entry.name).join(", ")}`);
  }
  return catalogue;
}

// A tier selects tests; the targets those tests need are what gets built. The
// game target is always built so a failing test can be reproduced by playing.
export function selectTests(catalogue, selection = {}) {
  const mode = selection.mode || "tier";
  const selected = mode === "all" ? catalogue
    : mode === "only" ? catalogue.filter((entry) => selection.names.includes(entry.name))
    : catalogue.filter((entry) => entry.tier === (selection.tier || TEST_TIER_DEFAULT));
  if (mode === "only") {
    const missing = selection.names.filter((name) => !catalogue.some((entry) => entry.name === name));
    if (missing.length > 0) throw new Error(`unknown test name: ${missing.join(", ")}`);
  }
  return {
    names: selected.map((entry) => entry.name),
    // Running everything builds everything: a narrowed target list would leave
    // out fixture and tool targets that no test names directly.
    targets: mode === "all" ? [] : ["game", ...new Set(selected.map((entry) => entry.target).filter(Boolean))],
  };
}

export function nativeTestPlan(gameDir, platform = process.platform, configured = false, selection = {}) {
  const buildDir = join(gameDir, "build", "native-debug");
  const mode = selection.mode || "tier";
  const targets = selection.targets || [];
  return [
    ...(configured ? [] : [
      ["cmake", "-S", gameDir, "-B", buildDir, "-G", "Ninja", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_BUILD_TYPE=Debug",
        ...(platform === "linux" ? ["-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined"] : [])],
    ]),
    ["cmake", "--build", buildDir, ...(targets.length > 0 ? ["--target", ...targets] : [])],
    ["ctest", "--test-dir", buildDir, "--output-on-failure", "-j", String(availableParallelism()),
      ...(mode === "all" ? []
        : mode === "only" ? ["-R", `^(${selection.names.join("|")})$`]
        : ["-L", `^${selection.tier || TEST_TIER_DEFAULT}$`]),
    ],
  ];
}

// Golden banks are addressed absolutely because CTest gives each test its own
// working directory; recording is opt-in so a normal run can never rewrite one.
export function goldenEnvironment(gameDir, updateGoldens, env = process.env) {
  const next = { ...env, GAME_GOLDENS_DIR: join(gameDir, "tests", "goldens") };
  if (updateGoldens) next.GAME_UPDATE_GOLDENS = "1";
  else delete next.GAME_UPDATE_GOLDENS;
  return next;
}

function runNativeTests(gameDir, options = {}) {
  const buildDir = join(gameDir, "build", "native-debug");
  const configured = existsSync(join(buildDir, "CMakeCache.txt"));
  const env = goldenEnvironment(gameDir, options.updateGoldens === true);
  mkdirSync(env.GAME_GOLDENS_DIR, { recursive: true });
  const selection = { ...options.selection };
  // An unconfigured tree has no CTest catalogue yet, so the first run builds
  // everything and filters by label; later runs build only what they will run.
  if (configured) Object.assign(selection, selectTests(ctestCatalogue(buildDir), selection));
  for (const [command, ...args] of nativeTestPlan(gameDir, process.platform, configured, selection)) {
    run(command, args, gameDir, "native game tests", env);
    // CMake regenerates during the build step, so the labels are only settled
    // once it has run.
    if (command === "cmake" && args[0] === "--build") assertTiersLabelled(ctestCatalogue(buildDir));
  }
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
  const studioRoot = findStudioRoot(gameDir);
  const repoRevision = gitRevision(studioRoot, "reference-template proof");
  const engineRevision = gitRevision(join(studioRoot, seed.engine?.source || ""), "reference-template engine proof");
  const dependencies = {
    schema: "ai_studio.game.dependencies.v3",
    engine: { ...seed.engine, revision: engineRevision },
    features: (seed.features || []).map((feature) => ({ ...feature })),
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

async function auditGameReleaseAssets({ gameDir, artifactDir: releaseArtifactDir, studioRoot, dependencies }) {
  const modulePath = join(studioRoot, "ai_studio", "assets", "manifests", "game_release.mjs");
  const { assertGameReleaseAssets } = await import(pathToFileURL(modulePath).href);
  const result = assertGameReleaseAssets(gameDir);
  const runtimeBuild = createRuntimeBuildRecord({ gameDir, studioRoot, dependencies });
  const assetPack = readFileSync(join(releaseArtifactDir, "assets", "game.ntpack"));
  return {
    schema: "ai_studio.game_release_asset_audit.v1",
    ok: true,
    packed: result.packed,
    runtimeFingerprint: runtimeBuild.fingerprint,
    assetPackSha256: createHash("sha256").update(assetPack).digest("hex"),
  };
}

export async function packageGame(options, dependencies = {}, metadata = null) {
  const gameDir = resolve(dependencies.gameDir || GAME_DIR);
  if (options.build) (dependencies.build || buildGame)(gameDir, options.target);
  const proof = metadata || gamePackageMetadata(gameDir, options.templateProof);
  const studioRoot = findStudioRoot(gameDir);
  const releaseArtifactDir = artifactDir(gameDir, options.target);
  const assetAuditProof = await (dependencies.assetAudit || auditGameReleaseAssets)({
    gameDir, artifactDir: releaseArtifactDir, studioRoot, dependencies: proof.dependencies,
  });
  return packageWebArtifact({
    gameDir,
    artifactDir: releaseArtifactDir,
    target: options.target,
    studioRoot,
    assetAuditProof,
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
  const verifyDependencies = dependencies.verifyDependencies || ((metadata, proofOptions) => verifyDependencySources({ studioRoot: findStudioRoot(gameDir), dependencies: metadata.dependencies, ...proofOptions }));
  const packageGameOwned = dependencies.package || ((options, metadata) => packageGame(
    options, { gameDir, assetAudit: dependencies.assetAudit }, metadata,
  ));
  const smokePackage = dependencies.smoke || smokePackagedWebArtifact;
  const prepare = (templateProof = false, proofOptions = undefined) => {
    doctor({ gameDir, templateProof });
    const metadata = loadMetadata(gameDir, templateProof);
    verifyDependencies(metadata, proofOptions);
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
    // Iteration gate: dependency shape/version checks stay hard, but a dirty
    // engine/feature tree only warns here — release lanes still require clean.
    prepare(false, { cleanliness: "warn" });
    nodeTest(gameDir);
    const only = args.only || [];
    const selection = args.all ? { mode: "all" }
      : only.length > 0 ? { mode: "only", names: only }
      : { mode: "tier", tier: args.tier || TEST_TIER_DEFAULT };
    nativeTest(gameDir, { selection, updateGoldens: args.updateGoldens });
    return { message: args.updateGoldens ? "goldens recorded" : "game tests passed" };
  }
  if (args.command === "playable") {
    prepare(false);
    if (args.build) (dependencies.build || buildGame)(gameDir, args.target);
    validateWebArtifact({ gameDir, artifactDir: artifactDir(gameDir, args.target), target: args.target, studioRoot: findStudioRoot(gameDir) });
    return { message: `playable proof passed (${args.target})` };
  }
  if (args.command === "package") {
    const metadata = prepare(args.templateProof);
    const result = await packageGameOwned(args, metadata);
    return { message: `package passed: ${result.zipPath}`, result };
  }
  const metadata = prepare(args.templateProof);
  if (!args.skipTests) {
    nodeTest(gameDir);
    nativeTest(gameDir, { selection: { mode: "all" } });
  }
  const result = await packageGameOwned(args, metadata);
  await smokePackage({ zipPath: result.zipPath, expectedTarget: args.target, mode: "full" });
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
