#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const toolDir = dirname(fileURLToPath(import.meta.url));

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function usage() {
  console.log(`usage:
  node tools/assets/job/plan_missing_source_family_prompts.mjs --job <art-job.json> [--coverage-audit <audit.json>] --output-dir <dir> [--key-color #00ff00] [--force]

Creates prompt packets for required source families that do not have a passing
final-accepted generation record yet.`);
}

function parseArgs(argv) {
  const values = {};
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg === "--force") values.force = true;
    else if (arg.startsWith("--")) {
      const key = arg.slice(2).replaceAll("-", "_");
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

function readJson(path) {
  try {
    return JSON.parse(readFileSync(resolve(path), "utf8"));
  } catch (error) {
    fail(`cannot read JSON ${path}: ${error.message}`);
  }
}

function hasText(value) {
  return typeof value === "string" && value.trim().length > 0;
}

function normalize(value) {
  return String(value || "").trim().toLowerCase().replace(/\s+/g, " ");
}

function slug(value) {
  return normalize(value).replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "") || "source-family";
}

function unique(items) {
  const out = [];
  const seen = new Set();
  for (const item of items) {
    if (!hasText(item)) continue;
    const key = normalize(item);
    if (seen.has(key)) continue;
    seen.add(key);
    out.push(String(item).trim());
  }
  return out;
}

function requiredFamilies(job) {
  return unique(
    job.expected_outputs?.required_source_families ??
      job.generation_contract?.final_source_families_required ??
      job.generation_contract?.required_source_families ??
      []
  );
}

function missingFamiliesFromAudit(audit, required) {
  if (!audit || audit.verdict === "pass") return [];
  const passing = new Set(
    (audit.records || [])
      .filter((record) => record.status === "pass" && hasText(record.source_family))
      .map((record) => normalize(record.source_family))
  );
  return required.filter((family) => !passing.has(normalize(family)));
}

function runPlanner(jobPath, family, markdown, json, args) {
  const plannerArgs = [
    resolve(toolDir, "plan_source_sheet_prompt.mjs"),
    "--job",
    jobPath,
    "--source-family",
    family,
    "--output",
    markdown,
    "--json-output",
    json,
  ];
  if (args.key_color) plannerArgs.push("--key-color", args.key_color);
  if (args.force) plannerArgs.push("--force");
  const result = spawnSync(process.execPath, plannerArgs, {
    cwd: process.cwd(),
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.status !== 0) {
    fail(`prompt planner failed for ${family}:\n${result.stdout}${result.stderr}`);
  }
}

const args = parseArgs(process.argv.slice(2));
if (args.help) {
  usage();
  process.exit(0);
}
for (const key of ["job", "output_dir"]) {
  if (!args[key]) fail(`--${key.replaceAll("_", "-")} is required`);
}

const job = readJson(args.job);
const required = requiredFamilies(job);
if (required.length === 0) fail("job has no required source families");
const coverage = args.coverage_audit && existsSync(resolve(args.coverage_audit)) ? readJson(args.coverage_audit) : null;
const missing = coverage ? missingFamiliesFromAudit(coverage, required) : required;
if (missing.length === 0) {
  console.log("ok: no missing source families");
  process.exit(0);
}

mkdirSync(resolve(args.output_dir), { recursive: true });
const packets = [];
for (const family of missing) {
  const id = `${job.id || "art-job"}-${slug(family)}-prompt`;
  const markdown = `${args.output_dir.replaceAll("\\", "/").replace(/\/+$/g, "")}/${id}.md`;
  const json = `${args.output_dir.replaceAll("\\", "/").replace(/\/+$/g, "")}/${id}.json`;
  runPlanner(args.job, family, markdown, json, args);
  packets.push({
    source_family: family,
    markdown,
    json,
    generation_record_command: `node tools/assets/job/new_generation_record.mjs --id <accepted-source-id> --project-dir <project-dir> --source-family "${family}" --source-family-role "<accepted role>" --accepted-source <path> --provider <provider> --model <model-or-workflow> --workflow-path <workflow.json> --prompt-packet ${json} --seed <seed> --prompt "<prompt>" --negative-prompt "<negative prompt>"`,
  });
}

const queuePath = `${args.output_dir.replaceAll("\\", "/").replace(/\/+$/g, "")}/${job.id || "art-job"}-source-family-prompt-queue.json`;
const queue = {
  schema: "game.source_family_prompt_queue",
  version: 1,
  art_job: args.job.replaceAll("\\", "/"),
  coverage_audit: args.coverage_audit ? args.coverage_audit.replaceAll("\\", "/") : undefined,
  missing_source_families: missing,
  packets,
};
writeFileSync(resolve(queuePath), `${JSON.stringify(queue, null, 2)}\n`, "utf8");
console.log(`wrote ${queuePath}`);
