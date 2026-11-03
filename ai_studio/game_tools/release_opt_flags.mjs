#!/usr/bin/env node
// Release optimization gate: release code is compiled and linked for speed,
// never for size. Two layers: a source scan (fails before any build) and an
// inspection of the commands the release Ninja tree really runs.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { basename, join, relative, resolve } from "node:path";

import { isMain } from "../core_harness/tool_lib/cli.mjs";

const STUDIO_ROOT = resolve(import.meta.dirname, "..", "..");
const RULE = [
  "Rule: release code is never compiled or linked for size (-Os, -Oz) or below -O2; performance comes first.",
  "Fix: delete the flag. A size budget is met by removing code or data, never by -Oz/-Os.",
  "See ai_studio/game_tools/README.md#release-optimization-gate",
];
// The gate and its own fixtures must name the flags they forbid.
const SELF = new Set(["ai_studio/game_tools/release_opt_flags.mjs", "ai_studio/game_tools/tests/release_opt_flags.test.mjs"]);
const SKIP_DIRS = new Set([".git", "node_modules", "CMakeFiles", "_cache", "tmp"]);
const CMAKE_FILE = /(^|\/)(CMakeLists\.txt|[^/]+\.cmake|CMake(User)?Presets\.json)$/;
const CODE_FILE = /\.(mjs|cjs|js|py)$/;
const SIZE_FLAG = /(?<![\w-])-O[sz](?![\w-])/;
const SIZE_BUILD_TYPE = /CMAKE_BUILD_TYPE\b.*\bMinSizeRel\b/;
const SPEED_LEVELS = new Set(["-O2", "-O3", "-Ofast"]);
const COMPILER = /^(emcc|em\+\+|clang|clang\+\+|gcc|g\+\+|cc|c\+\+)(\.bat|\.exe|\.py)?$/i;
const SEPARATORS = new Set(["&&", "||", ";", "|"]);

const slash = (path) => path.replaceAll("\\", "/");

function walk(root, dir = root, out = []) {
  let entries;
  try { entries = readdirSync(dir, { withFileTypes: true }); } catch { return out; }
  for (const entry of entries) {
    if (entry.isDirectory()) {
      if (!SKIP_DIRS.has(entry.name) && !/^build/.test(entry.name)) walk(root, join(dir, entry.name), out);
    } else if (entry.isFile()) out.push(slash(relative(root, join(dir, entry.name))));
  }
  return out;
}

// Untracked files count: a flag must not slip through by staying uncommitted.
function listFiles(root, paths = []) {
  const top = spawnSync("git", ["-C", root, "rev-parse", "--show-toplevel"], { encoding: "utf8" });
  if (top.status === 0 && resolve(top.stdout.trim()) === resolve(root)) {
    const listed = spawnSync("git", ["-C", root, "ls-files", "-z", "-co", "--exclude-standard", "--", ...paths],
      { encoding: "utf8", maxBuffer: 256 * 1024 * 1024 });
    if (listed.status === 0) {
      return listed.stdout.split("\0").filter(Boolean).filter((path) => !path.split("/").some((part) => SKIP_DIRS.has(part) || /^build/.test(part)));
    }
  }
  return paths.length ? paths.flatMap((path) => walk(root, join(root, path)).map((file) => slash(join(path, file)))) : walk(root);
}

