#!/usr/bin/env node
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { listArg, parseArgs, stringArg } from "./profile_lib.mjs";

const EXTERNAL_NEEDS = new Set([
  "shared-dashboard",
  "shared-traces",
  "human-review",
  "datasets",
  "experiments",
  "online-evals",
  "production-monitoring",
  "prompt-management",
  "otlp-export",
  "cost-tracking",
]);

const REQUIRED_CAPTURE_FIELDS = [
  "work_item",
  "iteration",
  "phase",
  "category",
  "intent",
  "result",
  "value",
  "duration_ms",
  "tools",
  "commands",
  "context_inputs",
  "evidence",
];

function usage() {
  console.error(`usage:
  node tools/ai_profile/observability_gate.mjs [--need <name> ...]
    [--team single|small|multi]
    [--setup-cost low|medium|high]
    [--sensitivity low|medium|high]
    [--self-host-ok]
    [--pilot-proven]
    [--json-output <file>]

Decides whether the current project should stay on local JSONL profiling or run
a bounded external observability pilot. The local JSONL layer remains the
default source of reflection evidence.`);
  process.exit(2);
}

function normalizeList(values, key) {
  return listArg(values, key)
    .flatMap((value) => String(value).split(","))
    .map((value) => value.trim().toLowerCase())
    .filter(Boolean);
}

function enumValue(values, key, allowed, fallback) {
  const value = stringArg(values, key, fallback).toLowerCase();
  if (!allowed.has(value)) {
    throw new Error(`--${key} must be one of ${[...allowed].join(", ")}`);
  }
  return value;
}

function scoreFriction(setupCost, sensitivity, selfHostOk) {
  let friction = { low: 0, medium: 1, high: 2 }[setupCost] ?? 0;
  if (sensitivity === "medium" && !selfHostOk) friction += 1;
  if (sensitivity === "high" && !selfHostOk) friction += 2;
  return friction;
}

function decide({ needs, team, setupCost, sensitivity, selfHostOk, pilotProven }) {
  const matchedExternalNeeds = needs.filter((need) => EXTERNAL_NEEDS.has(need));
  const unknownNeeds = needs.filter((need) => !EXTERNAL_NEEDS.has(need));
  const pressure =
    matchedExternalNeeds.length +
    (team === "multi" ? 1 : 0) +
    (team === "small" && matchedExternalNeeds.length > 0 ? 0.5 : 0);
  const friction = scoreFriction(setupCost, sensitivity, selfHostOk);

  let recommendation = "local_jsonl_only";
  if (pilotProven && pressure >= 3 && friction <= 2) {
    recommendation = "external_adoption_candidate";
  } else if (pressure >= 2 && friction <= 3) {
    recommendation = "external_pilot";
  }

  const reasons = [];
  if (matchedExternalNeeds.length > 0) {
    reasons.push(`external need(s): ${matchedExternalNeeds.join(", ")}`);
  } else {
    reasons.push("no strong shared observability/eval need was declared");
  }
  reasons.push(`setup cost: ${setupCost}`);
  reasons.push(`data sensitivity: ${sensitivity}${selfHostOk ? " with self-host allowed" : ""}`);
  reasons.push(`team mode: ${team}`);
  if (unknownNeeds.length > 0) {
    reasons.push(`unknown need label(s) ignored: ${unknownNeeds.join(", ")}`);
  }
  if (recommendation === "external_adoption_candidate") {
    reasons.push("pilot is marked proven; adoption can be considered after keeping local export");
  } else if (recommendation === "external_pilot") {
    reasons.push("run a bounded pilot beside local JSONL; do not replace the local profile yet");
  } else {
    reasons.push("stay local until a dashboard, eval, dataset, feedback, OTLP, or production need is concrete");
  }

  return {
    schema_version: 1,
    recommendation,
    keep_local_jsonl: true,
    needs,
    matched_external_needs: matchedExternalNeeds,
    unknown_needs: unknownNeeds,
    team,
    setup_cost: setupCost,
    sensitivity,
    self_host_ok: selfHostOk,
    pilot_proven: pilotProven,
    friction_score: friction,
    external_pressure_score: pressure,
    reasons,
    required_capture_fields: REQUIRED_CAPTURE_FIELDS,
    pilot_rules: [
      "Keep tmp/session_profiles JSONL as the local source of truth.",
      "Use the external tool only for the declared need, not as a general dashboard habit.",
      "A pilot passes only if it reduces reflection/debug time or enables human review/evals that local JSONL cannot provide.",
      "Do not commit raw exported traces, tokens, prompts, or telemetry unless explicitly requested and scrubbed.",
    ],
  };
}

function renderMarkdown(decision) {
  const lines = [];
  lines.push("# AI Observability Tooling Gate");
  lines.push("");
  lines.push(`Recommendation: ${decision.recommendation}`);
  lines.push(`Keep local JSONL: ${decision.keep_local_jsonl ? "yes" : "no"}`);
  lines.push("");
  lines.push("## Inputs");
  lines.push(`- needs: ${decision.needs.length ? decision.needs.join(", ") : "none"}`);
  lines.push(`- team: ${decision.team}`);
  lines.push(`- setup_cost: ${decision.setup_cost}`);
  lines.push(`- sensitivity: ${decision.sensitivity}`);
  lines.push(`- self_host_ok: ${decision.self_host_ok ? "yes" : "no"}`);
  lines.push(`- pilot_proven: ${decision.pilot_proven ? "yes" : "no"}`);
  lines.push("");
  lines.push("## Reasons");
  for (const reason of decision.reasons) lines.push(`- ${reason}`);
  lines.push("");
  lines.push("## Required Capture Fields");
  for (const field of decision.required_capture_fields) lines.push(`- ${field}`);
  lines.push("");
  lines.push("## Pilot Rules");
  for (const rule of decision.pilot_rules) lines.push(`- ${rule}`);
  return `${lines.join("\n")}\n`;
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

let decision;
try {
  decision = decide({
    needs: normalizeList(values, "need"),
    team: enumValue(values, "team", new Set(["single", "small", "multi"]), "single"),
    setupCost: enumValue(values, "setup-cost", new Set(["low", "medium", "high"]), "medium"),
    sensitivity: enumValue(values, "sensitivity", new Set(["low", "medium", "high"]), "medium"),
    selfHostOk: Boolean(values["self-host-ok"]),
    pilotProven: Boolean(values["pilot-proven"]),
  });
} catch (error) {
  console.error(`error: ${error.message}`);
  usage();
}

const jsonOutput = stringArg(values, "json-output", "");
if (jsonOutput) {
  const target = resolve(jsonOutput);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(decision, null, 2)}\n`, "utf8");
}

process.stdout.write(renderMarkdown(decision));
