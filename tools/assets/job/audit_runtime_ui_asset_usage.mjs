#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

const SIZE_CLASS_MODES = new Map([
  ["large_only", new Set(["desktop", "large"])],
  ["compact_only", new Set(["compact", "mobile_portrait", "mobile_landscape"])],
  ["icon_slot_only", new Set(["icon_slot"])],
  ["status_strip_only", new Set(["status_strip"])],
]);

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(2);
}

function usage() {
  console.error(`usage:
  node tools/assets/job/audit_runtime_ui_asset_usage.mjs --asset-manifest <assets.json> --usage <runtime-usage.json> [--json-output <report.json>] [--report <report.md>]

Checks that runtime UI placements obey generated asset usage_policy:
minimum size, disallowed use tags, and size_class vs layout mode.`);
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

function rectSize(entry, problems, label) {
  if (Array.isArray(entry.size) && entry.size.length === 2) {
    const [width, height] = entry.size;
    if (isPositiveNumber(width) && isPositiveNumber(height)) return { width, height };
    problems.push(`${label} size must be [width,height] positive numbers`);
    return null;
  }
  if (Array.isArray(entry.rect) && entry.rect.length === 4) {
    const [, , width, height] = entry.rect;
    if (isPositiveNumber(width) && isPositiveNumber(height)) return { width, height };
    problems.push(`${label} rect width/height must be positive numbers`);
    return null;
  }
  problems.push(`${label} needs size [width,height] or rect [x,y,width,height]`);
  return null;
}

function validateUsage(asset, entry) {
  const label = `usage ${entry.context || entry.id || "(unknown)"}`;
  const problems = [];
  if (!hasText(entry.id)) problems.push(`${label} needs id`);
  if (!hasText(entry.context)) problems.push(`${label} needs context`);
  const size = rectSize(entry, problems, label);
  const policy = asset?.usage_policy;
  if (!asset) {
    problems.push(`${label} references unknown runtime asset id ${entry.id || "(missing)"}`);
    return { id: entry.id, context: entry.context, status: "fail", problems, size };
  }
  if (!policy || typeof policy !== "object") {
    problems.push(`${label} asset ${entry.id} has no usage_policy`);
  } else {
    const minSize = policy.min_size;
    if (!Array.isArray(minSize) || minSize.length !== 2 || !isPositiveNumber(minSize[0]) || !isPositiveNumber(minSize[1])) {
      problems.push(`${label} asset ${entry.id} usage_policy.min_size is invalid`);
    } else if (size) {
      if (size.width < minSize[0]) problems.push(`${label} width ${size.width} is smaller than min_size width ${minSize[0]}`);
      if (size.height < minSize[1]) problems.push(`${label} height ${size.height} is smaller than min_size height ${minSize[1]}`);
    }
    const usageTags = new Set(Array.isArray(entry.usage_tags) ? entry.usage_tags.filter(hasText) : []);
    if (!Array.isArray(entry.usage_tags)) problems.push(`${label} needs usage_tags array`);
    for (const tag of policy.disallowed_uses || []) {
      if (usageTags.has(tag)) problems.push(`${label} uses disallowed tag ${tag}`);
    }
    const layoutMode = entry.layout_mode;
    const allowedModes = SIZE_CLASS_MODES.get(policy.size_class);
    if (allowedModes && !allowedModes.has(layoutMode)) {
      problems.push(`${label} layout_mode ${layoutMode || "(missing)"} is not allowed for size_class ${policy.size_class}`);
    }
  }
  return {
    id: entry.id,
    context: entry.context,
    layout_mode: entry.layout_mode,
    usage_tags: entry.usage_tags || [],
    size,
    status: problems.length === 0 ? "pass" : "fail",
    problems,
  };
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) usage();
  if (!args["asset-manifest"] || !args.usage) usage();
  if (!existsSync(args["asset-manifest"])) fail(`asset manifest not found: ${args["asset-manifest"]}`);
  if (!existsSync(args.usage)) fail(`runtime usage file not found: ${args.usage}`);

  const manifest = readJson(args["asset-manifest"]);
  const runtimeUsage = readJson(args.usage);
  const assetsById = new Map((manifest.assets || []).map((asset) => [asset.id, asset]));
  const entries = Array.isArray(runtimeUsage.usages) ? runtimeUsage.usages : [];
  const usageResults = entries.map((entry) => validateUsage(assetsById.get(entry.id), entry));
  const problems = usageResults.flatMap((entry) => entry.problems);
  if (entries.length === 0) problems.push("runtime usage file has no usages");

  const report = {
    schema: "game.runtime_ui_asset_usage_audit",
    version: 1,
    asset_manifest: normalizePath(args["asset-manifest"]),
    usage: normalizePath(args.usage),
    verdict: problems.length === 0 ? "pass" : "fail",
    problems,
    usages: usageResults,
  };

  if (args["json-output"]) writeText(args["json-output"], `${JSON.stringify(report, null, 2)}\n`);
  const lines = [
    "# Runtime UI Asset Usage Audit",
    "",
    `asset_manifest: \`${report.asset_manifest}\``,
    `usage: \`${report.usage}\``,
    `verdict: **${report.verdict}**`,
    "",
    "## Usages",
    "",
    ...usageResults.map((entry) => {
      const size = entry.size ? `${entry.size.width}x${entry.size.height}` : "unknown";
      return `- ${entry.status.toUpperCase()} \`${entry.id}\` in \`${entry.context}\` (${entry.layout_mode || "unknown"}, ${size})${entry.problems.length ? `: ${entry.problems.join("; ")}` : ""}`;
    }),
    "",
  ];
  if (args.report) writeText(args.report, `${lines.join("\n")}\n`);
  else console.log(JSON.stringify(report, null, 2));

  if (problems.length > 0) process.exit(1);
}

main();