function stripComment(line, kind) {
  if (kind === "cmake") {
    let quoted = false;
    for (let index = 0; index < line.length; index += 1) {
      if (line[index] === "\"" && line[index - 1] !== "\\") quoted = !quoted;
      else if (line[index] === "#" && !quoted) return line.slice(0, index);
    }
    return line;
  }
  if (kind === "json") return line;
  return /^\s*(\/\/|\/\*|\*|#)/.test(line) ? "" : line;
}

export function scanText(text, kind) {
  const hits = [];
  text.split(/\r?\n/).forEach((raw, index) => {
    const line = stripComment(raw, kind);
    const flag = SIZE_FLAG.exec(line)?.[0] || (SIZE_BUILD_TYPE.test(line) ? "MinSizeRel" : "");
    if (flag) hits.push({ line: index + 1, flag, text: raw.trim().slice(0, 160) });
  });
  return hits;
}

// codeRoots: where build-tool code lives; CMake files count anywhere under root.
export function scanSources(root, { paths = [], codeRoots = [], label = "" } = {}) {
  const violations = [];
  for (const file of listFiles(root, paths)) {
    if (SELF.has(file)) continue;
    const cmake = CMAKE_FILE.test(file);
    const code = !cmake && CODE_FILE.test(file) && codeRoots.some((prefix) => prefix === "" || file.startsWith(prefix));
    if (!cmake && !code) continue;
    let text;
    try { text = readFileSync(join(root, file), "utf8"); } catch { continue; }
    const kind = cmake ? (file.endsWith(".json") ? "json" : "cmake") : "code";
    for (const hit of scanText(text, kind)) violations.push({ file: label ? `${label}/${file}` : file, ...hit });
  }
  return violations;
}

export function scanStudioSources(studioRoot = STUDIO_ROOT) {
  return scanSources(studioRoot, {
    paths: ["templates", "features", "ai_studio/game_tools", "ai_studio/feature_testing"],
    codeRoots: ["ai_studio/game_tools/", "features/"].concat(
      existsSync(join(studioRoot, "templates")) ? readdirSync(join(studioRoot, "templates")).map((name) => `templates/${name}/tools/`) : []),
  });
}

export function scanGameSources(gameDir) {
  return scanSources(resolve(gameDir), { codeRoots: ["tools/"] });
}

function failure(title, lines) {
  return new Error([
    "",
    "!!! RELEASE OPTIMIZATION GATE FAILED !!!",
    title,
    ...lines.map((line) => `  ${line}`),
    ...RULE,
  ].join("\n"));
}

export function sourceViolationLines(violations) {
  return violations.map((v) => `${v.file}:${v.line}  ${v.flag}  ${v.text}`);
}

const studioScans = new Map();

// Runs on every game command that builds or tests; the Studio half is scanned once per process.
export function assertReleaseSources({ gameDir, studioRoot = STUDIO_ROOT }) {
  if (!studioScans.has(studioRoot)) studioScans.set(studioRoot, scanStudioSources(studioRoot));
  const violations = [...studioScans.get(studioRoot), ...scanGameSources(gameDir)];
  if (violations.length) throw failure("Size optimization flags in build sources:", sourceViolationLines(violations));
}

function tokenize(command) {
  return command.replaceAll("\"", " ").split(/\s+/).filter(Boolean);
}

function edgeName(output) {
  const target = /CMakeFiles\/([^/]+)\.dir\/(.*)$/.exec(slash(output));
  return target ? { target: target[1], unit: target[2] } : { target: slash(output), unit: "" };
}

// entries: Ninja compdb rows ({ command, output }). A violation is any size flag
// anywhere (env, launcher, wasm-opt step) or a compile/link whose last -O is
// not a speed level; clang and emcc keep the last -O they are given.
export function inspectCommands(entries) {
  const violations = [];
  for (const entry of entries) {
    const command = String(entry.command || "");
    if (!command) continue;
    const tokens = tokenize(command);
    const output = slash(String(entry.output || ""));
    const isObject = /\.(o|obj)$/.test(output);
    const kind = isObject ? "compile" : "link";
    const sizeToken = tokens.find((token) => /(^|=)-O[sz]$/.test(token));
    if (sizeToken) {
      violations.push({ ...edgeName(output), kind, flag: sizeToken.replace(/^.*=/, ""), reason: "size optimization" });
      continue;
    }
    for (let index = 0; index < tokens.length; index += 1) {
      if (!COMPILER.test(basename(tokens[index].replaceAll("\\", "/")))) continue;
      let end = index + 1;
      while (end < tokens.length && !SEPARATORS.has(tokens[end])) end += 1;
      const segment = tokens.slice(index + 1, end);
      index = end;
      const compiles = segment.includes("-c");
      const links = !compiles && /\.(js|mjs|wasm|html)$/.test(output);
      if (!compiles && !links) continue;
      const level = segment.filter((token) => /^-O(\w*)$/.test(token)).at(-1) || "";
      if (!SPEED_LEVELS.has(level)) {
        violations.push({ ...edgeName(output), kind: compiles ? "compile" : "link", flag: level || "(no -O: emcc/clang default -O0)", reason: "below -O2" });
      }
    }
  }
  return violations;
}

export function buildViolationLines(violations) {
  const groups = new Map();
  for (const v of violations) {
    const key = `${v.kind}\0${v.target}\0${v.flag}`;
    const group = groups.get(key) || { ...v, count: 0 };
    group.count += 1;
    groups.set(key, group);
  }
  return [...groups.values()].map((g) => g.kind === "compile"
    ? `target ${g.target}: ${g.count} translation unit(s) compile at ${g.flag} (e.g. ${g.unit})`
    : `link ${g.target}: ${g.flag}`);
}

function ninjaProgram(buildDir) {
  const cache = join(buildDir, "CMakeCache.txt");
  if (existsSync(cache)) {
    const match = /^CMAKE_MAKE_PROGRAM:[A-Z]+=(.+)$/m.exec(readFileSync(cache, "utf8"));
    if (match && existsSync(match[1].trim())) return match[1].trim();
  }
  return "ninja";
}

// The commands Ninja would run in this tree, response files expanded: what the
// compiler and linker really receive, whichever CMake file, feature or
// toolchain contributed the flag.
export function readBuildCommands(buildDir) {
  if (!statSync(join(buildDir, "build.ninja"), { throwIfNoEntry: false })?.isFile()) {
    throw failure(`Cannot inspect the release build: no build.ninja in ${buildDir}.`, [
      "The gate reads the compile and link commands of the Ninja tree that produced the artifact.",
    ]);
  }
  const result = spawnSync(ninjaProgram(buildDir), ["-C", buildDir, "-t", "compdb", "-x"], {
    encoding: "utf8", maxBuffer: 1024 * 1024 * 1024, stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.error || result.status !== 0) {
    throw failure(`Cannot read the release build commands in ${buildDir}.`, [String(result.error?.message || result.stderr || "").trim()]);
  }
  return JSON.parse(result.stdout);
}

export function assertReleaseBuild(buildDir, { read = readBuildCommands } = {}) {
  const violations = inspectCommands(read(buildDir));
  if (violations.length) {
    throw failure(`The release build in ${buildDir} is not optimized for speed:`, buildViolationLines(violations));
  }
  return "release flags: speed";
}

const USAGE = `usage: node ai_studio/game_tools/release_opt_flags.mjs [--game <dir>]... [--build <dir>]...

Without arguments it scans the Studio's templates, features and game tools.
--game scans that game's CMake and tools; --build inspects a configured release
Ninja tree. Exit 1 on any size flag or a release compile/link below -O2.`;

if (isMain(import.meta.url)) {
  const games = [];
  const builds = [];
  const argv = process.argv.slice(2);
  for (let index = 0; index < argv.length; index += 1) {
    if (argv[index] === "--game") games.push(argv[++index]);
    else if (argv[index] === "--build") builds.push(argv[++index]);
    else { console.log(USAGE); process.exit(argv[index] === "--help" ? 0 : 2); }
  }
  const errors = [];
  const sources = [
    ...(games.length || builds.length ? [] : scanStudioSources()),
    ...games.flatMap((game) => scanGameSources(game).map((v) => ({ ...v, file: `${slash(game)}/${v.file}` }))),
  ];
  if (sources.length) errors.push(failure("Size optimization flags in build sources:", sourceViolationLines(sources)).message);
  for (const build of builds) {
    try { assertReleaseBuild(resolve(build)); } catch (error) { errors.push(error.message); }
  }
  if (errors.length) {
    console.error(errors.join("\n"));
    process.exit(1);
  }
  console.log("release optimization gate passed");
}
