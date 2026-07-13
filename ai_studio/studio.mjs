#!/usr/bin/env node

import { existsSync, readdirSync, statSync } from "node:fs";
import { resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawn, spawnSync } from "node:child_process";

const ROOT = resolve(fileURLToPath(new URL("..", import.meta.url)));
const slash = (value) => String(value || "").replaceAll("\\", "/");
const PYTHON_RUN = ["node", "ai_studio/dev_environment/python_run.mjs"];
const pythonCommands = (files) => files.map((file) => [...PYTHON_RUN, file]);

const CANVAS_PYTHON_TESTS = ["ai_studio/assets/canvas/tools/ck_pixel_ops_test.py"];
const ASSET_PYTHON_TESTS = [
  "ai_studio/assets/tools/crop/plan_prepared_crops_from_intake_test.py",
  "ai_studio/assets/tools/image/alpha_dualplate/dual_plate_alpha_test.py",
  "ai_studio/assets/tools/image/alpha_dualplate/dual_plate_pair_gate_test.py",
  "ai_studio/assets/tools/image/alpha_dualplate/pair_align_test.py",
  "ai_studio/assets/tools/image/alpha_matte/chroma_key_alpha_test.py",
  "ai_studio/assets/tools/image/alpha_matte/key_matte_test.py",
  "ai_studio/assets/tools/image/bg_fix/normalize_background_test.py",
  "ai_studio/assets/tools/image/birefnet_cutout/birefnet_cutout_test.py",
  "ai_studio/assets/tools/image/denoise/denoise_test.py",
  "ai_studio/assets/tools/image/quantize/quantize_test.py",
  "ai_studio/assets/tools/image/regions/detect_regions_test.py",
  "ai_studio/assets/tools/image/route/route_cutout_test.py",
  "ai_studio/assets/tools/image/slice/slice_regions_test.py",
  "ai_studio/assets/tools/image/vitmatte_matte/matte_math_test.py",
  "ai_studio/assets/tools/lib/atomic_io_test.py",
  "ai_studio/assets/tools/lib/color_test.py",
  "ai_studio/assets/tools/review_atlas/atlas_review_labels_test.py",
  "ai_studio/assets/tools/review_atlas/audit_review_atlas_test.py",
  "ai_studio/assets/tools/review_atlas/build_review_atlas_test.py",
  "ai_studio/assets/tools/source_sheets/tests/audit_intake.test.py",
  "ai_studio/assets/tools/source_sheets/tests/normalize_chroma.test.py",
  "ai_studio/assets/tools/textures/tests/audit_tileable_texture.test.py",
  "ai_studio/assets/tools/video/sheet/pack_sheet_test.py",
];
const RUNTIME_PYTHON_TESTS = [
  "ai_studio/runtime_automation/devapi_client_test.py",
  "ai_studio/runtime_automation/iterate_test.py",
  "ai_studio/runtime_automation/png_io_test.py",
  "ai_studio/runtime_automation/state_capture_test.py",
];
const RUNTIME_PYTHON_COMMANDS = pythonCommands(RUNTIME_PYTHON_TESTS);
const RUNTIME_LINUX_ITERATE = [
  "xvfb-run", "-a", "node", "ai_studio/dev_environment/python_run.mjs",
  "ai_studio/runtime_automation/iterate.py", "--no-capture", "--json",
];
const GAME_STATE_PYTHON_TESTS = [
  "features/game-state/scripts/generate_state_test.py",
  "features/game-state/scripts/state_modules_test.py",
  "features/game-state/benchmarks/benchmark_codegen_test.py",
];
const ITEMS_PYTHON_TESTS = [
  "features/items-core/scripts/items_ops_test.py",
  "features/items-core/scripts/generate_items_api_proof_test.py",
];
const PROGRESSION_PYTHON_TESTS = ["features/progression-core/scripts/generate_progression_tracks_test.py"];
const TEMPLATE_PYTHON_TESTS = [
  "templates/template/devapi/responsive_viewports_test.py",
  "templates/template/devapi/smoke_bot_test.py",
];
const SKILL_PYTHON_TESTS = [".codex/skills/nt-asset-image-generation/scripts/test_expand_jobs.py"];

