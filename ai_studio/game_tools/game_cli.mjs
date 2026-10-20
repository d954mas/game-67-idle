import { createHash } from "node:crypto";
import { closeSync, copyFileSync, existsSync, lstatSync, mkdirSync, mkdtempSync, openSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { availableParallelism, tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawn, spawnSync } from "node:child_process";

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

// One CLI for every game. A game keeps a thin tools/game.mjs that passes its
// directory and, when it needs them, its own extra commands.
const TOOLS_DIR = dirname(fileURLToPath(import.meta.url));
const TARGET_CONFIG = await import(pathToFileURL(join(
  TOOLS_DIR, "..", "..", "features", "platform-sdk", "publish-targets", "target_config.mjs",
)).href);
const PACKAGE_TARGETS = new Set(TARGET_CONFIG.portalTargetNames());
const BUILD_TARGETS = new Set(TARGET_CONFIG.targetNames());
const COMMANDS = new Set(["doctor", "build", "run", "test", "playable", "package", "portal-check", "verify", "clean"]);
// The tier vocabulary is shared with cmake/GameTests.cmake; CTest labels carry it.
export const TEST_TIERS = ["core", "slow", "taste"];
export const TEST_TIER_DEFAULT = "core";
const USAGE = "usage: node tools/game.mjs <doctor|build|run|test|playable|package|portal-check|verify|clean>[--target local|itch|poki|yandex|playgama|crazygames|wavedash|pikabu|gamepush] [--no-build] [--out <dir>] [--template-proof] [--skip-tests] [test: --tier core|slow|taste | --all | --only <test>] [--update-goldens] [test|verify: --verbose] [clean: --all]";

function readJson(path, label) {
  let value;
  try { value = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, "")); }
  catch (error) { throw new Error(`${label} is not valid JSON: ${error.message}`); }
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value;
}

// tiers: the game's own CTest tiers beyond the shared ones.
export function parseGameArgs(argv, { tiers = [] } = {}) {
  const command = argv[0] || "";
  if (!COMMANDS.has(command)) throw new Error(USAGE);
  const args = {
    command,
    target: ["playable", "package", "verify"].includes(command) ? "itch" : "local",
    targetSpecified: false,
    build: true,
    templateProof: false,
    skipTests: false,
    outDir: "",
    only: [],
    tier: "",
    all: false,
    updateGoldens: false,
    verbose: false,
  };
  for (let index = 1; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--target") { args.target = argv[++index] || ""; args.targetSpecified = true; }
    else if (arg === "--no-build") args.build = false;
    else if (arg === "--template-proof") args.templateProof = true;
    else if (arg === "--skip-tests") args.skipTests = true;
    else if (arg === "--out") args.outDir = argv[++index] || "";
    else if (arg === "--only") args.only.push(argv[++index] || "");
    else if (arg === "--tier") args.tier = argv[++index] || "";
    else if (arg === "--all") args.all = true;
    else if (arg === "--update-goldens") args.updateGoldens = true;
    else if (arg === "--verbose") args.verbose = true;
    else throw new Error(`unknown option: ${arg}\n${USAGE}`);
  }
  if (args.only.some((name) => !/^[A-Za-z0-9_.-]+$/.test(name))) throw new Error(`--only takes CTest names\n${USAGE}`);
  if ((args.only.length > 0 || args.updateGoldens || args.tier) && command !== "test") {
    throw new Error(`--only, --tier and --update-goldens are valid only for test\n${USAGE}`);
  }
  if (args.all && !["test", "clean"].includes(command)) throw new Error(`--all is valid only for test and clean\n${USAGE}`);
  if (args.tier && !TEST_TIERS.includes(args.tier) && !tiers.includes(args.tier)) throw new Error(`unknown test tier: ${args.tier}\n${USAGE}`);
  if ((args.tier ? 1 : 0) + (args.all ? 1 : 0) + (args.only.length > 0 ? 1 : 0) > 1) {
    throw new Error(`--tier, --all and --only cannot be combined\n${USAGE}`);
  }
  if (!BUILD_TARGETS.has(args.target) || (["package", "verify"].includes(command) && !PACKAGE_TARGETS.has(args.target))) {
    throw new Error(`unknown target for ${command}: ${args.target}`);
  }
  if (command === "portal-check" && !args.targetSpecified) throw new Error("portal-check requires --target <portal>");
  if (command === "portal-check" && !PACKAGE_TARGETS.has(args.target)) throw new Error("portal-check requires a portal target");
  if (!args.build && !["playable", "package", "verify"].includes(command)) throw new Error(`--no-build is not valid for ${command}`);
  if (args.templateProof && !["doctor", "test", "package", "verify"].includes(command)) throw new Error(`--template-proof is not valid for ${command}`);
  if (args.skipTests && command !== "verify") throw new Error(`--skip-tests is not valid for ${command}`);
  if (args.skipTests && !args.templateProof) throw new Error("--skip-tests is valid only with --template-proof");
  if (args.outDir && !["package", "verify"].includes(command)) throw new Error(`--out is not valid for ${command}`);
  if (args.verbose && !["test", "verify"].includes(command)) throw new Error(`--verbose is not valid for ${command}`);
  delete args.targetSpecified;
  return args;
}

