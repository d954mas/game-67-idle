#!/usr/bin/env node

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { basename, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawn, spawnSync } from "node:child_process";

const ROOT = resolve(fileURLToPath(new URL("..", import.meta.url)));
const slash = (value) => String(value || "").replaceAll("\\", "/");
const NODE_TEST_CONCURRENCY = 4;
const DOMAIN_CONCURRENCY = 3;
const PYTHON_RUN = ["node", "ai_studio/dev_environment/python_run.mjs"];
const pythonCommands = (files, options = {}) => {
  if (!options.batchUnittest) return files.map((file) => [...PYTHON_RUN, file]);
  const direct = files.filter((file) => file.endsWith(".test.py"));
  const batch = files.filter((file) => !file.endsWith(".test.py"));
  return [
    ...(batch.length ? [[...PYTHON_RUN, "-m", "unittest", ...batch]] : []),
    ...direct.map((file) => [...PYTHON_RUN, file]),
  ];
};

const RUNTIME_LINUX_ITERATE = [
  "xvfb-run", "-a", "node", "ai_studio/dev_environment/python_run.mjs",
  "ai_studio/runtime_automation/iterate.py", "--no-capture", "--json",
];

const ROUTES = Object.freeze({
  game: {
    owner: "game-owned lifecycle",
    availability: "game-owned",
    commands: [
      { id: "create", availability: "shared", owner: "games/new_game.mjs", route: ["node", "games/new_game.mjs"] },
      { id: "doctor", availability: "game-owned", owner: "<game-root>/tools/game.mjs", route: ["node", "<game-root>/tools/game.mjs", "doctor"] },
      { id: "build", availability: "game-owned", owner: "<game-root>/tools/game.mjs", route: ["node", "<game-root>/tools/game.mjs", "build"] },
      { id: "run", availability: "game-owned", owner: "<game-root>/tools/game.mjs", route: ["node", "<game-root>/tools/game.mjs", "run"] },
      { id: "test", availability: "game-owned", owner: "<game-root>/tools/game.mjs", route: ["node", "<game-root>/tools/game.mjs", "test"] },
      { id: "playable", availability: "game-owned", owner: "<game-root>/tools/game.mjs", route: ["node", "<game-root>/tools/game.mjs", "playable"] },
      { id: "package", availability: "game-owned", owner: "<game-root>/tools/game.mjs", route: ["node", "<game-root>/tools/game.mjs", "package"], note: "build_web.mjs only builds; game.mjs owns final ZIP verification." },
      { id: "verify", availability: "game-owned", owner: "<game-root>/tools/game.mjs", route: ["node", "<game-root>/tools/game.mjs", "verify"] },
    ],
  },
  canvas: { owner: "ai_studio/assets/canvas", surface: "/canvas/", cli: ["node", "ai_studio/assets/canvas/cli.mjs"] },
  taskboard: { owner: "ai_studio/taskboard", surface: "/taskboard/", cli: ["node", "ai_studio/taskboard/cli.mjs"] },
  assets: {
    owner: "ai_studio/assets",
    surface: "/asset_viewer/",
    tools: {
      search: ["node", "ai_studio/assets/catalog/search.mjs"],
      intake: ["node", "ai_studio/assets/intake/stage.mjs"],
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
  ["node", "templates/template/tools/build_web.mjs", "--preset", "wasm-release", "--target", "itch", "--no-debug-ui"],
  ["node", "-e", "const fs=require('node:fs'); for (const p of ['templates/template/build/wasm-release-itch/bin/game.js','templates/template/build/wasm-release-itch/bin/game.wasm','templates/template/build/wasm-release-itch/bin/assets/game.ntpack']) if (!fs.existsSync(p)) throw new Error('missing artifact '+p)"],
  ["node", "templates/template/tools/game.mjs", "verify", "--target", "itch", "--no-build", "--template-proof", "--skip-tests", "--out", "build/package-proof"],
];

const CHECKS = Object.freeze([
  { id: "studio.facade", testFiles: ["ai_studio/studio.test.mjs", "ai_studio/studio_ci.test.mjs"] },
  { id: "studio.config", testFiles: ["ai_studio/config.test.mjs"] },
  {
    id: "reference-template.native",
    commands: NATIVE_COMMANDS,
    nativeRoots: ["features/audio-core/tests", "templates/template/tests"],
    releaseOnly: true,
  },
  { id: "reference-template.web", commands: WEB_COMMANDS, requiresEmsdk: true, releaseOnly: true },
  {
    id: "studio.architecture-map",
    testRoots: ["ai_studio/architecture_map"],
    commands: [["node", "ai_studio/architecture_map/validate_map.mjs", "--strict", "--report", "tmp/studio-verify-map-report.json"]],
  },
  { id: "studio.assets.canvas", testRoots: ["ai_studio/assets/canvas"] },
  { id: "studio.assets.canvas-python", pythonRoots: ["ai_studio/assets/canvas/tools"] },
  {
    id: "studio.assets",
    testRoots: [
      "ai_studio/assets/catalog",
      "ai_studio/assets/gallery",
      "ai_studio/assets/intake",
      "ai_studio/assets/items_viewer",
      "ai_studio/assets/licenses",
      "ai_studio/assets/manifests",
      "ai_studio/assets/previews",
      "ai_studio/assets/sources",
      "ai_studio/assets/tests",
      "ai_studio/assets/tools",
    ],
  },
  { id: "studio.assets.python", pythonRoots: ["ai_studio/assets/tools"], batchPythonUnittest: true },
  {
    id: "studio.core-harness",
    testRoots: ["ai_studio/core_harness"],
    commands: [
      ["node", "ai_studio/core_harness/validation/doc_reference_check.mjs"],
      ["node", "ai_studio/core_harness/agent_surfaces/sync.mjs", "--check"],
    ],
  },
  { id: "studio.skills.python", pythonRoots: [".codex/skills"] },
  { id: "studio.dev-environment", testRoots: ["ai_studio/dev_environment"] },
  { id: "studio.quality", testRoots: ["ai_studio/quality"] },
  {
    id: "studio.runtime-automation",
    pythonRoots: ["ai_studio/runtime_automation"],
  },
  {
    id: "studio.runtime-automation.live",
    commandsByPlatform: { linux: [RUNTIME_LINUX_ITERATE] },
    releaseOnly: true,
  },
  { id: "studio.shell", testRoots: ["ai_studio/studio_shell"] },
  {
    id: "studio.taskboard",
    testRoots: ["ai_studio/taskboard"],
    commands: [["node", "ai_studio/taskboard/cli.mjs", "validate", "--json"]],
  },
  { id: "studio.workspace", testRoots: ["ai_studio/workspace"] },
  { id: "workspace.game-create", testFiles: ["games/new_game.test.mjs"] },
  { id: "features.contracts", testFiles: ["features/validate_contracts.test.mjs"], commands: [["node", "features/validate_contracts.mjs"]] },
  { id: "features.audio.web", testFiles: ["features/audio-core/tests/test_audio_web_library.mjs"] },
  {
    id: "features.audio.linux",
    commandsByPlatform: { linux: [["bash", "features/audio-core/tests/run_linux.sh"]] },
    releaseOnly: true,
  },
  { id: "features.platform-sdk", testFiles: ["features/platform-sdk/tests/platform_sdk.test.mjs"] },
  { id: "features.game-state", pythonRoots: ["features/game-state"] },
  { id: "features.items-core", pythonRoots: ["features/items-core"] },
  { id: "features.progression-core", pythonRoots: ["features/progression-core"] },
  { id: "reference-template", testFiles: [
    "templates/new_template.test.mjs",
    "templates/template/tools/build_web.test.mjs",
    "templates/template/tools/game.test.mjs",
    "templates/template/tools/package_web.test.mjs",
    "templates/template/tools/portal_evidence.test.mjs",
  ] },
  { id: "reference-template.python", pythonRoots: ["templates/template/devapi"] },
]);

const DOMAINS = Object.freeze([
  { id: "harness", checks: ["studio.facade", "studio.config", "studio.core-harness", "studio.skills.python", "studio.dev-environment", "studio.quality"] },
  { id: "workspace", checks: ["studio.workspace", "workspace.game-create"] },
  { id: "architecture", checks: ["studio.architecture-map"] },
  { id: "shell", checks: ["studio.shell"] },
  { id: "assets", checks: ["studio.assets.canvas", "studio.assets.canvas-python", "studio.assets", "studio.assets.python"] },
  { id: "work-management", checks: ["studio.taskboard"] },
  { id: "design", checks: [] },
  { id: "runtime", checks: ["studio.runtime-automation", "studio.runtime-automation.live"] },
  { id: "features", checks: ["features.contracts", "features.audio.web", "features.audio.linux", "features.platform-sdk", "features.game-state", "features.items-core", "features.progression-core"] },
  { id: "template-release", checks: ["reference-template", "reference-template.python", "reference-template.native", "reference-template.web"] },
]);

const FULL_IDS = DOMAINS.map((domain) => domain.id);

function publicDomain(domain) {
  return {
    id: domain.id,
    checks: domain.checks.length,
    releaseProof: domain.checks.some((id) => CHECKS.find((check) => check.id === id)?.releaseOnly),
  };
}

export function describeStudio() {
  return {
    schema: "ai_studio.studio.describe.v1",
    exitSemantics: { pass: 0, failed: 1, blockedOrSetup: 2 },
    routes: ROUTES,
    verification: {
      execution: "owner-domains",
      domainConcurrency: DOMAIN_CONCURRENCY,
      nodeTestConcurrency: NODE_TEST_CONCURRENCY,
      changedSource: "git status --porcelain=v1 -z --untracked-files=all",
      gamePolicy: "games/<id> paths are never executed; games/new_game.* is the exact root exception",
      testInventory: "deterministic tests belong to one owner domain; internal checks are not public routing categories",
      domains: DOMAINS.map(publicDomain),
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
  return /^games\/[^/]+\//.test(path);
}

function domainForPath(path) {
  if (["games/new_game.mjs", "games/new_game.test.mjs", "games/README.md"].includes(path)) return "workspace";
  if (isGameOwned(path)) return null;
  if (path === "ai_studio/tree.json" || path.startsWith("ai_studio/architecture_map/")) return "architecture";
  if (path.startsWith("ai_studio/assets/")) return "assets";
  if (path.startsWith("ai_studio/game_design/")) return "design";
  if (path.startsWith("ai_studio/runtime_automation/")) return "runtime";
  if (path.startsWith("ai_studio/studio_shell/")) return "shell";
  if (path.startsWith("ai_studio/taskboard/")) return "work-management";
  if (path.startsWith("ai_studio/workspace/")) return "workspace";
  if (path.startsWith("features/")) return "features";
  if (path.startsWith("templates/")) return "template-release";
  if (
    path === ".github/workflows/studio-verify.yml"
    || path.startsWith(".vscode/")
    || /^ai_studio\/studio(?:_ci|\.test)?\.mjs$/.test(path)
    || /^ai_studio\/config(?:\.test)?\.mjs$/.test(path)
    || /^ai_studio\/studio\.config(?:\.local)?\.json$/.test(path)
    || path.startsWith("ai_studio/core_harness/")
    || path.startsWith("ai_studio/dev_environment/")
    || path.startsWith("ai_studio/python/")
    || path.startsWith("ai_studio/quality/")
    || path.startsWith(".codex/")
    || path.startsWith(".claude/")
    || [".gitignore", "AGENTS.md", "CLAUDE.md", "README.md", "ai_studio/README.md"].includes(path)
  ) return "harness";
  throw new Error(`unowned shared path: ${path}`);
}

export function selectChangedDomains(paths) {
  const selected = new Set();
  for (const raw of paths || []) {
    const id = domainForPath(slash(raw).replace(/^\.\//, ""));
    if (id) selected.add(id);
  }
  return FULL_IDS.filter((id) => selected.has(id));
}

const DISCOVERY_IGNORES = new Set([
  ".git", ".mypy_cache", ".pytest_cache", ".ruff_cache", ".venv",
  "venv", "node_modules", "__pycache__", "build", "tmp",
]);

function walkMatchingFiles(root, relRoot, matches) {
  const absolute = resolve(root, relRoot);
  if (!existsSync(absolute)) return [];
  if (statSync(absolute).isFile()) return matches(absolute.split(/[\\/]/).at(-1)) ? [slash(relRoot)] : [];
  const out = [];
  for (const name of readdirSync(absolute).sort()) {
    if (DISCOVERY_IGNORES.has(name)) continue;
    const rel = slash(`${relRoot}/${name}`);
    const path = resolve(root, rel);
    if (statSync(path).isDirectory()) out.push(...walkMatchingFiles(root, rel, matches));
    else if (matches(name)) out.push(rel);
  }
  return out;
}

function nodeTestsForCheck(root, check) {
  return [
    ...(check.testFiles || []),
    ...(check.testRoots || []).flatMap((path) => walkMatchingFiles(root, path, (name) => name.endsWith(".test.mjs"))),
  ].filter((path) => existsSync(resolve(root, path)));
}

function pythonTestsForCheck(root, check) {
  return (check.pythonRoots || []).flatMap((path) => walkMatchingFiles(root, path, (name) => (
    /(?:^test_.+|.+_test|.+\.test)\.py$/.test(name)
  )));
}

export function isDeterministicTestPath(path) {
  if (path === "games/new_game.test.mjs") return true;
  if (!/^(ai_studio|features|templates|\.codex\/skills)\//.test(path)) return false;
  return path.endsWith(".test.mjs") || /(?:^|\/)(?:test_[^/]+|[^/]+_test)\.py$/.test(path) || path.endsWith(".test.py") || /\/tests\/test_[^/]+\.c$/.test(path);
}

function suiteOwnsTest(suite, path) {
  if (suite.testFiles?.includes(path) || suite.pythonFiles?.includes(path)) return true;
  if (suite.testRoots?.some((root) => path.startsWith(`${root}/`)) && path.endsWith(".test.mjs")) return true;
  if (suite.pythonRoots?.some((root) => path.startsWith(`${root}/`)) && /(?:^|\/)(?:test_[^/]+|[^/]+_test|[^/]+\.test)\.py$/.test(path)) return true;
  if (suite.nativeRoots?.some((root) => path.startsWith(`${root}/`)) && /\/tests\/test_[^/]+\.c$/.test(path)) return true;
  return false;
}

function cmakeSourceCorpus(root, trackedPaths) {
  return trackedPaths
    .filter((path) => path === "templates/template/CMakeLists.txt"
      || (path.startsWith("templates/template/cmake/") && path.endsWith(".cmake")))
    .filter((path) => existsSync(resolve(root, path)))
    .map((path) => readFileSync(resolve(root, path), "utf8").replace(/#.*$/gm, ""))
    .join("\n");
}

function cmakeMentionsNativeTest(cmakeSources, testPath) {
  const escaped = basename(testPath).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  return new RegExp(`(?:^|[^A-Za-z0-9_.-])${escaped}(?:$|[^A-Za-z0-9_.-])`).test(cmakeSources);
}

export function unassignedDeterministicTests(root = ROOT) {
  const repoRoot = resolve(root);
  const result = spawnSync("git", ["-c", `safe.directory=${slash(repoRoot)}`, "ls-files", "-z"], { cwd: repoRoot, encoding: "utf8", shell: false });
  if (result.error || result.status !== 0) throw new Error(result.error?.message || result.stderr || "git ls-files failed");
  const trackedPaths = result.stdout.split("\0").map(slash).filter(Boolean);
  const cmakeSources = cmakeSourceCorpus(repoRoot, trackedPaths);
  return trackedPaths.filter(isDeterministicTestPath).filter((path) => (
    !CHECKS.some((check) => suiteOwnsTest(check, path))
    || (/\/tests\/test_[^/]+\.c$/.test(path) && !cmakeMentionsNativeTest(cmakeSources, path))
  ));
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

export async function runOwnedDomain(domain, options = {}) {
  const root = resolve(options.root || ROOT);
  const platform = options.platform || process.platform;
  const includeRelease = options.includeRelease === true;
  const checks = [];
  for (const id of domain.checks) {
    const check = CHECKS.find((entry) => entry.id === id);
    if (!check) return { status: 2, setupError: true, stderr: `unknown check ${id} in ${domain.id}` };
    if (!check.releaseOnly || includeRelease) checks.push(check);
  }

  for (const check of checks) {
    if (!check.requiresEmsdk) continue;
    const emsdk = process.env.EMSDK || "";
    const toolchain = emsdk ? resolve(emsdk, "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake") : "";
    if (!emsdk || !existsSync(toolchain)) {
      return { status: 2, classification: "setup-blocked", code: "missing-emsdk", stderr: emsdk ? `missing Emscripten toolchain: ${toolchain}` : "EMSDK is required for template release proof" };
    }
  }

  const nodeTests = [...new Set(checks.flatMap((check) => nodeTestsForCheck(root, check)))];
  const nodeSteps = [];
  const pythonSteps = [];
  const commandSteps = [];
  if (nodeTests.length) {
    nodeSteps.push({ owner: domain.id, command: process.execPath, args: ["--test", `--test-concurrency=${NODE_TEST_CONCURRENCY}`, ...nodeTests] });
  }
  for (const check of checks) {
    const pythonTests = pythonTestsForCheck(root, check);
    for (const [command, ...args] of pythonCommands(pythonTests, { batchUnittest: check.batchPythonUnittest })) {
      pythonSteps.push({ owner: check.id, command, args });
    }
    for (const [command, ...args] of check.commandsByPlatform?.[platform] || check.commands || []) {
      commandSteps.push({ owner: check.id, command, args });
    }
  }

  if (nodeSteps.length + pythonSteps.length + commandSteps.length === 0) {
    return { status: 0, classification: "not-applicable", reason: "no deterministic technical checks" };
  }

  const runCommand = options.runCommand || ((command, args) => runCommandBounded(command, args, { root }));
  const runSteps = async (steps) => {
    for (const step of steps) {
      const result = await runCommand(step.command, step.args, { root });
      if (result.status !== 0) return { step, result };
    }
    return null;
  };
  const [nodeFailure, pythonFailure] = await Promise.all([
    runSteps(nodeSteps),
    runSteps(pythonSteps),
  ]);
  const failure = nodeFailure || pythonFailure || await runSteps(commandSteps);
  if (failure) {
    const { step, result } = failure;
    return {
      status: result.status,
      ...(result.setupError ? { setupError: true } : {}),
      stderr: `[${step.owner}]\n${result.tail || result.stderr || result.stdout || "check failed"}`,
    };
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
  if (!["changed", "domain", "full"].includes(mode)) throw new Error("verify requires --changed, --domain <id>, or --full");
  const root = resolve(options.root || ROOT);
  const readChangedPaths = dependencies.changedPaths || changedPaths;
  const ids = mode === "full"
    ? FULL_IDS
    : mode === "domain"
      ? [options.domain]
      : selectChangedDomains(options.paths || readChangedPaths(root));
  if (mode === "domain" && !FULL_IDS.includes(options.domain)) throw new Error(`unknown verification domain: ${options.domain || "<missing>"}`);
  const results = new Array(ids.length);
  let cursor = 0;
  const run = dependencies.runDomain || ((entry) => runOwnedDomain(entry, { root, includeRelease: mode !== "changed" }));
  const runNext = async () => {
    while (cursor < ids.length) {
      const index = cursor++;
      const id = ids[index];
    const startedAt = performance.now();
    const durationMs = () => Math.round((performance.now() - startedAt) * 1000) / 1000;
    const domain = DOMAINS.find((entry) => entry.id === id);
    const raw = await run(domain);
    if (raw.classification === "setup-blocked") {
        results[index] = { id, status: "blocked", code: raw.code || "setup", reason: raw.stderr || "domain setup is unavailable", durationMs: durationMs() };
      continue;
    }
    if (raw.classification === "not-applicable") {
      results[index] = { id, status: "not-applicable", reason: raw.reason, durationMs: durationMs() };
      continue;
    }
    if (raw.setupError) throw new Error(raw.stderr || `domain setup failed: ${domain.id}`);
    const status = raw.status === 0 ? "pass" : "fail";
      results[index] = { id, status, durationMs: durationMs(), ...(status === "fail" ? { tail: tailOutput(`${raw.stdout || ""}${raw.stderr || ""}`) } : {}) };
    }
  };
  await Promise.all(Array.from({ length: Math.min(DOMAIN_CONCURRENCY, ids.length) }, () => runNext()));
  const blocked = results.some((entry) => entry.status === "blocked");
  const failed = results.some((entry) => entry.status === "fail");
  const status = blocked ? "blocked" : failed ? "failed" : "pass";
  return {
    schema: "ai_studio.studio.verify.v2",
    mode,
    ...(mode === "domain" ? { domain: options.domain } : {}),
    status,
    ok: status === "pass",
    exitCode: blocked ? 2 : failed ? 1 : 0,
    domains: results,
  };
}

function parseCli(argv) {
  const [command, ...rest] = argv;
  const json = rest.includes("--json");
  const args = rest.filter((arg) => arg !== "--json");
  if (command === "describe" && args.length === 0) return { command, json };
  if (command === "verify" && args.length === 1 && ["--changed", "--full"].includes(args[0])) return { command, json, mode: args[0].slice(2) };
  if (command === "verify" && args.length === 2 && args[0] === "--domain") return { command, json, mode: "domain", domain: args[1] };
  throw new Error("usage: node ai_studio/studio.mjs describe [--json] | verify (--changed|--domain <id>|--full) [--json]");
}

function printText(result) {
  if (result.schema === "ai_studio.studio.describe.v1") {
    console.log(Object.entries(result.routes).map(([id, route]) => `${id}\t${route.owner}`).join("\n"));
    return;
  }
  for (const domain of result.domains) {
    console.log(`${domain.status}\t${domain.id}\t${(domain.durationMs / 1000).toFixed(3)}s`);
    if (domain.tail) console.log(domain.tail.split(/\r?\n/).map((line) => `  ${line}`).join("\n"));
  }
  console.log(`${result.status}\t${result.mode}\t${result.domains.length} domains`);
}

function errorEnvelope(error) {
  const message = error?.message || String(error);
  const code = message.startsWith("usage:") || message.startsWith("verify requires") || message.startsWith("unknown verification domain") ? "usage" : "setup";
  return { schema: "ai_studio.studio.error.v1", ok: false, error: { code, message }, exitCode: 2 };
}

export async function main(argv = process.argv.slice(2), dependencies = {}) {
  const wantsJson = argv.includes("--json");
  try {
    const args = parseCli(argv);
    const result = args.command === "describe" ? describeStudio() : await verifyStudio({ mode: args.mode, domain: args.domain }, dependencies);
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
