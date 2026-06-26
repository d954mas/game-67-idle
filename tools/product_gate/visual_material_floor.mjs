#!/usr/bin/env node
// Static floor for authored 3D asset visuals.
//
// This catches the failure mode where a game imports GLB/GLTF geometry but
// renders it through one flat tint/fallback material, so screenshots read as
// debug-colored blocks even though "real assets" are technically on screen.

import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const DEFAULT_ROOT = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const TEXT_EXTENSIONS = new Set([".c", ".h", ".mjs", ".js", ".frag", ".vert", ".glsl", ".md"]);

function usage() {
  console.error(`usage:
  node tools/product_gate/visual_material_floor.mjs [--root <repo>] [--json-output <path>] [--allow-color-only <reason>]

Fails active 3D asset games when sourced GLB/GLTF geometry is rendered with a
flat color-only material/fallback texture path instead of source materials,
textures, UVs, or per-primitive material colors.`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = { root: null, jsonOutput: "", allowColorOnly: "" };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") usage();
    const value = argv[index + 1];
    if (!value || value.startsWith("--")) usage();
    index += 1;
    if (arg === "--root") values.root = value;
    else if (arg === "--json-output") values.jsonOutput = value;
    else if (arg === "--allow-color-only") values.allowColorOnly = value;
    else usage();
  }
  return values;
}

function readText(path) {
  try {
    return existsSync(path) ? readFileSync(path, "utf8") : "";
  } catch {
    return "";
  }
}

function walk(root, subdir, predicate) {
  const start = join(root, subdir);
  const out = [];
  function visit(dir) {
    let entries = [];
    try {
      entries = readdirSync(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const entry of entries) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) {
        if (entry.name === ".git" || entry.name === "build" || entry.name === "tmp") continue;
        visit(path);
      } else if (predicate(path)) {
        out.push(path);
      }
    }
  }
  visit(start);
  return out;
}

function extension(path) {
  const match = path.match(/(\.[^.\\/]+)$/);
  return match ? match[1].toLowerCase() : "";
}

function activeConcept(root) {
  const gameProject = readText(join(root, "GAME_PROJECT.md"));
  if (/status:\s*none|no active game concept/i.test(gameProject)) {
    return { active: false, reason: "no active game concept" };
  }
  if (/game id:|game folder:|gamedesign[\\/]+projects/i.test(gameProject)) {
    return { active: true, reason: "active game concept detected" };
  }
  return { active: false, reason: "no active game concept" };
}

function lineOf(text, pattern) {
  const lines = text.split(/\r?\n/);
  for (let index = 0; index < lines.length; index += 1) {
    if (pattern.test(lines[index])) return index + 1;
  }
  return 1;
}

function rel(root, path) {
  return relative(root, path).replace(/\\/g, "/");
}

function sourceModelEvidence(root) {
  const files = walk(root, "src", (path) => TEXT_EXTENSIONS.has(extension(path)));
  const hits = [];
  for (const file of files) {
    const text = readText(file);
    if (/\.(glb|gltf)\b/i.test(text) || /nt_builder_add_scene_mesh|add_model\s*\(/.test(text)) {
      hits.push({ path: rel(root, file), line: lineOf(text, /\.(glb|gltf)\b|nt_builder_add_scene_mesh|add_model\s*\(/i) });
    }
  }
  return hits;
}

function colorOnlyShaderEvidence(root) {
  const files = walk(root, "assets", (path) => /\.(frag|glsl)$/i.test(path));
  const hits = [];
  for (const file of files) {
    const text = readText(file);
    const colorOnly = /frag_color\s*=\s*v_color\s*;/i.test(text);
    const samplesTexture = /\btexture\s*\(|\bsampler2D\b|\btexcoord\b|\buv\b/i.test(text);
    if (colorOnly && !samplesTexture) {
      hits.push({ path: rel(root, file), line: lineOf(text, /frag_color\s*=\s*v_color\s*;/i) });
    }
  }
  return hits;
}

function flatRuntimeEvidence(root) {
  const files = walk(root, "src", (path) => TEXT_EXTENSIONS.has(extension(path)));
  const hits = [];
  for (const file of files) {
    const text = readText(file);
    const hasTint = /nt_drawable_comp_set_color|color_mode\s*=\s*NT_COLOR_MODE_FLOAT4/i.test(text);
    const hasFallback = /nt_resource_set_placeholder_texture|fallback_checker|__fallback_checker__/i.test(text);
    const hasSingleMaterial = /s_mesh_material|nt_material_comp_handle\([^)]*\)\s*=\s*[^;]*mesh_material/i.test(text);
    const hasMaterialTable = /material_count|materials\s*\[|base_color_texture|base_color_factor|texture_count|sampler2D|texcoord/i.test(text);
    if (hasTint && hasFallback && hasSingleMaterial && !hasMaterialTable) {
      hits.push({ path: rel(root, file), line: lineOf(text, /nt_drawable_comp_set_color|nt_resource_set_placeholder_texture|s_mesh_material/i) });
    }
  }
  return hits;
}

function buildReport(root, options) {
  const concept = activeConcept(root);
  if (!concept.active) {
    return {
      schema: "game.visual_material_floor",
      verdict: "skip",
      problems: [],
      warnings: [concept.reason],
      evidence: {},
    };
  }

  const sourceModels = sourceModelEvidence(root);
  const colorOnlyShaders = colorOnlyShaderEvidence(root);
  const flatRuntime = flatRuntimeEvidence(root);
  const materialFloorFails = sourceModels.length > 0 && colorOnlyShaders.length > 0 && flatRuntime.length > 0;
  const problems = [];
  const warnings = [];

  if (options.allowColorOnly) {
    warnings.push(`color-only material floor bypassed: ${options.allowColorOnly}`);
  } else if (materialFloorFails) {
    problems.push(
      "GLB/GLTF assets are present, but runtime uses a flat color-only mesh path: source materials/textures/UVs are not proven in product rendering."
    );
  }

  const verdict = problems.length > 0 ? "fail" : warnings.length > 0 ? "warn" : "pass";
  return {
    schema: "game.visual_material_floor",
    verdict,
    problems,
    warnings,
    evidence: {
      active_concept: concept.reason,
      source_models: sourceModels,
      color_only_shaders: colorOnlyShaders,
      flat_runtime_paths: flatRuntime,
    },
  };
}

const options = parseArgs(process.argv.slice(2));
const root = resolve(options.root || process.env.TASKBOARD_ROOT || DEFAULT_ROOT);
const report = buildReport(root, options);

if (options.jsonOutput) {
  const target = resolve(options.jsonOutput);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(report, null, 2)}\n`, "utf8");
}

if (report.verdict === "skip") {
  console.log(`skip: visual material floor (${report.warnings.join("; ")})`);
  process.exit(0);
}
if (report.verdict === "pass") {
  console.log("ok: visual material floor passed");
  process.exit(0);
}
if (report.verdict === "warn") {
  console.log(`warn: visual material floor bypassed (${report.warnings.join("; ")})`);
  process.exit(0);
}

for (const problem of report.problems) console.error(`problem: ${problem}`);
for (const item of report.evidence.source_models || []) console.error(`evidence: source model import at ${item.path}:${item.line}`);
for (const item of report.evidence.color_only_shaders || []) console.error(`evidence: color-only shader at ${item.path}:${item.line}`);
for (const item of report.evidence.flat_runtime_paths || []) console.error(`evidence: flat runtime material path at ${item.path}:${item.line}`);
console.error("hint: stop feature expansion and add a material/texture pass, or record an explicit --allow-color-only reason for a debug-only prototype.");
process.exit(1);