const ROUTES = Object.freeze({
  game: {
    owner: "game-owned lifecycle",
    availability: "game-owned",
    commands: [
      { id: "create", availability: "shared", owner: "games/new_game.mjs", route: ["node", "games/new_game.mjs"] },
      { id: "doctor", availability: "game-owned", owner: "<game-root>", route: null, note: "Each game owns its doctor command; Studio does not synthesize one from CMake." },
      { id: "build", availability: "game-owned", owner: "<game-root>/CMakeLists.txt", route: ["cmake", "--build", "<game-build>"] },
      { id: "run", availability: "game-owned", owner: "<game-root>", route: ["<game-build>/bin/game"] },
      { id: "test", availability: "game-owned", owner: "<game-root>/CMakeLists.txt", route: ["ctest", "--test-dir", "<game-build>"] },
      { id: "package", availability: "game-owned", owner: "<game-root>", route: null, note: "Each game owns its packaging command; build_web.mjs only builds a web artifact directory." },
    ],
  },
  canvas: { owner: "ai_studio/assets/canvas", surface: "/canvas/", cli: ["node", "ai_studio/assets/canvas/cli.mjs"] },
  taskboard: { owner: "ai_studio/taskboard", surface: "/taskboard/", cli: ["node", "ai_studio/taskboard/cli.mjs"] },
  assets: {
    owner: "ai_studio/assets",
    surface: "/asset_viewer/",
    tools: {
      search: ["node", "ai_studio/assets/backlog/storage/search.mjs"],
      intake: ["node", "ai_studio/assets/backlog/storage/intake/stage.mjs"],
      canvas: ["node", "ai_studio/assets/canvas/cli.mjs"],
    },
  },
});

export function nativeConfigureCommand(platform = process.platform) {
  return [
    "cmake", "-S", "templates/template", "-B", "templates/template/build/native-debug", "-G", "Ninja",
    "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_BUILD_TYPE=Debug",
    ...(platform === "linux" ? ["-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined"] : []),
  ];
}

const NATIVE_COMMANDS = [
  nativeConfigureCommand(),
  ["cmake", "--build", "templates/template/build/native-debug"],
  ["ctest", "--test-dir", "templates/template/build/native-debug", "--output-on-failure"],
];
const WEB_COMMANDS = [
  ["node", "templates/template/tools/build_web.mjs", "--preset", "wasm-release", "--target", "local", "--no-debug-ui"],
  ["node", "-e", "const fs=require('node:fs'); for (const p of ['templates/template/build/wasm-release/bin/game.js','templates/template/build/wasm-release/bin/game.wasm','templates/template/build/wasm-release/bin/assets/game.ntpack']) if (!fs.existsSync(p)) throw new Error('missing artifact '+p)"],
];

const NATIVE_TEST_FILES = Object.freeze([
  "features/audio-core/tests/test_audio.c",
  "features/audio-core/tests/test_audio_backend_native.c",
  "features/audio-core/tests/test_audio_resource.c",
  "templates/template/tests/test_game_analytics.c",
  "templates/template/tests/test_game_audio.c",
  "templates/template/tests/test_game_event_render.c",
  "templates/template/tests/test_game_events.c",
  "templates/template/tests/test_game_events_log_mirror.c",
  "templates/template/tests/test_game_events_typed.c",
  "templates/template/tests/test_game_format.c",
  "templates/template/tests/test_game_save.c",
  "templates/template/tests/test_game_state_json.c",
  "templates/template/tests/test_game_state_roundtrip.c",
  "templates/template/tests/test_game_storage.c",
  "templates/template/tests/test_items_api.c",
  "templates/template/tests/test_items_api_core_only.c",
  "templates/template/tests/test_items_catalog.c",
  "templates/template/tests/test_items_fragment.c",
  "templates/template/tests/test_platform_lifecycle.c",
  "templates/template/tests/test_platform_sdk.c",
  "templates/template/tests/test_platform_sdk_events.c",
  "templates/template/tests/test_progression.c",
  "templates/template/tests/test_progression_catalog.c",
  "templates/template/tests/test_progression_curve.c",
  "templates/template/tests/test_template_composition.c",
]);

