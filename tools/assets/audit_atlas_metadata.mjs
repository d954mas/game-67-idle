#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

const TRIM_MODES = new Set(["none", "alpha", "source_rect", "manual"]);

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(2);
}

function usage() {
  console.error(`usage:
  node tools/assets/audit_atlas_metadata.mjs --asset-manifest <assets.json> [--json-output <report.json>] [--report <report.md>]

Checks runtime UI atlas metadata: pack groups, trim/bleed/extrude/padding
policy, scale variants, alias links, and slice9 rotation safety.`);
  process.exit(2);
}

function parseArgs(argv) {
  const out = {};
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") out.help = true;
    else if (arg.startsWith("--")) {
      const key = arg.slice(2);
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      out[key] = value;
      index += 1;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return out;
}

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function writeText(path, text) {
  mkdirSync(dirname(resolve(path)), { recursive: true });
  writeFileSync(path, text, "utf8");
}

function normalizePath(path) {
  return String(path || "").replaceAll("\\", "/").replace(/^\.\//, "");
}

function hasText(value) {
  return typeof value === "string" && value.trim().length > 0;
}

function isPositiveNumber(value) {
  return typeof value === "number" && Number.isFinite(value) && value > 0;
}

function isNonNegativeNumber(value) {
  return typeof value === "number" && Number.isFinite(value) && value >= 0;
}

function validateSize(value, label, problems) {
  if (!Array.isArray(value) || value.length !== 2 || !isPositiveNumber(value[0]) || !isPositiveNumber(value[1])) {
    problems.push(`${label} must be [width,height] positive numbers`);
  }
}

function validateRect(value, label, problems) {
  if (!Array.isArray(value) || value.length !== 4) {
    problems.push(`${label} must be [x,y,width,height]`);
    return;
  }
  const [x, y, width, height] = value;
  if (!isNonNegativeNumber(x) || !isNonNegativeNumber(y) || !isPositiveNumber(width) || !isPositiveNumber(height)) {
    problems.push(`${label} must use non-negative x/y and positive width/height`);
  }
}

function validateAtlasPolicy(asset, assetIds) {
  const id = asset.id || "(unknown)";
  const problems = [];
  if (!hasText(asset.id)) problems.push("asset needs id");
  if (!hasText(asset.kind)) problems.push(`asset ${id} needs kind`);
  if (!hasText(asset.path)) problems.push(`asset ${id} needs path`);
  if (!hasText(asset.pack_group)) problems.push(`asset ${id} needs pack_group`);
  if (!hasText(asset.source_crop)) problems.push(`asset ${id} needs source_crop`);
  validateSize(asset.original_size, `asset ${id} original_size`, problems);
  validateRect(asset.trim_rect, `asset ${id} trim_rect`, problems);

  const policy = asset.atlas_policy;
  if (!policy || typeof policy !== "object") {
    problems.push(`asset ${id} needs atlas_policy`);
  } else {
    if (!hasText(policy.trim_mode)) {
      problems.push(`asset ${id} atlas_policy.trim_mode is required`);
    } else if (!TRIM_MODES.has(policy.trim_mode)) {
      problems.push(`asset ${id} atlas_policy.trim_mode must be one of ${Array.from(TRIM_MODES).join(", ")}`);
    }
    if (policy.alpha_bleed !== true) problems.push(`asset ${id} atlas_policy.alpha_bleed must be true`);
    if (policy.premultiply_alpha !== true) problems.push(`asset ${id} atlas_policy.premultiply_alpha must be true`);
    if (!isPositiveNumber(policy.extrude) || policy.extrude < 1) problems.push(`asset ${id} atlas_policy.extrude must be >= 1`);
    if (!isPositiveNumber(policy.shape_padding) || policy.shape_padding < 2) problems.push(`asset ${id} atlas_policy.shape_padding must be >= 2`);
    if (!isNonNegativeNumber(policy.border_padding)) problems.push(`asset ${id} atlas_policy.border_padding must be >= 0`);
    if (!hasText(policy.scale_variant)) problems.push(`asset ${id} atlas_policy.scale_variant is required`);
    if (asset.kind === "slice9" && policy.allow_rotation !== false) {
      problems.push(`asset ${id} slice9 atlas_policy.allow_rotation must be false`);
    }
    if (asset.kind === "slice9" && policy.trim_preserves_slice9 !== true) {
      problems.push(`asset ${id} slice9 atlas_policy.trim_preserves_slice9 must be true`);
    }
    if (asset.kind !== "slice9" && policy.allow_rotation === undefined) {
      problems.push(`asset ${id} atlas_policy.allow_rotation is required`);
    }
  }

  if (asset.alias_of !== undefined) {
    if (!hasText(asset.alias_of)) {
      problems.push(`asset ${id} alias_of must be a non-empty asset id`);
    } else if (asset.alias_of === asset.id) {
      problems.push(`asset ${id} alias_of must not reference itself`);
    } else if (!assetIds.has(asset.alias_of)) {
      problems.push(`asset ${id} alias_of references missing asset ${asset.alias_of}`);
    }
  }

  return problems;
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) usage();
  if (!args["asset-manifest"]) usage();
  const manifestPath = args["asset-manifest"];
  if (!existsSync(manifestPath)) fail(`asset manifest not found: ${manifestPath}`);
  const manifest = readJson(manifestPath);
  const assets = Array.isArray(manifest.assets) ? manifest.assets : [];
  const assetIds = new Set(assets.map((asset) => String(asset.id || "")).filter((id) => id.length > 0));
  const assetReports = assets.map((asset) => {
    const problems = validateAtlasPolicy(asset, assetIds);
    return {
      id: asset.id,
      kind: asset.kind,
      pack_group: asset.pack_group,
      status: problems.length === 0 ? "pass" : "fail",
      problems,
    };
  });
  const problems = assetReports.flatMap((asset) => asset.problems);
  if (manifest.schema !== "game.asset_manifest") problems.push("asset manifest schema must be game.asset_manifest");
  if (assets.length === 0) problems.push("asset manifest has no assets");

  const report = {
    schema: "game.atlas_metadata_audit",
    version: 1,
    asset_manifest: normalizePath(manifestPath),
    verdict: problems.length === 0 ? "pass" : "fail",
    problems,
    assets: assetReports,
  };

  if (args["json-output"]) writeText(args["json-output"], `${JSON.stringify(report, null, 2)}\n`);
  const lines = [
    "# Atlas Metadata Audit",
    "",
    `asset_manifest: \`${report.asset_manifest}\``,
    `verdict: **${report.verdict}**`,
    "",
    "## Assets",
    "",
    ...assetReports.map((asset) => `- ${asset.status.toUpperCase()} \`${asset.id}\` (${asset.pack_group || "no pack_group"})${asset.problems.length ? `: ${asset.problems.join("; ")}` : ""}`),
    "",
  ];
  if (args.report) writeText(args.report, `${lines.join("\n")}`);
  else console.log(JSON.stringify(report, null, 2));
  if (problems.length > 0) process.exit(1);
}

main();
