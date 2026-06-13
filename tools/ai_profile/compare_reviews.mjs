#!/usr/bin/env node
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/compare_reviews.mjs <baseline.review.json> <current.review.json> [--output <compare.md>] [--json-output <compare.json>] [--fail-on-regression]

Compares two review.mjs --json-output artifacts. Current-scope regressions are
treated as urgent; whole-profile deltas are reported as historical trend.`);
  process.exit(2);
}

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function asNumber(value) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : 0;
}

function findingTypes(review) {
  return new Set(asArray(review.findings).map((finding) => finding.type).filter(Boolean));
}

function currentScope(review) {
  const scope = review.current_scope || {};
  return {
    enabled: scope.enabled === true,
    records: asNumber(scope.records),
    findings: asArray(scope.findings),
    missing_context_inputs: asNumber(scope.missing_context_inputs),
    missing_work_item_records: asNumber(scope.missing_work_item_records),
    repeated_broad_final_commands: asArray(scope.repeated_broad_final_commands),
    recovered_failed_records: asArray(scope.recovered_failed_records),
    unresolved_failed_records: asArray(scope.unresolved_failed_records),
    low_profile_coverage: scope.low_profile_coverage === true ? 1 : 0,
  };
}

function extractMetrics(review) {
  const scope = currentScope(review);
  const types = findingTypes(review);
  return {
    current_findings: scope.findings.length,
    current_missing_context_inputs: scope.missing_context_inputs,
    current_missing_work_item_records: scope.missing_work_item_records,
    current_repeated_broad_final_commands: scope.repeated_broad_final_commands.length,
    current_recovered_failed_records: scope.recovered_failed_records.length,
    current_unresolved_failed_records: scope.unresolved_failed_records.length,
    current_low_profile_coverage: scope.low_profile_coverage,
    whole_findings: asArray(review.findings).length,
    whole_missing_context_inputs: asArray(review.missing_context_inputs).length,
    whole_repeated_broad_final_commands: asArray(review.repeated_broad_final_commands).length,
    whole_recovered_failed_records: asArray(review.recovered_failed_records).length,
    whole_unresolved_failed_records: asArray(review.unresolved_failed_records).length,
    whole_low_profile_coverage: types.has("low_profile_coverage") ? 1 : 0,
  };
}

const METRICS = [
  ["current_findings", "Current-scope findings", "current"],
  ["current_missing_context_inputs", "Current-scope missing context inputs", "current"],
  ["current_missing_work_item_records", "Current-scope missing work-item records", "current"],
  ["current_repeated_broad_final_commands", "Current-scope repeated broad/final commands", "current"],
  ["current_recovered_failed_records", "Current-scope recovered failed records", "current"],
  ["current_unresolved_failed_records", "Current-scope unresolved failed records", "current"],
  ["current_low_profile_coverage", "Current-scope low coverage flag", "current"],
  ["whole_findings", "Whole-profile findings", "historical"],
  ["whole_missing_context_inputs", "Whole-profile missing context inputs", "historical"],
  ["whole_repeated_broad_final_commands", "Whole-profile repeated broad/final commands", "historical"],
  ["whole_recovered_failed_records", "Whole-profile recovered failed records", "historical"],
  ["whole_unresolved_failed_records", "Whole-profile unresolved failed records", "historical"],
  ["whole_low_profile_coverage", "Whole-profile low coverage flag", "historical"],
];

function compareReviews(baseline, current) {
  const baselineMetrics = extractMetrics(baseline);
  const currentMetrics = extractMetrics(current);
  const deltas = METRICS.map(([key, label, scope]) => {
    const before = asNumber(baselineMetrics[key]);
    const after = asNumber(currentMetrics[key]);
    const delta = after - before;
    return {
      key,
      label,
      scope,
      baseline: before,
      current: after,
      delta,
      direction: delta > 0 ? "worse" : delta < 0 ? "better" : "same",
    };
  });

  const currentRegressions = deltas.filter((item) => item.scope === "current" && item.delta > 0);
  const historicalRegressions = deltas.filter((item) => item.scope === "historical" && item.delta > 0);
  const improvements = deltas.filter((item) => item.delta < 0);
  const verdict = currentRegressions.length > 0
    ? "regressed"
    : improvements.length > 0
      ? "improved"
      : "stable";

  return {
    schema_version: 1,
    baseline_profile: baseline.profile || "",
    current_profile: current.profile || "",
    baseline_scope: {
      enabled: currentScope(baseline).enabled,
      records: currentScope(baseline).records,
    },
    current_scope: {
      enabled: currentScope(current).enabled,
      records: currentScope(current).records,
    },
    verdict,
    current_regressions: currentRegressions,
    historical_regressions: historicalRegressions,
    improvements,
    deltas,
  };
}

function renderMarkdown(baselineFile, currentFile, comparison) {
  const lines = [];
  lines.push(`# AI Profile Baseline Compare`);
  lines.push("");
  lines.push(`Baseline: ${baselineFile}`);
  lines.push(`Current: ${currentFile}`);
  lines.push(`Verdict: ${comparison.verdict}`);
  lines.push(`Baseline scope records: ${comparison.baseline_scope.records}`);
  lines.push(`Current scope records: ${comparison.current_scope.records}`);

  lines.push("");
  lines.push("## Current-Scope Regressions");
  if (comparison.current_regressions.length === 0) {
    lines.push("- none");
  } else {
    for (const item of comparison.current_regressions) {
      lines.push(`- ${item.label}: ${item.baseline} -> ${item.current} (${item.delta >= 0 ? "+" : ""}${item.delta})`);
    }
  }

  lines.push("");
  lines.push("## Improvements");
  if (comparison.improvements.length === 0) {
    lines.push("- none");
  } else {
    for (const item of comparison.improvements) {
      lines.push(`- ${item.label}: ${item.baseline} -> ${item.current} (${item.delta})`);
    }
  }

  lines.push("");
  lines.push("## Historical Whole-Profile Regressions");
  if (comparison.historical_regressions.length === 0) {
    lines.push("- none");
  } else {
    for (const item of comparison.historical_regressions) {
      lines.push(`- ${item.label}: ${item.baseline} -> ${item.current} (+${item.delta})`);
    }
  }

  lines.push("");
  lines.push("## All Deltas");
  for (const item of comparison.deltas) {
    lines.push(`- [${item.scope}] ${item.label}: ${item.baseline} -> ${item.current} (${item.delta >= 0 ? "+" : ""}${item.delta})`);
  }

  return `${lines.join("\n")}\n`;
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help) usage();
const baselineFile = positionals[0];
const currentFile = positionals[1];
if (!baselineFile || !currentFile) usage();
const outputFile = stringArg(values, "output", "");
const jsonOutputFile = stringArg(values, "json-output", "");

let baseline;
let current;
try {
  baseline = JSON.parse(readFileSync(baselineFile, "utf8"));
  current = JSON.parse(readFileSync(currentFile, "utf8"));
} catch (error) {
  console.error(`profile compare failed: ${error.message}`);
  process.exit(1);
}

const comparison = compareReviews(baseline, current);
const rendered = renderMarkdown(basename(baselineFile), basename(currentFile), comparison);

if (outputFile) {
  const target = resolve(outputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, rendered, "utf8");
}
if (jsonOutputFile) {
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(comparison, null, 2)}\n`, "utf8");
}

process.stdout.write(rendered);
if (values["fail-on-regression"] && comparison.current_regressions.length > 0) process.exit(1);