const SUITES = Object.freeze([
  { id: "studio.facade", testFiles: ["ai_studio/studio.test.mjs", "ai_studio/studio_ci.test.mjs"] },
  { id: "reference-template.native", commands: NATIVE_COMMANDS, nativeFiles: NATIVE_TEST_FILES },
  { id: "reference-template.web", commands: WEB_COMMANDS, requiresEmsdk: true },
  { id: "studio.architecture-map", testRoots: ["ai_studio/architecture_map"] },
  { id: "studio.assets.canvas", testRoots: ["ai_studio/assets/canvas"] },
  { id: "studio.assets.canvas-python", pythonFiles: CANVAS_PYTHON_TESTS, commands: pythonCommands(CANVAS_PYTHON_TESTS) },
  { id: "studio.assets", testRoots: ["ai_studio/assets/backlog", "ai_studio/assets/gallery", "ai_studio/assets/items_viewer", "ai_studio/assets/tools"] },
  { id: "studio.assets.python", pythonFiles: ASSET_PYTHON_TESTS, commands: pythonCommands(ASSET_PYTHON_TESTS) },
  { id: "studio.core-harness", testRoots: ["ai_studio/core_harness"] },
  { id: "studio.skills.python", pythonFiles: SKILL_PYTHON_TESTS, commands: pythonCommands(SKILL_PYTHON_TESTS) },
  { id: "studio.dev-environment", testRoots: ["ai_studio/dev_environment"] },
  { id: "studio.game-design", commands: [["node", "ai_studio/core_harness/validation/doc_reference_check.mjs"]] },
  { id: "studio.quality", testRoots: ["ai_studio/quality"] },
  {
    id: "studio.runtime-automation",
    pythonFiles: RUNTIME_PYTHON_TESTS,
    commandsByPlatform: {
      win32: RUNTIME_PYTHON_COMMANDS,
      linux: [...RUNTIME_PYTHON_COMMANDS, RUNTIME_LINUX_ITERATE],
      darwin: RUNTIME_PYTHON_COMMANDS,
    },
  },
  { id: "studio.shell", testRoots: ["ai_studio/studio_shell"] },
  { id: "studio.taskboard", testRoots: ["ai_studio/taskboard"] },
  { id: "studio.workspace", testRoots: ["ai_studio/workspace"] },
  { id: "workspace.game-create", testFiles: ["games/new_game.test.mjs"] },
  { id: "features.contracts", testFiles: ["features/validate_contracts.test.mjs"], commands: [["node", "features/validate_contracts.mjs"]] },
  { id: "features.audio.web", testFiles: ["features/audio-core/tests/test_audio_web_library.mjs"] },
  { id: "features.platform-sdk", testFiles: ["features/platform-sdk/tests/platform_sdk.test.mjs"] },
  { id: "features.game-state", pythonFiles: GAME_STATE_PYTHON_TESTS, commands: pythonCommands(GAME_STATE_PYTHON_TESTS) },
  { id: "features.items-core", pythonFiles: ITEMS_PYTHON_TESTS, commands: pythonCommands(ITEMS_PYTHON_TESTS) },
  { id: "features.progression-core", pythonFiles: PROGRESSION_PYTHON_TESTS, commands: pythonCommands(PROGRESSION_PYTHON_TESTS) },
  { id: "reference-template", testFiles: ["templates/new_template.test.mjs", "templates/template/tools/build_web.test.mjs"] },
  { id: "reference-template.python", pythonFiles: TEMPLATE_PYTHON_TESTS, commands: pythonCommands(TEMPLATE_PYTHON_TESTS) },
  { id: "studio.validation", commands: [
    ["node", "ai_studio/taskboard/cli.mjs", "validate", "--json"],
    ["node", "ai_studio/architecture_map/validate_map.mjs", "--strict", "--report", "tmp/studio-verify-map-report.json"],
    ["node", "ai_studio/core_harness/validation/doc_reference_check.mjs"],
    ["node", "ai_studio/core_harness/validation/enforcement_check.mjs"],
    ["node", "ai_studio/core_harness/agent_surfaces/sync.mjs", "--check"],
  ] },
  {
    id: "audio.native",
    commandsByPlatform: {
      win32: [
        ["cmake", "--build", "templates/template/build/native-debug", "--target", "test_audio_core", "test_audio_resource", "test_audio_backend_native", "test_game_audio"],
        ["ctest", "--test-dir", "templates/template/build/native-debug", "-R", "^(test_audio_core|test_audio_resource|test_audio_backend_native|test_game_audio|test_audio_web_library)$", "--output-on-failure"],
      ],
      linux: [["bash", "features/audio-core/tests/run_linux.sh"]],
    },
  },
]);

