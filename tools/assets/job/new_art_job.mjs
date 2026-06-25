#!/usr/bin/env node
// Scaffold a structured generated-art job for agent-readable iteration.
//
// Example:
//   node tools/assets/job/new_art_job.mjs --id 67-world-characters-v2 --family "next 67 variants" --concept meme-evolution --target gamedesign/projects/meme-evolution/visuals/67-world-first-7-lineup-v1.png
//
// Use --dry-run to preview without writing files.

import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { fail } from "../../lib/cli.mjs";
import { writeJsonFile } from "../../lib/json.mjs";

const root = resolve(fileURLToPath(new URL("../../..", import.meta.url)));

function parseArgs(argv) {
  const out = { targets: [], dryRun: false };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === "--dry-run") {
      out.dryRun = true;
    } else if (arg === "--id") {
      out.id = argv[++i];
    } else if (arg === "--family") {
      out.family = argv[++i];
    } else if (arg === "--concept") {
      out.concept = argv[++i];
    } else if (arg === "--project-dir") {
      out.projectDir = argv[++i];
    } else if (arg === "--target") {
      out.targets.push(argv[++i]);
    } else if (arg === "--audience") {
      out.audience = argv[++i];
    } else if (arg === "--help" || arg === "-h") {
      out.help = true;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return out;
}

function usage() {
  console.log("usage: node tools/assets/job/new_art_job.mjs --id <job-id> --family <asset family> [--concept meme-evolution] [--project-dir gamedesign/projects/meme-evolution] [--target path] [--audience text] [--dry-run]");
}

function writeJson(path, data, dryRun) {
  return writeJsonFile(path, data, { root, onError: fail, dryRun });
}

const args = parseArgs(process.argv.slice(2));
if (args.help) {
  usage();
  process.exit(0);
}
if (!args.id) fail("--id is required");
if (!/^[a-z0-9][a-z0-9_-]*[a-z0-9]$/.test(args.id)) {
  fail("--id must use lowercase letters, numbers, hyphen, or underscore");
}
if (!args.family) fail("--family is required");

const concept = args.concept || "meme-evolution";
const explicitProjectDir = args.projectDir ? args.projectDir.replaceAll("\\", "/").replace(/\/+$/g, "") : "";
const conceptProjectDir = `gamedesign/projects/${concept}`;
const legacyConceptDir = `gamedesign/${concept}`;
const projectDir = explicitProjectDir || (existsSync(join(root, conceptProjectDir)) ? conceptProjectDir : legacyConceptDir);
const artRequestPath = `${projectDir}/art_requests/${args.id}.json`;
const cropManifestPath = `${projectDir}/data/${args.id}-crop_manifest.json`;
const runtimeManifestPath = `${projectDir}/data/${args.id}-asset_manifest.json`;
const runtimeDir = `assets/runtime/${args.id}`;

