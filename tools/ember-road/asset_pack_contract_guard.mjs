#!/usr/bin/env node
import { existsSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";

function usage() {
  console.error("usage: node tools/ember-road/asset_pack_contract_guard.mjs [--root <dir>] [--json]");
  process.exit(2);
}

const args = process.argv.slice(2);
function takeString(name, fallback) {
  const index = args.indexOf(name);
  if (index === -1) return fallback;
  const value = args[index + 1];
  if (!value || value.startsWith("--")) usage();
  args.splice(index, 2);
  return value;
}

const root = resolve(takeString("--root", process.cwd()));
const json = args.includes("--json");
if (json) args.splice(args.indexOf("--json"), 1);
if (args.includes("--help") || args.includes("-h")) usage();
if (args.length > 0) usage();

function readRel(path) {
  const abs = join(root, path);
  return existsSync(abs) ? readFileSync(abs, "utf8") : null;
}

function activeEmberRoad() {
  const status = readRel(join("tasks", "STATUS.md")) || "";
  const agents = readRel("AGENTS.md") || "";
  return /\bember-road\b/i.test(`${status}\n${agents}`);
}

function requireFile(problems, path) {
  const text = readRel(path);
  if (text === null) {
    problems.push({ file: path, rule: "missing-file", detail: "required Ember Road asset-pack contract file is missing" });
    return "";
  }
  return text;
}

function requirePattern(problems, file, text, rule, pattern, detail) {
  if (!pattern.test(text)) problems.push({ file, rule, detail });
}

const problems = [];
const skipped = !activeEmberRoad();

if (!skipped) {
  const cmake = requireFile(problems, "CMakeLists.txt");
  const builder = requireFile(problems, join("tools", "ember-road", "build_packs.c"));
  const runtime = requireFile(problems, join("src", "clean_seed_main.c"));

  requirePattern(
    problems,
    "CMakeLists.txt",
    cmake,
    "pack-builder-target",
    /add_executable\s*\(\s*build_ember_road_packs\s+tools\/ember-road\/build_packs\.c\s*\)/,
    "clean native builds must build the Ember Road pack builder",
  );
  requirePattern(
    problems,
    "CMakeLists.txt",
    cmake,
    "base-pack-output",
    /set\s*\(\s*EMBER_ROAD_BASE_PACK\s+"\$\{EMBER_ROAD_PACK_DIR\}\/ember_road_base\.ntpack"\s*\)/,
    "base pack output must stay stable and project-local",
  );
  requirePattern(
    problems,
    "CMakeLists.txt",
    cmake,
    "runtime-pack-copy",
    /set\s*\(\s*EMBER_ROAD_RUNTIME_PACK\s+"\$\{EMBER_ROAD_RUNTIME_ASSET_DIR\}\/ember_road_base\.ntpack"\s*\)[\s\S]*copy_if_different\s+"\$\{EMBER_ROAD_BASE_PACK\}"\s+"\$\{EMBER_ROAD_RUNTIME_PACK\}"/,
    "clean build must copy ember_road_base.ntpack into the runtime assets directory",
  );
  requirePattern(
    problems,
    "CMakeLists.txt",
    cmake,
    "game-depends-runtime-assets",
    /add_dependencies\s*\(\s*\$\{GAME_TARGET\}\s+ember_road_runtime_assets\s*\)/,
    "game target must depend on generated runtime assets",
  );
  requirePattern(
    problems,
    "CMakeLists.txt",
    cmake,
    "font-source-dependency",
    /external\/neotolis-engine\/assets\/fonts\/LilitaOne-RussianChineseKo\.ttf/,
    "font source must be an explicit pack dependency so rebuilds notice font changes",
  );
  requirePattern(
    problems,
    "CMakeLists.txt",
    cmake,
    "runtime-pack-compile-path",
    /EMBER_ROAD_BASE_PACK_PATH="\$\{EMBER_ROAD_RUNTIME_PACK_C\}"/,
    "runtime must receive the generated pack path from CMake",
  );

  requirePattern(
    problems,
    "tools/ember-road/build_packs.c",
    builder,
    "font-resource",
    /nt_builder_add_font[\s\S]*\.resource_name\s*=\s*"ember_road\/font_ui"/,
    "pack builder must include the UI font resource ember_road/font_ui",
  );
  requirePattern(
    problems,
    "tools/ember-road/build_packs.c",
    builder,
    "latin-cyrillic-charset",
    /NT_CHARSET_ASCII[\s\S]*CYRILLIC_CHARSET|CYRILLIC_CHARSET[\s\S]*NT_CHARSET_ASCII/,
    "font pack must include Latin plus Cyrillic glyph coverage",
  );
  requirePattern(
    problems,
    "tools/ember-road/build_packs.c",
    builder,
    "fail-missing-font",
    /file_exists\s*\(\s*font_path\s*\)[\s\S]*return\s+1\s*;/,
    "pack builder must fail loudly when the required font file is missing",
  );
  requirePattern(
    problems,
    "tools/ember-road/build_packs.c",
    builder,
    "fail-build-error",
    /nt_builder_finish_pack[\s\S]*result\s*!=\s*NT_BUILD_OK[\s\S]*return\s+1\s*;/,
    "pack builder must fail loudly when pack generation fails",
  );

  requirePattern(
    problems,
    "src/clean_seed_main.c",
    runtime,
    "generated-header-used",
    /#include\s+"ember_road_base\.h"/,
    "runtime must use generated asset ids from the pack header",
  );
  requirePattern(
    problems,
    "src/clean_seed_main.c",
    runtime,
    "runtime-loads-pack",
    /nt_resource_load_auto\s*\(\s*s_asset_pack_id\s*,\s*EMBER_ROAD_BASE_PACK_PATH\s*\)/,
    "runtime must load the generated pack from the CMake-provided path",
  );
  requirePattern(
    problems,
    "src/clean_seed_main.c",
    runtime,
    "runtime-requests-font",
    /nt_resource_request\s*\(\s*ASSET_FONT_EMBER_ROAD_FONT_UI\s*,\s*NT_ASSET_FONT\s*\)/,
    "runtime must request the packed UI font resource",
  );
  requirePattern(
    problems,
    "src/clean_seed_main.c",
    runtime,
    "engine-text-renderer",
    /nt_text_renderer_draw/,
    "runtime product text must render through the engine text renderer",
  );
}

const report = {
  schema: "ember_road.asset_pack_contract_guard",
  active_concept: !skipped,
  skipped,
  status: problems.length > 0 ? "fail" : "pass",
  problems,
};

if (json) {
  console.log(JSON.stringify(report, null, 2));
} else if (skipped) {
  console.log("ok: Ember Road asset pack contract skipped (ember-road inactive)");
} else if (problems.length === 0) {
  console.log("ok: Ember Road clean build asset pack contract is enforced");
} else {
  for (const problem of problems) {
    console.error(`${problem.file}: ${problem.rule}: ${problem.detail}`);
  }
}

if (problems.length > 0) process.exit(1);