const FULL_IDS = SUITES.map((suite) => suite.id);

function publicSuite(suite) {
  return {
    id: suite.id,
    availability: suite.availability || "available",
    commandPlan: suite.commandsByPlatform
      ? { windows: suite.commandsByPlatform.win32, linux: suite.commandsByPlatform.linux }
      : (suite.commands || []),
    ...(suite.owner ? { owner: suite.owner } : {}),
    ...(suite.reason ? { reason: suite.reason } : {}),
  };
}

export function describeStudio() {
  return {
    schema: "ai_studio.studio.describe.v1",
    exitSemantics: { pass: 0, failed: 1, blockedOrSetup: 2 },
    routes: ROUTES,
    verification: {
      execution: "sequential",
      changedSource: "git status --porcelain=v1 -z --untracked-files=all",
      gamePolicy: "games/<id> paths are never executed; games/new_game.* is the exact root exception",
      testInventory: "every tracked deterministic Studio/feature/reference-template test must be assigned to one or more owner suites",
      suites: SUITES.map(publicSuite),
    },
  };
}

export function parsePorcelainZ(raw) {
  const records = String(raw || "").split("\0");
  const paths = [];
  for (let index = 0; index < records.length; index += 1) {
    const record = records[index];
    if (!record) continue;
    if (record.length < 4 || record[2] !== " ") throw new Error(`invalid git porcelain record: ${record}`);
    paths.push(slash(record.slice(3)));
    if (record[0] === "R" || record[0] === "C" || record[1] === "R" || record[1] === "C") {
      const other = records[++index];
      if (!other) throw new Error("invalid git porcelain rename record");
      paths.push(slash(other));
    }
  }
  return [...new Set(paths)];
}

function isGameOwned(path) {
  return /^games\/[^/]+(?:\/|$)/.test(path);
}

function isDocumentationPath(path) {
  const name = path.split("/").at(-1) || "";
  return /\.(?:md|rst|adoc)$/i.test(name) || /^(?:README|INSTALL|CHANGELOG|LICENSE|NOTICE)$/i.test(name);
}