const artJob = {
  schema: "game.art_job",
  version: 1,
  id: args.id,
  status: "draft",
  asset_family: args.family,
  audience: args.audience || "kids, bright meme casual game",
  runtime_harness: "native PC",
  visual_targets: args.targets,
  candidate_policy: {
    target_count: 4,
    keep_raw_candidates: true,
    reject_to_tmp: true,
    accept_one_or_two_for_runtime: true
  },
  reusable_kinds: ["sprite", "slice9", "icon", "tile", "border", "effect"],
  required_asset_groups: [
    {
      id: "modal_panel_slice9",
      kind: "slice9",
      role: "blank resizable modal/panel background",
      states: ["default"],
      content_policy: "runtime text/icons only inside content safe area",
      stretch_zone_policy: "center and long edges must be plain/repeatable texture; non-stretch ornaments are corner-only or separate overlay sprites",
      decor_overlay_policy: "badges, gems, medallions, locks, banners, and center ornaments must be separate overlay assets with anchors",
      target_preview_sizes: [[240, 160], [360, 240], [520, 320]]
    },
    {
      id: "primary_button_slice9",
      kind: "slice9",
      role: "blank resizable command button backgrounds",
      states: ["default", "pressed", "disabled", "selected"],
      content_policy: "runtime label/icon composed over blank background",
      stretch_zone_policy: "center and long edges must stay structurally boring at the smallest button size",
      decor_overlay_policy: "decorative caps are corner-only; center gems or badges are separate overlay assets",
      target_preview_sizes: [[128, 48], [180, 56], [240, 64]]
    },
    {
      id: "ui_icon_set",
      kind: "icon",
      role: "isolated readable icons with transparent or chroma-key background",
      states: ["default", "locked"],
      size_class: "64px source, preview at gameplay size"
    },
    {
      id: "gameplay_sprite_or_marker",
      kind: "sprite",
      role: "isolated gameplay sprite, marker, or FX with pivot/anchor",
      states: ["default"],
      anchor_policy: "pivot or anchor must be recorded before runtime use"
    }
  ],
  must_not_bake: [
    "button labels",
    "resource counters",
    "timer values",
    "tutorial text",
    "debug text",
    "game state values"
  ],
  generation_contract: {
    source_families: [
      "blank UI kit sheet",
      "isolated icon sheet",
      "world/map/background layer sheet",
      "sprite or FX sheet"
    ],
    final_asset_policy: {
      procedural_art_allowed: "debug_only",
      runtime_final_must_use_generated_or_artist_source: true,
      layered_source_required_for_ui: true,
      notes: "Code may cut, validate, pack, and compose. Code-generated/programmer art is not acceptable as final generated UI unless an explicit exception is recorded."
    },
    source_family_roles: {
      "blank UI kit sheet": "stretchable bases only: panels, button states, bars, frames, dividers",
      "ui decor overlay sheet": "non-stretch ornaments, gems, badges, caps, medallions with anchors",
      "isolated icon sheet": "semantic icons with gutters and transparent/chroma source",
      "world/map/background layer sheet": "non-UI illustration layers or stamps",
      "sprite or FX sheet": "isolated gameplay sprites/effects with pivots"
    },
    prompt_constraints: [
      "no readable text",
      "no fake letters",
      "no fused icons inside button backgrounds",
      "clear gutters between assets",
      "consistent perspective and lighting",
      "transparent or flat chroma-key background for isolated assets",
      "no unique decoration inside slice9 stretch zones"
    ],
    metadata_to_record: [
      "provider or generator",
      "model/workflow",
      "workflow file or workflow json",
      "seed",
      "prompt",
      "negative prompt",
      "source family role",
      "accepted source image path",
      "rejected candidate notes"
    ]
  },
  expected_outputs: {
    source_art: [],
    required_source_families: [
      "blank UI kit sheet",
      "isolated icon sheet",
      "ui decor overlay sheet"
    ],
    generation_records: [],
    crop_manifest: cropManifestPath,
    runtime_manifest: runtimeManifestPath,
    runtime_dir: runtimeDir,
    native_evidence: []
  },
  runtime_composition: {
    buttons: "slice9 background plus runtime text/icon",
    cards: "slice9 card background plus character sprite and runtime state",
    board: "frame, tile, slot, highlight, and effect pieces composed at runtime",
    characters: "separate transparent sprites with pivots and gameplay ids"
  },
  qa_rejects: [
    "random letters",
    "watermarks",
    "labels fused into reusable backgrounds",
    "icons fused into buttons",
    "wrong subject",
    "weak silhouette at gameplay size",
    "opaque chroma-key background",
    "baked board state"
  ],
  commands: {
    plan_source_sheet_prompt: `node tools/assets/job/plan_source_sheet_prompt.mjs --job ${artRequestPath} --source-family "<source family>" --output ${projectDir}/art/prompts/<source-id>-prompt.md --json-output ${projectDir}/art/prompts/<source-id>-prompt.json`,
    new_generation_record: `node tools/assets/job/new_generation_record.mjs --id <source-id> --project-dir ${projectDir} --source-family "<source family>" --source-family-role "<source family role>" --accepted-source <path> --provider <provider> --model <model-or-workflow> --workflow-path <workflow.json> --prompt-packet ${projectDir}/art/prompts/<source-id>-prompt.json --seed <seed> --prompt "<prompt>" --negative-prompt "<negative prompt>"`,
    slice_assets: "fill after source sheets are accepted",
    validate_draft: `node tools/assets/job/validate_art_job.mjs --job ${artRequestPath}`,
    validate_strict: `node tools/assets/job/validate_art_job.mjs --job ${artRequestPath} --strict`,
    validate_final_art: `node tools/assets/job/validate_art_job.mjs --job ${artRequestPath} --final-art`,
    build_pack: "fill after pack builder is wired",
    native_evidence: "fill with DevAPI scenario command"
  }
};

const cropManifest = {
  schema: "game.art_crop_manifest",
  version: 1,
  art_job: artRequestPath,
  output_dir: runtimeDir,
  green_screen: {
    mode: "chroma_key",
    notes: "Use transparent PNGs or a flat chroma-key background for local alpha removal."
  },
  sources: []
};

const assetManifest = {
  schema: "game.asset_manifest",
  version: 1,
  art_job: artRequestPath,
  crop_manifest: cropManifestPath,
  runtime_dir: runtimeDir,
  commands: {
    slice_assets: "",
    build_pack: "",
    native_evidence: ""
  },
  assets: []
};

writeJson(artRequestPath, artJob, args.dryRun);
writeJson(cropManifestPath, cropManifest, args.dryRun);
writeJson(runtimeManifestPath, assetManifest, args.dryRun);
