#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { appendRecord, buildRecord, listArg, parseArgs, stringArg } from "./profile_lib.mjs";

const TIERS = ["preflight", "scoped", "final"];

function usage() {
  console.error(`usage:
  node tools/ai_profile/validation_run.mjs --change <kind> [--change <kind> ...] [--file <path> ...] [--risk low|medium|high] [options]
  node tools/ai_profile/validation_run.mjs --plan <validation_plan.json> [options]

options:
  --tier <preflight|scoped|final>  repeatable; default: all tiers from plan
  --profile <path>                profile JSONL path
  --work-item <id>                task/issue/phase id
  --iteration <name>              small iteration or batch label
  --json-output <file>            write run summary JSON
  --dry-run                       print/record no commands; write summary only
  --continue-on-fail              keep running later checks after failures

Runs non-placeholder validation checks from plan_validation.mjs and records each
executed command in the AI profile. Broad/final checks run once at the end of
the selected batch and are skipped after earlier failures unless
--continue-on-fail is explicit.`);
  process.exit(2);
}

function normalizeTier(raw) {
  const tier = String(raw || "").trim().toLowerCase();
  if (!TIERS.includes(tier)) {
    console.error(`unknown tier: ${raw}`);
    usage();
  }
  return tier;
}

function hasPlaceholder(check) {
  return Boolean(check.placeholder) || /<[^>]+>/.test(String(check.command || ""));
}

function shellQuote(value) {
  return String(value).includes(" ") ? JSON.stringify(String(value)) : String(value);
}

function planFromCommand(values) {
  const args = ["tools/ai_profile/plan_validation.mjs", "--json"];
  for (const change of listArg(values, "change")) args.push("--change", change);
  for (const file of listArg(values, "file")) args.push("--file", file);
  const risk = stringArg(values, "risk", "");
  if (risk) args.push("--risk", risk);
  const result = spawnSync(process.execPath, args, {
    cwd: process.cwd(),
    env: process.env,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.stderr) process.stderr.write(result.stderr);
  if (result.status !== 0) {
    if (result.stdout) process.stdout.write(result.stdout);
    process.exit(result.status || 1);
  }
  return JSON.parse(result.stdout);
}

function readPlan(values) {
  const planPath = stringArg(values, "plan", "");
  if (planPath) return JSON.parse(readFileSync(resolve(planPath), "utf8"));
  return planFromCommand(values);
}

function checksByTier(plan, selectedTiers) {
  const checks = Array.isArray(plan.checks) ? plan.checks : [];
  return checks.filter((check) => selectedTiers.has(check.tier));
}

function recordValidation(values, check, result, durationMs) {
  const exitCode = typeof result.status === "number" ? result.status : 1;
  const recordValues = {
    ...values,
    phase: stringArg(values, "phase", "validation"),
    category: "validation",
    intent: `Run validation check ${check.id || check.command}`,
    result: exitCode === 0 ? "pass" : "fail",
    value: check.broad ? "necessary_overhead" : "productive",
    "duration-ms": String(durationMs),
    tool: "ai_profile/validation_run.mjs",
    command: check.command,
    notes: check.why || "",
  };
  const extra = { validation_check_id: check.id || "", validation_tier: check.tier || "", command_exit_code: exitCode };
  if (result.error) extra.command_error = result.error.message;
  const profilePath = stringArg(values, "profile", "");
  return appendRecord(profilePath, buildRecord(recordValues, extra));
}

function runCheck(values, check) {
  console.error(`validation_run: ${check.tier} ${check.id || check.command}`);
  const started = process.hrtime.bigint();
  const result = spawnSync(check.command, {
    cwd: process.cwd(),
    env: process.env,
    shell: true,
    stdio: "inherit",
  });
  const ended = process.hrtime.bigint();
  const durationMs = Number((ended - started) / 1000000n);
  let profile = "";
  try {
    profile = recordValidation(values, check, result, durationMs);
  } catch (error) {
    console.error(`profile validation record failed: ${error.message}`);
  }
  return {
    id: check.id || "",
    tier: check.tier || "",
    command: check.command || "",
    exit_code: typeof result.status === "number" ? result.status : 1,
    result: result.status === 0 ? "pass" : "fail",
    duration_ms: durationMs,
    profile,
  };
}

function writeJson(path, value) {
  if (!path) return;
  const target = resolve(path);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

const selectedTierArgs = listArg(values, "tier");
const selectedTiers = new Set(selectedTierArgs.length > 0 ? selectedTierArgs.map(normalizeTier) : TIERS);
const plan = readPlan(values);
const runnableChecks = checksByTier(plan, selectedTiers);
const dryRun = values["dry-run"] === true;
const continueOnFail = values["continue-on-fail"] === true;
const executed = [];
const skipped = [];
let failed = false;

for (const check of runnableChecks) {
  if (hasPlaceholder(check)) {
    skipped.push({ id: check.id || "", tier: check.tier || "", command: check.command || "", reason: "placeholder command" });
    continue;
  }
  if (failed && !continueOnFail) {
    skipped.push({ id: check.id || "", tier: check.tier || "", command: check.command || "", reason: "previous check failed" });
    continue;
  }
  if (dryRun) {
    skipped.push({ id: check.id || "", tier: check.tier || "", command: check.command || "", reason: "dry run" });
    continue;
  }
  const result = runCheck(values, check);
  executed.push(result);
  if (result.result !== "pass") failed = true;
}

const summary = {
  schema_version: 1,
  plan: {
    risk: plan.risk || "",
    changes: plan.changes || [],
    next_action: plan.next_action || "",
  },
  selected_tiers: [...selectedTiers],
  dry_run: dryRun,
  executed,
  skipped,
  passed: executed.every((item) => item.result === "pass"),
  failed_count: executed.filter((item) => item.result !== "pass").length,
};
summary.exit_code = summary.failed_count > 0 ? executed.find((item) => item.result !== "pass")?.exit_code || 1 : 0;

writeJson(stringArg(values, "json-output", ""), summary);

console.log("# Validation Run");
console.log(`Changes: ${(summary.plan.changes || []).join(", ") || "(from plan)"}`);
console.log(`Risk: ${summary.plan.risk || "unknown"}`);
console.log(`Tiers: ${summary.selected_tiers.join(", ")}`);
console.log(`Executed: ${summary.executed.length}`);
console.log(`Skipped: ${summary.skipped.length}`);
console.log(`Result: ${summary.passed ? "pass" : "fail"}`);
for (const item of summary.executed) {
  console.log(`- ${item.result} ${item.tier} ${item.id || shellQuote(item.command)} (${item.duration_ms}ms)`);
}
for (const item of summary.skipped) {
  console.log(`- skipped ${item.tier} ${item.id || shellQuote(item.command)}: ${item.reason}`);
}

process.exit(summary.exit_code);