function suitesForPath(path) {
  const ids = new Set();
  const add = (...values) => values.forEach((value) => ids.add(value));
  if (["games/new_game.mjs", "games/new_game.test.mjs", "games/README.md"].includes(path)) add("workspace.game-create");
  else if (isGameOwned(path)) return [];
  else if (path === ".github/workflows/studio-verify.yml" || /^ai_studio\/studio(?:_ci|\.test)?\.mjs$/.test(path)) add("studio.facade");
  else if (path === "ai_studio/tree.json" || path.startsWith("ai_studio/architecture_map/")) add("studio.architecture-map");
  else if (path.startsWith("ai_studio/assets/canvas/")) {
    add("studio.assets.canvas");
    if (path.endsWith(".py")) add("studio.assets.canvas-python");
  } else if (path.startsWith("ai_studio/assets/")) {
    add("studio.assets");
    if (path.endsWith(".py")) add("studio.assets.python");
  } else if (path.startsWith(".codex/skills/")) {
    if (path.endsWith(".py")) add("studio.skills.python");
    else add("studio.core-harness");
  } else if (path.startsWith("ai_studio/core_harness/") || path.startsWith(".codex/") || path.startsWith(".claude/")) add("studio.core-harness");
  else if (path.startsWith("ai_studio/dev_environment/") || path.startsWith("ai_studio/python/")) add("studio.dev-environment");
  else if (path.startsWith("ai_studio/game_design/")) add("studio.game-design");
  else if (path.startsWith("ai_studio/quality/")) add("studio.quality");
  else if (path.startsWith("ai_studio/runtime_automation/")) add("studio.runtime-automation");
  else if (path.startsWith("ai_studio/studio_shell/")) add("studio.shell");
  else if (path.startsWith("ai_studio/taskboard/")) add("studio.taskboard");
  else if (path.startsWith("ai_studio/workspace/")) add("studio.workspace");
  else if (path.startsWith("features/")) {
    add("features.contracts");
    const rel = path.split("/").slice(2).join("/");
    const featureDoc = isDocumentationPath(path);
    const nativeRuntime = !featureDoc && (/^(?:src|include|vendor|tests|scripts)\//.test(rel) || rel === "feature.json");
    if (path.startsWith("features/audio-core/")) {
      add("features.audio.web");
      if (nativeRuntime && !rel.startsWith("web/")) add("reference-template.native", "audio.native");
      if (!featureDoc && /^(?:web\/|src\/audio_backend_web\.c|feature\.json)/.test(rel)) add("reference-template.web");
    } else if (path.startsWith("features/platform-sdk/")) {
      add("features.platform-sdk");
      if (nativeRuntime && !rel.startsWith("web/")) add("reference-template.native");
      if (!featureDoc && /^(?:web\/|publish-targets\/|scripts\/artifact_tools\.mjs|feature\.json)/.test(rel)) add("reference-template.web");
    } else if (path.startsWith("features/game-state/")) {
      add("features.game-state");
      if (nativeRuntime) add("reference-template.native", "reference-template.web");
    } else if (path.startsWith("features/items-core/")) {
      add("features.items-core");
      if (nativeRuntime) add("reference-template.native", "reference-template.web");
    } else if (path.startsWith("features/progression-core/")) {
      add("features.progression-core");
      if (nativeRuntime) add("reference-template.native", "reference-template.web");
    } else if (path.startsWith("features/game-events/") && nativeRuntime) add("reference-template.native", "reference-template.web");
  } else if (path.startsWith("templates/template/")) {
    add("reference-template");
    if (path === "templates/template/game-dependencies.json") add("features.contracts");
    if (!isDocumentationPath(path)) {
      if (/^templates\/template\/(?:CMakeLists\.txt|cmake\/|src\/|assets\/|state\/|content\/|tests\/test_.*\.c$)/.test(path)) add("reference-template.native");
      if (/^templates\/template\/(?:CMakeLists\.txt|cmake\/|web\/|src\/|assets\/|state\/|content\/|tools\/build_web)/.test(path)) add("reference-template.web");
      if (path.startsWith("templates/template/devapi/") || TEMPLATE_PYTHON_TESTS.includes(path)) add("reference-template.python");
    }
  } else if (path.startsWith("templates/")) add("reference-template");
  else if (["AGENTS.md", "CLAUDE.md", "README.md", "ai_studio/README.md"].includes(path)) add("studio.validation");
  else return ["full"];

  if (ids.size && ![...ids].every((id) => id === "workspace.game-create")) ids.add("studio.validation");
  return SUITES.map((suite) => suite.id).filter((id) => ids.has(id));
}

export function selectChangedSuites(paths) {
  const selected = [];
  for (const raw of paths || []) {
    for (const id of suitesForPath(slash(raw).replace(/^\.\//, ""))) {
      if (id === "full") return ["full"];
      if (!selected.includes(id)) selected.push(id);
    }
  }
  return selected;
}

function walkTests(root, relRoot) {
  const absolute = resolve(root, relRoot);
  if (!existsSync(absolute)) return [];
  if (statSync(absolute).isFile()) return [slash(relRoot)];
  const out = [];
  for (const name of readdirSync(absolute).sort()) {
    const rel = slash(`${relRoot}/${name}`);
    const path = resolve(root, rel);
    if (statSync(path).isDirectory()) out.push(...walkTests(root, rel));
    else if (name.endsWith(".test.mjs")) out.push(rel);
  }
  return out;
}

function suiteCommands(root, suite) {
  const tests = [
    ...(suite.testFiles || []),
    ...(suite.testRoots || []).flatMap((path) => walkTests(root, path)),
  ];
  const uniqueTests = [...new Set(tests)].filter((path) => existsSync(resolve(root, path)));
  const commands = [];
  if (uniqueTests.length) commands.push([process.execPath, "--test", "--test-concurrency=1", ...uniqueTests]);
  commands.push(...(suite.commandsByPlatform?.[process.platform] || suite.commands || []));
  return commands;
}

export function isDeterministicTestPath(path) {
  if (path === "games/new_game.test.mjs") return true;
  if (!/^(ai_studio|features|templates|\.codex\/skills)\//.test(path)) return false;
  return path.endsWith(".test.mjs") || /(?:^|\/)(?:test_[^/]+|[^/]+_test)\.py$/.test(path) || path.endsWith(".test.py") || /\/tests\/test_[^/]+\.c$/.test(path);
}

function suiteOwnsTest(suite, path) {
  if (suite.testFiles?.includes(path) || suite.pythonFiles?.includes(path) || suite.nativeFiles?.includes(path)) return true;
  if (suite.testRoots?.some((root) => path.startsWith(`${root}/`)) && path.endsWith(".test.mjs")) return true;
  return false;
}

export function unassignedDeterministicTests(root = ROOT) {
  const repoRoot = resolve(root);
  const result = spawnSync("git", ["-c", `safe.directory=${slash(repoRoot)}`, "ls-files", "-z"], { cwd: repoRoot, encoding: "utf8", shell: false });
  if (result.error || result.status !== 0) throw new Error(result.error?.message || result.stderr || "git ls-files failed");
  return result.stdout.split("\0").map(slash).filter(isDeterministicTestPath).filter((path) => !SUITES.some((suite) => suiteOwnsTest(suite, path)));
}

function tailOutput(value, maxLines = 40, maxBytes = 8192) {
  const lines = String(value || "").trimEnd().split(/\r?\n/).slice(-maxLines);
  return lines.join("\n").slice(-maxBytes);
}

function rollingAppend(current, chunk, limit = 8192) {
  return `${current}${chunk}`.slice(-limit);
}

function runCommandBounded(command, args, options) {
  return new Promise((resolveResult) => {
    const child = spawn(command, args, { cwd: options.root, shell: false, env: process.env, stdio: ["ignore", "pipe", "pipe"] });
    let tail = "";
    child.stdout.on("data", (chunk) => { tail = rollingAppend(tail, chunk); });
    child.stderr.on("data", (chunk) => { tail = rollingAppend(tail, chunk); });
    child.on("error", (error) => resolveResult({ status: 2, setupError: true, tail: rollingAppend(tail, error.message) }));
    child.on("close", (status) => resolveResult({ status: status ?? 1, tail }));
  });
}

export async function runOwnedSuite(suite, options = {}) {
  const root = resolve(options.root || ROOT);
  if (suite.requiresEmsdk) {
    const emsdk = process.env.EMSDK || "";
    const toolchain = emsdk ? resolve(emsdk, "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake") : "";
    if (!emsdk || !existsSync(toolchain)) {
      return { status: 2, classification: "setup-blocked", code: "missing-emsdk", stderr: emsdk ? `missing Emscripten toolchain: ${toolchain}` : "EMSDK is required for reference-template.web" };
    }
  }
  for (const [command, ...args] of suiteCommands(root, suite)) {
    const result = await runCommandBounded(command, args, { root });
    if (result.status !== 0) return { status: result.status, stderr: result.tail, ...(result.setupError ? { setupError: true } : {}) };
  }
  return { status: 0, stdout: "", stderr: "" };
}

function changedPaths(root) {
  const result = spawnSync("git", ["status", "--porcelain=v1", "-z", "--untracked-files=all"], { cwd: root, encoding: "utf8", shell: false });
  if (result.error || result.status !== 0) throw new Error(result.error?.message || result.stderr || "git status failed");
  return parsePorcelainZ(result.stdout);
}

export async function verifyStudio(options = {}, dependencies = {}) {
  const mode = options.mode;
  if (!['changed', 'full'].includes(mode)) throw new Error("verify requires exactly one of --changed or --full");
  const root = resolve(options.root || ROOT);
  const readChangedPaths = dependencies.changedPaths || changedPaths;
  const selection = mode === "full" ? FULL_IDS : selectChangedSuites(options.paths || readChangedPaths(root));
  const ids = selection.includes("full") ? FULL_IDS : selection;
  const results = [];
  for (const id of ids) {
    const suite = SUITES.find((entry) => entry.id === id);
    if (suite.availability === "required-pending") {
      results.push({ id, status: "blocked", owner: suite.owner, reason: suite.reason });
      continue;
    }
    const run = dependencies.runSuite || ((entry) => runOwnedSuite(entry, { root }));
    const raw = await run(suite);
    if (raw.classification === "setup-blocked") {
      results.push({ id, status: "blocked", code: raw.code || "setup", reason: raw.stderr || "suite setup is unavailable" });
      continue;
    }
    if (raw.setupError) throw new Error(raw.stderr || `suite setup failed: ${suite.id}`);
    const status = raw.status === 0 ? "pass" : "fail";
    results.push({ id, status, ...(status === "fail" ? { tail: tailOutput(`${raw.stdout || ""}${raw.stderr || ""}`) } : {}) });
  }
  const blocked = results.some((entry) => entry.status === "blocked");
  const failed = results.some((entry) => entry.status === "fail");
  const status = blocked ? "blocked" : failed ? "failed" : "pass";
  return { schema: "ai_studio.studio.verify.v1", mode, status, ok: status === "pass", exitCode: blocked ? 2 : failed ? 1 : 0, suites: results };
}

function parseCli(argv) {
  const [command, ...rest] = argv;
  const json = rest.includes("--json");
  const args = rest.filter((arg) => arg !== "--json");
  if (command === "describe" && args.length === 0) return { command, json };
  if (command === "verify" && args.length === 1 && ["--changed", "--full"].includes(args[0])) return { command, json, mode: args[0].slice(2) };
  throw new Error("usage: node ai_studio/studio.mjs describe [--json] | verify (--changed|--full) [--json]");
}

function printText(result) {
  if (result.schema === "ai_studio.studio.describe.v1") {
    console.log(Object.entries(result.routes).map(([id, route]) => `${id}\t${route.owner}`).join("\n"));
    return;
  }
  for (const suite of result.suites) {
    console.log(`${suite.status}\t${suite.id}`);
    if (suite.tail) console.log(suite.tail.split(/\r?\n/).map((line) => `  ${line}`).join("\n"));
  }
  console.log(`${result.status}\t${result.mode}\t${result.suites.length} suites`);
}

function errorEnvelope(error) {
  const message = error?.message || String(error);
  const code = message.startsWith("usage:") || message.startsWith("verify requires") ? "usage" : "setup";
  return { schema: "ai_studio.studio.error.v1", ok: false, error: { code, message }, exitCode: 2 };
}

export async function main(argv = process.argv.slice(2), dependencies = {}) {
  const wantsJson = argv.includes("--json");
  try {
    const args = parseCli(argv);
    const result = args.command === "describe" ? describeStudio() : await verifyStudio({ mode: args.mode }, dependencies);
    if (args.json) console.log(JSON.stringify(result));
    else printText(result);
    return args.command === "verify" ? result.exitCode : 0;
  } catch (error) {
    const envelope = errorEnvelope(error);
    if (wantsJson) console.log(JSON.stringify(envelope));
    else console.error(envelope.error.message);
    return 2;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  process.exitCode = await main();
}