function requiredScaffold(gameDir, ciWorkflow = true) {
  return [
    "CMakeLists.txt",
    "tools/game.mjs",
    "tools/build_web.mjs",
    "tools/lib/studio_root.mjs",
    "release/README.md",
    ...(ciWorkflow ? [".github/workflows/game-verify.yml"] : []),
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

// ciWorkflow: false for a game that keeps verification local and ships no CI.
export function doctorGame({ gameDir, templateProof = false, ciWorkflow = true } = {}) {
  const root = resolve(gameDir);
  const missing = requiredScaffold(root, ciWorkflow).filter((path) => !existsSync(path));
  if (missing.length) throw new Error(`game scaffold is missing: ${missing.map((path) => path.slice(root.length + 1)).join(", ")}`);
  if (templateProof) {
    const template = validateTemplateIdentity(readJson(join(root, "template.json"), "template identity"));
    const seed = readJson(join(root, "game-dependencies.json"), "template dependency seed");
    if (template.schema !== "ai_studio.template.v1" || seed.schema !== "ai_studio.game.dependencies.seed.v3") throw new Error("reference-template proof metadata is invalid");
    return { gameId: template.id, kind: "reference-template" };
  }
  const identity = validateGameIdentity(readJson(join(root, "game.json"), "game identity"));
  validateDependencies(readJson(join(root, "dependencies.json"), "dependencies"));
  return { gameId: identity.id, kind: "game" };
}

// With a capture, a step's output goes to the capture instead of the terminal:
// a green test run is read back by agents on every later turn, so it must stay
// one line, while a red one still shows what failed.
function run(command, args, cwd, label, env = process.env, capture = null) {
  if (!capture) {
    const result = spawnSync(command, args, { cwd, shell: false, stdio: "inherit", env });
    if (result.error) throw result.error;
    if (result.status !== 0) throw new Error(`${label} exited ${result.status ?? 1}`);
    return;
  }
  const result = spawnSync(command, args, {
    cwd, shell: false, env, encoding: "utf8", stdio: ["ignore", "pipe", "pipe"], maxBuffer: 512 * 1024 * 1024,
  });
  if (result.error) throw result.error;
  const output = `${result.stdout || ""}${result.stderr || ""}`;
  capture.steps.push({ label, command: [command, ...args].join(" "), output });
  if (result.status !== 0) {
    const error = new Error(`${label} exited ${result.status ?? 1}`);
    error.output = output;
    throw error;
  }
}

// Runs beside the native build and CTest, which never touch what it reads. Its
// output goes to a file, not a pipe: the native steps block this process's event
// loop, and a pipe nobody drains stalls the child once it fills.
function runNodeTests(gameDir, capture = null) {
  const tools = join(gameDir, "tools");
  const files = readdirSync(tools).filter((name) => name.endsWith(".test.mjs")).sort().map((name) => join("tools", name));
  if (files.length === 0) throw new Error("game scaffold has no Node tests");
  // Concurrency mirrors the studio gate; every heavy fixture is mkdtemp-isolated.
  const args = ["--test", "--test-concurrency=4", ...files];
  const logDir = mkdtempSync(join(tmpdir(), "game-node-tests-"));
  const logPath = join(logDir, "node-tests.log");
  const log = openSync(logPath, "w");
  const child = spawn(process.execPath, args, { cwd: gameDir, shell: false, stdio: ["ignore", log, log] });
  closeSync(log);
  return new Promise((resolveRun, rejectRun) => {
    child.on("error", rejectRun);
    child.on("close", (code) => {
      const output = readFileSync(logPath, "utf8");
      rmSync(logDir, { recursive: true, force: true });
      if (capture) capture.steps.push({ label: "game tests", command: [process.execPath, ...args].join(" "), output });
      else process.stdout.write(output);
      if (code === 0) return resolveRun();
      const error = new Error(`game tests exited ${code ?? 1}`);
      error.output = output;
      rejectRun(error);
    });
  });
}

export function summarizeTestCapture(capture) {
  const text = capture.steps.map((step) => step.output).join("\n");
  const nodePassed = [...text.matchAll(/^(?:ℹ|#) pass (\d+)$/gm)].reduce((sum, match) => sum + Number(match[1]), 0);
  const native = [...text.matchAll(/(\d+)% tests passed, (\d+) tests? failed out of (\d+)/g)].at(-1);
  return { nodePassed, nativePassed: native ? Number(native[3]) - Number(native[2]) : 0 };
}

function writeTestLog(gameDir, capture) {
  const path = join(gameDir, "build", "logs", "game-test.log");
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, capture.steps.map((step) => `$ ${step.command}\n${step.output}`).join("\n"));
  return path;
}

function outputTail(output, lines = 80) {
  return String(output || "").trimEnd().split(/\r?\n/).slice(-lines).join("\n");
}

// Build variants per portal and preset pile up; native-debug is the iteration
// tree every test run and capture reuses, so it survives unless --all.
export function cleanBuild(gameDir, { all = false, remove = (path) => rmSync(path, { recursive: true, force: true }) } = {}) {
  const buildDir = join(gameDir, "build");
  if (!existsSync(buildDir)) return [];
  const removed = [];
  for (const entry of readdirSync(buildDir, { withFileTypes: true })) {
    if (!all && entry.name === "native-debug") continue;
    const path = join(buildDir, entry.name);
    const bytes = treeBytes(path);
    remove(path);
    removed.push({ name: entry.name, bytes });
  }
  return removed;
}

function treeBytes(path) {
  let bytes = 0;
  const stack = [path];
  while (stack.length) {
    const current = stack.pop();
    let info;
    try { info = lstatSync(current); } catch { continue; }
    if (info.isDirectory()) for (const name of readdirSync(current)) stack.push(join(current, name));
    else bytes += info.size;
  }
  return bytes;
}

// Runs the game's Node and native suites; quiet unless verbose.
// The Node suite starts first and runs while the native build and CTest do; a
// native failure is reported ahead of a Node one.
export async function runTestSuites(gameDir, { nodeTest, nativeTest, selection, updateGoldens = false, includeNode = true, verbose = false }) {
  const capture = verbose ? null : { steps: [] };
  const started = Date.now();
  try {
    let nodeRun = null;
    if (includeNode) nodeRun = Promise.resolve(nodeTest(gameDir, capture));
    let nativeError = null;
    try { nativeTest(gameDir, { selection, updateGoldens, capture }); } catch (error) { nativeError = error; }
    let nodeError = null;
    try { await nodeRun; } catch (error) { nodeError = error; }
    if (nativeError && nodeError) {
      nativeError.message = `${nativeError.message}; also ${nodeError.message}`;
      nativeError.output = [nodeError.output, nativeError.output].filter(Boolean).join("\n");
    }
    if (nativeError || nodeError) throw nativeError || nodeError;
  } catch (error) {
    if (capture) {
      const log = writeTestLog(gameDir, capture);
      error.message = `${error.message}\n${outputTail(error.output)}\nfull log: ${log}`;
    }
    throw error;
  }
  if (!capture) return "";
  const log = writeTestLog(gameDir, capture);
  const { nodePassed, nativePassed } = summarizeTestCapture(capture);
  const seconds = ((Date.now() - started) / 1000).toFixed(1);
  return `node ${nodePassed} passed, native ${nativePassed} passed in ${seconds}s; log: ${log}`;
}

const isDirectory = (path) => statSync(path, { throwIfNoEntry: false })?.isDirectory() ?? false;

// CTest labels are the only record of which tier a test belongs to, so the
// runner reads them back instead of keeping a second list that drifts.
export function ctestCatalogue(buildDir, spawn = spawnSync) {
  const result = spawn("ctest", ["--test-dir", buildDir, "--show-only=json-v1"], {
    encoding: "utf8", shell: false, stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`CTest discovery exited ${result.status ?? 1}`);
  const buildRoot = `${buildDir.replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase()}/`;
  const catalogue = JSON.parse(result.stdout).tests.map((entry) => {
    const property = (name) => [].concat((entry.properties || []).find((item) => item.name === name)?.value ?? []);
    const labels = property("LABELS");
    const argv = Array.isArray(entry.command) ? entry.command : [];
    // Every executable a test runs out of the build tree is a target it needs
    // built: its own, a tool it drives under another name, or the binary a Node
    // or Python contract is handed in argv or ENVIRONMENT. The interpreter itself
    // is not, and neither is a pack, data file or existing directory read out of
    // the tree (a binary dir exists from configure on; an executable may not yet).
    const environment = property("ENVIRONMENT").map(String).filter((pair) => pair.includes("="))
      .map((pair) => pair.slice(pair.indexOf("=") + 1));
    const targets = [...argv, ...environment]
      .map((arg) => String(arg).replace(/\\/g, "/"))
      .filter((arg) => arg.toLowerCase().startsWith(buildRoot) && !isDirectory(arg))
      .map((arg) => arg.split("/").pop() || "")
      .filter((name) => /\.exe$/i.test(name) || (name !== "" && !name.includes(".")))
      .map((name) => name.replace(/\.exe$/i, ""));
    // CTest omits the command while the executable is still unbuilt, and a
    // checkout reached through a link can report a path outside the tree it was
    // asked about: either way a test named after its own executable still names
    // that target.
    const own = (String(argv[0] || "").replace(/\\/g, "/").split("/").pop() || "").replace(/\.exe$/i, "");
    if (argv.length === 0 || own === entry.name) targets.push(entry.name);
    return {
      name: entry.name,
      tier: String(labels[0] || ""),
      targets: [...new Set(targets)],
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
// gameTarget: the game's playable CMake target, when it is not named "game".
export function selectTests(catalogue, selection = {}, { gameTarget = "game" } = {}) {
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
    targets: mode === "all" ? [] : [...new Set([gameTarget, ...selected.flatMap((entry) => entry.targets)])],
  };
}

export function ctestParallelism(environment = process.env) {
  const raw = environment.CTEST_PARALLEL_LEVEL;
  if (raw === undefined || raw === "") return availableParallelism();
  const jobs = Number(raw);
  if (!Number.isSafeInteger(jobs) || jobs < 0) throw new Error("CTEST_PARALLEL_LEVEL must be a non-negative integer");
  return jobs;
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
    ["ctest", "--test-dir", buildDir, "--output-on-failure", "-j", String(ctestParallelism()),
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

// dependencies.settle: a game that owns how its native-debug tree is configured
// gets it ready itself, through the runner it is handed; the tree is then configured.
export function runNativeTests(gameDir, options = {}, dependencies = {}) {
  const runCommand = dependencies.run || run;
  const catalogue = dependencies.catalogue || ctestCatalogue;
  const buildDir = join(gameDir, "build", "native-debug");
  const env = goldenEnvironment(gameDir, options.updateGoldens === true);
  mkdirSync(env.GAME_GOLDENS_DIR, { recursive: true });
  const selection = { ...options.selection };
  let configured = true;
  if (dependencies.settle) {
    dependencies.settle((command, args, label) => runCommand(command, args, gameDir, label, env, options.capture), env);
  } else {
    configured = existsSync(join(buildDir, "CMakeCache.txt")) && existsSync(join(buildDir, "build.ninja"));
    // Ninja regenerates the build graph before discovery, so a new test is visible
    // without paying for a second configure process on every iteration.
    if (configured) runCommand("cmake", ["--build", buildDir, "--target", "build.ninja"], gameDir, "native test graph refresh", env, options.capture);
  }
  if (configured) Object.assign(selection, selectTests(catalogue(buildDir), selection, { gameTarget: dependencies.gameTarget }));
  for (const [command, ...args] of nativeTestPlan(gameDir, process.platform, configured, selection)) {
    runCommand(command, args, gameDir, "native game tests", env, options.capture);
    // CMake regenerates during the build step, so the labels are only settled
    // once it has run.
    if (command === "cmake" && args[0] === "--build") assertTiersLabelled(catalogue(buildDir));
  }
}

function buildGame(gameDir, target) {
  const code = buildWeb(["--preset", "wasm-release", "--target", target, "--no-debug-ui"], process.env, { gameDir });
  if (code !== 0) throw new Error(`web build exited ${code}`);
}

function artifactDir(gameDir, target) {
  return join(gameDir, "build", target === "local" ? "wasm-release" : `wasm-release-${target}`, "bin");
}

/* A game that calls a remote service records the endpoints CMake compiled in
   beside the build; the smoke admits only those, failed as if offline. No record
   means no remote endpoint. */
export function buildRemoteEndpoints(gameDir, target, { read = readFileSync, exists = existsSync, artifactDir: artifactDirFor = artifactDir } = {}) {
  const path = join(dirname(artifactDirFor(gameDir, target)), "remote_endpoints.json");
  if (!exists(path)) return [];
  let record;
  try { record = JSON.parse(read(path, "utf8")); }
  catch (error) { throw new Error(`remote endpoint record is unreadable (reconfigure the build): ${path}: ${error.message}`); }
  const urls = record?.allowedRemoteUrls;
  if (!Array.isArray(urls) || !urls.every((url) => typeof url === "string")) throw new Error(`remote endpoint record is malformed: ${path}`);
  return urls;
}

/* A release crash reports raw wasm offsets, which mean nothing without the map
   from that exact link, so the map is kept beside the ZIP. Only a build this
   command ran owns the map; --no-build packages whatever it was handed. */
export function archiveSymbolMap(releaseArtifactDir, zipPath, { exists = existsSync, copy = copyFileSync } = {}) {
  const source = join(dirname(releaseArtifactDir), "game.js.symbols");
  if (!exists(source)) return "";
  const target = zipPath.replace(/\.zip$/, ".symbols");
  copy(source, target);
  return target;
}

/* One verdict for a portal submission. The three checks belong to three owners
   -- the feature owns the artifact contract and the SDK probe, the studio owns
   the store spec -- and a release is only ready when all three agree, so they
   are asked together rather than remembered separately. */
// artifactDir names where a game with its own web layout keeps its artifact.
// gameArtifact { args(gameDir, target), replacesFeatureInspect? } adds the game's
// own artifact check after the feature's inspect; it may replace the inspect
// only by saying why, and the plan carries that as a note the run prints.
export function portalCheckPlan(studioRoot, gameDir, target, { exists = existsSync, artifactDir: artifactDirFor = artifactDir, gameArtifact = null } = {}) {
  if (!PACKAGE_TARGETS.has(target)) throw new Error("portal-check requires a portal target");
  const steps = [];
  const replaces = String(gameArtifact?.replacesFeatureInspect || "").trim();
  if (gameArtifact?.replacesFeatureInspect !== undefined && !replaces) throw new Error("a game artifact check that replaces the feature inspect must say why");
  if (!replaces) {
    steps.push({
      id: "artifact",
      args: [join(studioRoot, "features", "platform-sdk", "scripts", "artifact_tools.mjs"),
        "inspect", "--target", target, "--artifact", artifactDirFor(gameDir, target)],
    });
  }
  if (gameArtifact) {
    steps.push({
      id: replaces ? "artifact" : "game-artifact",
      args: gameArtifact.args(gameDir, target),
      ...(replaces ? { note: `the game's artifact check replaces features/platform-sdk artifact_tools inspect: ${replaces}` } : {}),
    });
  }

  const spec = join(studioRoot, "ai_studio", "store_kit", `${target}_spec.json`);
  const storeDir = join(gameDir, "release", "store", target);
  if (exists(spec)) {
    steps.push({
      id: "store",
      args: [join(studioRoot, "ai_studio", "store_kit", "check.mjs"),
        "--portal", target, "--dir", storeDir],
      skip: exists(storeDir) ? null : `no store draft at ${storeDir}`,
    });
  }

  /* Only Yandex ships a dev server a game can be driven inside. The other
     portals prove their SDK in a console nobody can reach from here, so the
     row is absent rather than silently passing. */
  const probe = join(studioRoot, "features", "platform-sdk", "scripts", `${target}_sdk_probe.mjs`);
  if (exists(probe)) {
    steps.push({
      id: "sdk",
      args: [probe, "--artifact", artifactDirFor(gameDir, target)],
      skip: exists(artifactDirFor(gameDir, target)) ? null : "no built artifact to drive",
    });
  }
  return steps;
}

function portalCheck(gameDir, target, layout = {}) {
  const studioRoot = findStudioRoot(gameDir);
  const rows = [];
  for (const step of portalCheckPlan(studioRoot, gameDir, target, layout)) {
    if (step.note) console.log(`NOTE  ${step.id}: ${step.note}`);
    if (step.skip) {
      rows.push({ id: step.id, ok: false, detail: step.skip });
      continue;
    }
    const result = spawnSync(process.execPath, step.args, { cwd: gameDir, encoding: "utf8", shell: false });
    const output = `${result.stdout || ""}${result.stderr || ""}`.trim();
    rows.push({ id: step.id, ok: result.status === 0, detail: output });
  }
  for (const row of rows) {
    console.log(`${row.ok ? "PASS" : "FAIL"}  ${row.id}`);
    if (row.detail) console.log(row.detail.split("\n").map((line) => `    ${line}`).join("\n"));
  }
  if (rows.some((row) => !row.ok)) throw new Error(`portal-check failed for ${target}`);
  return rows;
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
    engine: { ...seed.engine, version: engineRevision.slice(0, 8), revision: engineRevision },
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

async function auditGameReleaseAssets({ gameDir, artifactDir: releaseArtifactDir, studioRoot, dependencies, target }) {
  const modulePath = join(studioRoot, "ai_studio", "assets", "manifests", "game_release.mjs");
  const { assertGameReleaseAssets } = await import(pathToFileURL(modulePath).href);
  const result = assertGameReleaseAssets(gameDir);
  const runtimeBuild = createRuntimeBuildRecord({
    gameDir, studioRoot, dependencies, compileProfile: TARGET_CONFIG.compileProfileForTarget(target, "wasm-release"),
  });
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
  const gameDir = resolve(dependencies.gameDir);
  if (options.build) (dependencies.build || buildGame)(gameDir, options.target);
  const proof = metadata || gamePackageMetadata(gameDir, options.templateProof);
  const studioRoot = findStudioRoot(gameDir);
  const releaseArtifactDir = artifactDir(gameDir, options.target);
  const assetAuditProof = await (dependencies.assetAudit || auditGameReleaseAssets)({
    gameDir, artifactDir: releaseArtifactDir, studioRoot, dependencies: proof.dependencies, target: options.target,
  });
  const packaged = await packageWebArtifact({
    gameDir,
    artifactDir: releaseArtifactDir,
    target: options.target,
    studioRoot,
    assetAuditProof,
    ...(options.outDir ? { outDir: resolve(gameDir, options.outDir) } : {}),
    ...proof,
    ...(metadata ? { dependencyVerifier: () => {} } : {}),
  });
  if (options.build) archiveSymbolMap(releaseArtifactDir, packaged.zipPath);
  return packaged;
}

export async function executeGameCommand(args, dependencies = {}) {
  if (!dependencies.gameDir) throw new Error("the game CLI needs the game directory");
  const gameDir = resolve(dependencies.gameDir);
  const doctor = dependencies.doctor || doctorGame;
  const nodeTest = dependencies.nodeTest || runNodeTests;
  const nativeTest = dependencies.nativeTest || runNativeTests;
  const loadMetadata = dependencies.loadMetadata || gamePackageMetadata;
  const verifyDependencies = dependencies.verifyDependencies || ((metadata, proofOptions) => verifyDependencySources({ studioRoot: findStudioRoot(gameDir), dependencies: metadata.dependencies, ...proofOptions }));
  const packageGameOwned = dependencies.package || ((options, metadata) => packageGame(
    options, { gameDir, assetAudit: dependencies.assetAudit }, metadata,
  ));
  const smokePackage = dependencies.smoke || smokePackagedWebArtifact;
  const remoteEndpoints = dependencies.remoteEndpoints || buildRemoteEndpoints;
  // A game whose web build has its own layout names its artifact directory and validator.
  const artifactDirFor = dependencies.artifactDir || artifactDir;
  const validateArtifact = dependencies.validateArtifact || validateWebArtifact;
  const prepare = (templateProof = false, proofOptions = undefined) => {
    doctor({ gameDir, templateProof });
    const metadata = loadMetadata(gameDir, templateProof);
    verifyDependencies(metadata, proofOptions);
    return metadata;
  };
  if (args.command === "clean") {
    const removed = (dependencies.clean || cleanBuild)(gameDir, { all: args.all });
    const megabytes = (bytes) => (bytes / 1024 / 1024).toFixed(0);
    for (const row of removed) console.log(`removed ${megabytes(row.bytes).padStart(6)} MB  build/${row.name}`);
    const freed = removed.reduce((sum, row) => sum + row.bytes, 0);
    return { message: `clean freed ${megabytes(freed)} MB${args.all ? "" : "; kept build/native-debug"}` };
  }
  if (args.command === "doctor") {
    const result = doctor({ gameDir, templateProof: args.templateProof });
    verifyDependencies(loadMetadata(gameDir, args.templateProof));
    return { message: `doctor passed (${result.gameId})` };
  }
  if (args.command === "build") {
    prepare(false);
    (dependencies.build || buildGame)(gameDir, args.target);
    return { message: `built wasm-release-${args.target}` };
  }
  if (args.command === "run") {
    run(process.execPath, [join(TOOLS_DIR, "serve_web.mjs"), "--preset", "wasm-release", "--target", args.target], gameDir, "game server");
    return { message: "game server stopped" };
  }
  if (args.command === "test") {
    // Iteration gate: dependency shape/version checks stay hard, but a dirty
    // engine/feature tree only warns here — release lanes still require clean.
    prepare(args.templateProof, { cleanliness: "warn" });
    const only = args.only || [];
    const selection = args.all ? { mode: "all" }
      : only.length > 0 ? { mode: "only", names: only }
      : { mode: "tier", tier: args.tier || TEST_TIER_DEFAULT };
    // The Node suite is core-tier logic: a focused or a wider-tier run leaves it out.
    const includeNode = selection.mode === "all" || (selection.mode === "tier" && selection.tier === TEST_TIER_DEFAULT);
    const summary = await runTestSuites(gameDir, {
      nodeTest, nativeTest, selection, updateGoldens: args.updateGoldens, includeNode, verbose: args.verbose,
    });
    const verdict = args.updateGoldens ? "goldens recorded" : "game tests passed";
    return { message: summary ? `${verdict}: ${summary}` : verdict };
  }
  if (args.command === "playable") {
    prepare(false);
    if (args.build) (dependencies.build || buildGame)(gameDir, args.target);
    validateArtifact({ gameDir, artifactDir: artifactDirFor(gameDir, args.target), target: args.target, studioRoot: findStudioRoot(gameDir) });
    return { message: `playable proof passed (${args.target}${dependencies.validateArtifact ? ", game-owned artifact validator" : ""})` };
  }
  if (args.command === "portal-check") {
    const rows = portalCheck(gameDir, args.target, { artifactDir: artifactDirFor, gameArtifact: dependencies.portalArtifact || null });
    return { message: `portal-check passed for ${args.target} (${rows.map((row) => row.id).join(", ")})` };
  }
  if (args.command === "package") {
    const metadata = prepare(args.templateProof);
    const result = await packageGameOwned(args, metadata);
    return { message: `package passed: ${result.zipPath}`, result };
  }
  const metadata = prepare(args.templateProof);
  const summary = args.skipTests ? "" : await runTestSuites(gameDir, { nodeTest, nativeTest, selection: { mode: "all" }, verbose: args.verbose });
  const result = await packageGameOwned(args, metadata);
  await smokePackage({
    zipPath: result.zipPath, expectedTarget: args.target, mode: "full", frameExpectation: args.templateProof ? "blank" : "rendered",
    allowedRemoteUrls: remoteEndpoints(gameDir, args.target, { artifactDir: artifactDirFor }),
  });
  return { message: `verify passed: ${result.zipPath}${summary ? `; tests: ${summary}` : ""}`, result };
}

// commands: the game's own extra commands, { name: async (argv, { gameDir }) => message }.
// They are tried before the shared ones, so a game can also replace one.
export async function main(argv = process.argv.slice(2), dependencies = {}) {
  try {
    const custom = dependencies.commands?.[argv[0]];
    if (custom) {
      console.log(await custom(argv.slice(1), { gameDir: resolve(dependencies.gameDir) }));
      return 0;
    }
    const args = parseGameArgs(argv, { tiers: dependencies.testTiers });
    const result = await executeGameCommand(args, dependencies);
    console.log(result.message);
    return 0;
  } catch (error) {
    const extra = Object.keys(dependencies.commands || {});
    const message = String(error?.message || error);
    console.error(message.startsWith("usage:") && extra.length ? `${message}
game commands: ${extra.join(", ")}` : message);
    return message.startsWith("usage:") || message.startsWith("unknown option") ? 2 : 1;
  }
}
