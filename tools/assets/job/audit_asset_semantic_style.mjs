#!/usr/bin/env node
import { dirname, resolve } from "node:path";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";

const ALLOWED_STAGES = new Set(["pre_slice", "post_crop", "pre_runtime", "final_art"]);
const ALLOWED_VERDICTS = new Set(["accept", "reject"]);
const ALLOWED_CHECKS = new Set(["pass", "fail", "unknown"]);

function usage() {
  console.error(`usage:
  node tools/assets/job/audit_asset_semantic_style.mjs --review <review.json> [--json-output <out.json>] [--report <out.md>]

Validates a human/agent semantic and style review for generated UI icons,
decor, sprites, or other runtime assets. The tool does not infer image
semantics itself; it makes the review auditable and blocks accepted assets with
unknown/failing semantic, style, or composability checks.`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = {};
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg.startsWith("--")) {
      const key = arg.slice(2);
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values[key] = value;
      index += 1;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return values;
}

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function readJson(path) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch (error) {
    fail(`cannot read JSON ${path}: ${error.message}`);
  }
}

function hasText(value) {
  return typeof value === "string" && value.trim().length > 0;
}

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function writeArtifact(path, text) {
  mkdirSync(dirname(resolve(path)), { recursive: true });
  writeFileSync(path, text, "utf8");
}

function validateCheck(value, label, problems) {
  if (!ALLOWED_CHECKS.has(value)) {
    problems.push(`${label} must be pass, fail, or unknown`);
  }
}

function audit(review, reviewPath) {
  const problems = [];
  const accepted = [];
  const rejected = [];
  const acceptedStyleGroups = new Set();

  if (review.schema !== "game.asset_semantic_style_review") {
    problems.push("schema must be game.asset_semantic_style_review");
  }
  if (!hasText(review.source_family)) problems.push("source_family is required");
  if (!hasText(review.accepted_visual_target)) problems.push("accepted_visual_target is required");
  if (!ALLOWED_STAGES.has(review.pipeline_stage)) {
    problems.push("pipeline_stage must be pre_slice, post_crop, pre_runtime, or final_art");
  }

  const contract = review.style_contract;
  if (!contract || typeof contract !== "object") {
    problems.push("style_contract object is required");
  } else {
    if (!hasText(contract.style_group)) problems.push("style_contract.style_group is required");
    if (asArray(contract.required_traits).length === 0) {
      problems.push("style_contract.required_traits needs at least one trait");
    }
    if (asArray(contract.forbidden_mixes).length === 0) {
      problems.push("style_contract.forbidden_mixes should name at least one style/semantic mix to avoid");
    }
  }

  const assets = asArray(review.assets);
  if (assets.length === 0) problems.push("assets must contain at least one reviewed asset");

  assets.forEach((asset, index) => {
    const label = `assets[${index}]${asset?.id ? ` ${asset.id}` : ""}`;
    if (!asset || typeof asset !== "object") {
      problems.push(`${label} must be an object`);
      return;
    }
    if (!hasText(asset.id)) problems.push(`${label} needs id`);
    if (!hasText(asset.intended_role)) problems.push(`${label} needs intended_role`);
    if (!hasText(asset.observed_subject)) problems.push(`${label} needs observed_subject`);
    if (!hasText(asset.evidence)) problems.push(`${label} needs evidence path or note`);
    if (!ALLOWED_VERDICTS.has(asset.verdict)) problems.push(`${label} verdict must be accept or reject`);
    validateCheck(asset.semantic_match, `${label} semantic_match`, problems);
    validateCheck(asset.style_match, `${label} style_match`, problems);
    validateCheck(asset.composability, `${label} composability`, problems);

    const styleGroup = asset.style_group;
    if (!hasText(styleGroup)) problems.push(`${label} needs style_group`);
    const problemList = asArray(asset.problems);
    if (asset.verdict === "accept") {
      accepted.push(asset.id || label);
      if (hasText(styleGroup)) acceptedStyleGroups.add(styleGroup);
      for (const field of ["semantic_match", "style_match", "composability"]) {
        if (asset[field] !== "pass") {
          problems.push(`${label} cannot be accepted with ${field}=${asset[field] || "(missing)"}`);
        }
      }
      if (contract?.style_group && styleGroup && styleGroup !== contract.style_group) {
        problems.push(`${label} accepted style_group ${styleGroup} does not match contract ${contract.style_group}`);
      }
    } else if (asset.verdict === "reject") {
      rejected.push(asset.id || label);
      if (problemList.length === 0 && !hasText(asset.rejection_reason)) {
        problems.push(`${label} rejected asset needs problems or rejection_reason`);
      }
    }
  });

  if (acceptedStyleGroups.size > 1) {
    problems.push(`accepted assets mix style groups: ${Array.from(acceptedStyleGroups).sort().join(", ")}`);
  }

  const report = {
    schema: "game.asset_semantic_style_audit",
    version: 1,
    review: reviewPath,
    source_family: review.source_family || null,
    pipeline_stage: review.pipeline_stage || null,
    accepted_asset_ids: accepted,
    rejected_asset_ids: rejected,
    accepted_style_groups: Array.from(acceptedStyleGroups).sort(),
    problems,
    verdict: problems.length === 0 ? "pass" : "fail",
  };
  return report;
}

function markdown(report) {
  const lines = [
    "# Asset Semantic / Style Audit",
    "",
    `Verdict: **${report.verdict}**`,
    `Source family: ${report.source_family || "(missing)"}`,
    `Pipeline stage: ${report.pipeline_stage || "(missing)"}`,
    `Accepted assets: ${report.accepted_asset_ids.length ? report.accepted_asset_ids.join(", ") : "(none)"}`,
    `Rejected assets: ${report.rejected_asset_ids.length ? report.rejected_asset_ids.join(", ") : "(none)"}`,
    "",
    "## Problems",
  ];
  if (report.problems.length === 0) lines.push("- none");
  else report.problems.forEach((problem) => lines.push(`- ${problem}`));
  return `${lines.join("\n")}\n`;
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) usage();
  if (!args.review) usage();

  const reviewPath = resolve(args.review);
  const report = audit(readJson(reviewPath), args.review);
  const json = `${JSON.stringify(report, null, 2)}\n`;
  console.log(json);
  if (args["json-output"]) writeArtifact(args["json-output"], json);
  if (args.report) writeArtifact(args.report, markdown(report));
  process.exit(report.verdict === "pass" ? 0 : 1);
}

main();
