#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

const CENTER_MODES = new Set(["solid", "plain_texture", "repeatable_texture", "transparent"]);
const EDGE_MODES = new Set(["solid", "plain_texture", "repeatable_texture", "straight_frame", "transparent"]);
const CORNER_MODES = new Set(["none", "simple_fixed", "decorative_fixed"]);
const ORNAMENT_MODES = new Set(["none", "corner_only", "separate_overlay_assets"]);
const SIZE_CLASSES = new Set(["flexible", "large_only", "compact_only", "icon_slot_only", "status_strip_only"]);

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(2);
}

function usage() {
  console.error(`usage:
  node tools/assets/audit_slice9_design_policy.mjs --crop-manifest <crop.json> [--runtime-manifest <assets.json>] [--json-output <report.json>] [--report <report.md>]

Checks slice9 design contracts: stretch zones, non-stretch ornament policy,
usage size class, and runtime-manifest policy parity.`);
  process.exit(2);
}

function parseArgs(argv) {
  const out = {};
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") out.help = true;
    else if (arg.startsWith("--")) {
      const key = arg.slice(2);
      const value = argv[i + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      out[key] = value;
      i += 1;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return out;
}

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function hasText(value) {
  return typeof value === "string" && value.trim().length > 0;
}

function normalizePath(path) {
  return String(path || "").replaceAll("\\", "/").replace(/^\.\//, "");
}

function policyKey(value) {
  return JSON.stringify(value || {});
}

function writeText(path, text) {
  mkdirSync(dirname(resolve(path)), { recursive: true });
  writeFileSync(path, text, "utf8");
}

function isPositiveNumber(value) {
  return typeof value === "number" && Number.isFinite(value) && value > 0;
}

function validateMode(value, allowed, label, problems) {
  if (!hasText(value)) {
    problems.push(`${label} is required`);
    return;
  }
  if (!allowed.has(value)) problems.push(`${label} must be one of ${Array.from(allowed).join(", ")}`);
}

function validateMinSize(value, label, problems) {
  if (!Array.isArray(value) || value.length !== 2 || !isPositiveNumber(value[0]) || !isPositiveNumber(value[1])) {
    problems.push(`${label}.min_size must be [width,height] positive numbers`);
    return null;
  }
  return { width: value[0], height: value[1] };
}

function validateSlice9Policy(entry, label) {
  const problems = [];
  const stretch = entry.stretch_policy;
  if (!stretch || typeof stretch !== "object") {
    problems.push(`${label} needs stretch_policy`);
  } else {
    validateMode(stretch.center, CENTER_MODES, `${label} stretch_policy.center`, problems);
    validateMode(stretch.horizontal_edges, EDGE_MODES, `${label} stretch_policy.horizontal_edges`, problems);
    validateMode(stretch.vertical_edges, EDGE_MODES, `${label} stretch_policy.vertical_edges`, problems);
    validateMode(stretch.corners, CORNER_MODES, `${label} stretch_policy.corners`, problems);
    validateMode(stretch.non_stretch_ornaments, ORNAMENT_MODES, `${label} stretch_policy.non_stretch_ornaments`, problems);
    if (stretch.non_stretch_ornaments === "separate_overlay_assets") {
      const overlayIds = Array.isArray(stretch.overlay_asset_ids) ? stretch.overlay_asset_ids.filter(hasText) : [];
      if (overlayIds.length === 0 && !hasText(stretch.overlay_family)) {
        problems.push(`${label} separate overlay ornaments need overlay_asset_ids or overlay_family`);
      }
    }
  }

  const usage = entry.usage_policy;
  if (!usage || typeof usage !== "object") {
    problems.push(`${label} needs usage_policy`);
  } else {
    validateMode(usage.size_class, SIZE_CLASSES, `${label} usage_policy.size_class`, problems);
    const minSize = validateMinSize(usage.min_size, `${label} usage_policy`, problems);
    if (!Array.isArray(usage.disallowed_uses)) {
      problems.push(`${label} usage_policy.disallowed_uses must be an array`);
    }
    const previewSizes = entry.target_preview_sizes || entry.preview_sizes || [];
    if (minSize && Array.isArray(previewSizes)) {
      for (const size of previewSizes) {
        if (!Array.isArray(size) || size.length !== 2) continue;
        if (isPositiveNumber(size[0]) && size[0] < minSize.width) {
          problems.push(`${label} preview width ${size[0]} is smaller than usage_policy.min_size width ${minSize.width}`);
        }
        if (isPositiveNumber(size[1]) && size[1] < minSize.height) {
          problems.push(`${label} preview height ${size[1]} is smaller than usage_policy.min_size height ${minSize.height}`);
        }
      }
    }
    const roleText = `${entry.role || ""} ${entry.need || ""}`.toLowerCase();
    if (roleText.includes("not suitable") && Array.isArray(usage.disallowed_uses) && usage.disallowed_uses.length === 0) {
      problems.push(`${label} says not suitable in prose but usage_policy.disallowed_uses is empty`);
    }
  }
  return problems;
}

function collectSlice9Crops(crop) {
  const out = [];
  for (const source of crop.sources || []) {
    for (const item of source.crops || []) {
      if (item.kind === "slice9") out.push({ ...item, source_id: source.id, source_path: source.path });
    }
  }
  return out;
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) usage();
  if (!args["crop-manifest"]) usage();

  const cropPath = args["crop-manifest"];
  if (!existsSync(cropPath)) fail(`crop manifest not found: ${cropPath}`);
  const crop = readJson(cropPath);
  const runtimePath = args["runtime-manifest"];
  if (runtimePath && !existsSync(runtimePath)) fail(`runtime manifest not found: ${runtimePath}`);
  const runtime = runtimePath ? readJson(runtimePath) : null;
  const runtimeById = new Map((runtime?.assets || []).filter((asset) => asset.kind === "slice9").map((asset) => [asset.id, asset]));

  const assets = [];
  const allProblems = [];
  for (const cropItem of collectSlice9Crops(crop)) {
    const assetProblems = validateSlice9Policy(cropItem, `slice9 crop ${cropItem.id || "(unknown)"}`);
    const runtimeAsset = runtimeById.get(cropItem.id);
    if (runtime) {
      if (!runtimeAsset) {
        assetProblems.push(`runtime manifest missing slice9 asset ${cropItem.id}`);
      } else {
        assetProblems.push(...validateSlice9Policy(runtimeAsset, `runtime slice9 asset ${cropItem.id}`));
        if (policyKey(runtimeAsset.stretch_policy) !== policyKey(cropItem.stretch_policy)) {
          assetProblems.push(`runtime slice9 asset ${cropItem.id} stretch_policy must match crop manifest`);
        }
        if (policyKey(runtimeAsset.usage_policy) !== policyKey(cropItem.usage_policy)) {
          assetProblems.push(`runtime slice9 asset ${cropItem.id} usage_policy must match crop manifest`);
        }
      }
    }
    allProblems.push(...assetProblems);
    assets.push({
      id: cropItem.id,
      kind: "slice9",
      output: cropItem.output,
      status: assetProblems.length === 0 ? "pass" : "fail",
      problems: assetProblems,
    });
  }

  if (assets.length === 0) allProblems.push("crop manifest has no slice9 crops");

  const report = {
    schema: "game.slice9_design_policy_audit",
    version: 1,
    crop_manifest: normalizePath(cropPath),
    runtime_manifest: runtimePath ? normalizePath(runtimePath) : undefined,
    verdict: allProblems.length === 0 ? "pass" : "fail",
    problems: allProblems,
    assets,
  };

  if (args["json-output"]) writeText(args["json-output"], `${JSON.stringify(report, null, 2)}\n`);
  const lines = [
    "# Slice9 Design Policy Audit",
    "",
    `crop_manifest: \`${report.crop_manifest}\``,
    runtimePath ? `runtime_manifest: \`${report.runtime_manifest}\`` : "",
    `verdict: **${report.verdict}**`,
    "",
    "## Assets",
    "",
    ...assets.map((asset) => `- ${asset.status.toUpperCase()} \`${asset.id}\`${asset.problems.length ? `: ${asset.problems.join("; ")}` : ""}`),
    "",
  ].filter((line) => line !== "");
  if (args.report) writeText(args.report, `${lines.join("\n")}\n`);
  else console.log(JSON.stringify(report, null, 2));

  if (allProblems.length > 0) process.exit(1);
}

main();
