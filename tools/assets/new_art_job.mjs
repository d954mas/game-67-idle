#!/usr/bin/env node
// Scaffold a structured generated-art job for agent-readable iteration.
//
// Example:
//   node tools/assets/new_art_job.mjs --id 67-world-characters-v2 --family "next 67 variants" --target gamedesign/meme-evolution/visuals/67-world-first-7-lineup-v1.png
//
// Use --dry-run to preview without writing files.

import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));

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

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function usage() {
  console.log("usage: node tools/assets/new_art_job.mjs --id <job-id> --family <asset family> [--concept meme-evolution] [--target path] [--audience text] [--dry-run]");
}

function writeJson(path, data, dryRun) {
  const fullPath = join(root, path);
  if (existsSync(fullPath)) {
    fail(`refusing to overwrite existing file: ${path}`);
  }
  const text = `${JSON.stringify(data, null, 2)}\n`;
  if (dryRun) {
    console.log(`would write ${path}`);
    console.log(text);
    return;
  }
  mkdirSync(dirname(fullPath), { recursive: true });
  writeFileSync(fullPath, text, "utf8");
  console.log(`wrote ${path}`);
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
const artRequestPath = `gamedesign/${concept}/art_requests/${args.id}.json`;
const cropManifestPath = `gamedesign/${concept}/data/${args.id}-crop_manifest.json`;
const runtimeManifestPath = `gamedesign/${concept}/data/${args.id}-asset_manifest.json`;
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
  must_not_bake: [
    "button labels",
    "resource counters",
    "timer values",
    "tutorial text",
    "debug text",
    "game state values"
  ],
  expected_outputs: {
    source_art: [],
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
    slice_assets: "fill after source sheets are accepted",
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
