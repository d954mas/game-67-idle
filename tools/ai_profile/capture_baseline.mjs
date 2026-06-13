#!/usr/bin/env node
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { parseArgs, stringArg, timestamp } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/capture_baseline.mjs <review.json> --label <name> [--output <baseline.review.json>] [--manifest <baseline.manifest.json>] [--force]

Copies a clean review JSON to a stable scratch baseline path so later closeout
or review commands do not overwrite the comparison anchor.`);
  process.exit(2);
}

function safeLabel(raw) {
  return String(raw || "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9._-]+/g, "-")
    .replace(/^-+|-+$/g, "");
}

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function asNumber(value) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : 0;
}

function baselineSummary(review) {
  const currentScope = review.current_scope || {};
  return {
    profile: review.profile || "",
    records: asNumber(review.records),
    current_scope_enabled: currentScope.enabled === true,
    current_scope_records: asNumber(currentScope.records),
    current_scope_findings: asArray(currentScope.findings).length,
    current_scope_missing_context_inputs: asNumber(currentScope.missing_context_inputs),
    current_scope_missing_work_item_records: asNumber(currentScope.missing_work_item_records),
    current_scope_repeated_broad_final_commands: asArray(currentScope.repeated_broad_final_commands).length,
    current_scope_recovered_failed_records: asArray(currentScope.recovered_failed_records).length,
    current_scope_unresolved_failed_records: asArray(currentScope.unresolved_failed_records).length,
    whole_profile_findings: asArray(review.findings).length,
  };
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help) usage();
const reviewFile = positionals[0];
const label = safeLabel(stringArg(values, "label", ""));
if (!reviewFile || !label) usage();

const source = resolve(reviewFile);
let review;
try {
  review = JSON.parse(readFileSync(source, "utf8"));
} catch (error) {
  console.error(`baseline capture failed for ${reviewFile}: ${error.message}`);
  process.exit(1);
}

const outputFile = stringArg(values, "output", resolve("tmp", "session_profiles", "baselines", `${label}.review.json`));
const manifestFile = stringArg(values, "manifest", outputFile.replace(/\.review\.json$/i, ".manifest.json"));
const target = resolve(outputFile);
const manifestTarget = resolve(manifestFile);
if (!values.force && (existsSync(target) || existsSync(manifestTarget))) {
  console.error(`baseline capture refused to overwrite existing artifact: ${existsSync(target) ? target : manifestTarget}`);
  console.error("Pass --force to replace it.");
  process.exit(1);
}

mkdirSync(dirname(target), { recursive: true });
mkdirSync(dirname(manifestTarget), { recursive: true });
copyFileSync(source, target);

const manifest = {
  schema_version: 1,
  label,
  captured_at: timestamp(),
  source_review: source,
  baseline_review: target,
  compare_command: `node tools/ai_profile/compare_reviews.mjs ${target} <current.review.json> --output tmp/session_profiles/${label}.compare.md --json-output tmp/session_profiles/${label}.compare.json`,
  summary: baselineSummary(review),
};
writeFileSync(manifestTarget, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");

process.stdout.write(`# AI Profile Baseline Captured\n\n`);
process.stdout.write(`Label: ${label}\n`);
process.stdout.write(`Source: ${source}\n`);
process.stdout.write(`Baseline: ${target}\n`);
process.stdout.write(`Manifest: ${manifestTarget}\n`);
process.stdout.write(`Current-scope findings: ${manifest.summary.current_scope_findings}\n`);
process.stdout.write(`Compare command: ${manifest.compare_command}\n`);
