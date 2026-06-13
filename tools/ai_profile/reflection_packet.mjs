#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { defaultProfilePath, parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/reflection_packet.mjs [profile.jsonl] [--output <packet.md>] [--json-output <packet.json>]

Builds a compact scratch packet from generated AI profile artifacts so a
retrospective can start from one small evidence file instead of several large
reviews.`);
  process.exit(2);
}

function artifactPath(profilePath, suffix) {
  return join(dirname(profilePath), `${basename(profilePath).replace(/\.jsonl$/i, "")}.${suffix}`);
}

function readJsonIfExists(path) {
  if (!existsSync(path)) return { exists: false, path, value: null, error: "" };
  try {
    return { exists: true, path, value: JSON.parse(readFileSync(path, "utf8")), error: "" };
  } catch (error) {
    return { exists: true, path, value: null, error: error.message };
  }
}

function fileMtimeMs(path) {
  try {
    return existsSync(path) ? statSync(path).mtimeMs : undefined;
  } catch {
    return undefined;
  }
}

function latestBaseline(profilePath) {
  const dir = join(dirname(profilePath), "baselines");
  if (!existsSync(dir)) return null;
  const manifests = [];
  for (const name of readdirSync(dir)) {
    if (!name.endsWith(".manifest.json")) continue;
    const path = join(dir, name);
    const loaded = readJsonIfExists(path);
    if (!loaded.value) continue;
    const capturedMs = Date.parse(loaded.value.captured_at || "");
    manifests.push({
      path,
      label: loaded.value.label || "",
      baseline_review: loaded.value.baseline_review || "",
      compare_command: loaded.value.compare_command || "",
      captured_at: loaded.value.captured_at || "",
      captured_ms: Number.isFinite(capturedMs) ? capturedMs : fileMtimeMs(path) || 0,
    });
  }
  manifests.sort((a, b) => b.captured_ms - a.captured_ms || b.path.localeCompare(a.path));
  return manifests[0] || null;
}

function comparePathFor(profilePath, baseline) {
  if (!baseline) return "";
  const label = baseline.label || basename(baseline.baseline_review || "baseline").replace(/\.review\.json$/i, "");
  return join(dirname(profilePath), `${label}.compare.json`);
}

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function buildPacket(profilePath) {
  const reviewPath = artifactPath(profilePath, "review.json");
  const followupsPath = artifactPath(profilePath, "followups.json");
  const summaryPath = artifactPath(profilePath, "summary.md");
  const review = readJsonIfExists(reviewPath);
  const followups = readJsonIfExists(followupsPath);
  const baseline = latestBaseline(profilePath);
  const comparePath = comparePathFor(profilePath, baseline);
  const compare = comparePath ? readJsonIfExists(comparePath) : { exists: false, path: "", value: null, error: "" };

  const currentScope = review.value?.current_scope || {};
  const suggestions = asArray(followups.value?.suggestions);
  const suppressed = asArray(followups.value?.suppressed_historical_findings);
  const currentRegressions = asArray(compare.value?.current_regressions);
  const improvements = asArray(compare.value?.improvements);

  const readiness = [];
  if (!review.exists) readiness.push("review_missing");
  if (!followups.exists) readiness.push("followups_missing");
  if (baseline && !compare.exists) readiness.push("comparison_missing");
  if (compare.value && currentRegressions.length > 0) readiness.push("current_regressions");
  if (readiness.length === 0) readiness.push("ready");

  return {
    schema_version: 1,
    profile: profilePath,
    generated_at: new Date().toISOString(),
    readiness,
    artifacts: {
      summary_md: summaryPath,
      review_json: reviewPath,
      followups_json: followupsPath,
      baseline_manifest: baseline?.path || "",
      baseline_review: baseline?.baseline_review || "",
      compare_json: comparePath,
    },
    current_scope: {
      enabled: currentScope.enabled === true,
      records: Number(currentScope.records || 0),
      findings: asArray(currentScope.findings),
      suggested_actions: asArray(currentScope.suggested_actions),
    },
    followups: {
      suggestions,
      suppressed_historical_findings: suppressed,
    },
    comparison: {
      exists: compare.exists,
      verdict: compare.value?.verdict || "",
      current_regressions: currentRegressions,
      improvements,
      historical_regressions: asArray(compare.value?.historical_regressions),
    },
    errors: [review.error && `review: ${review.error}`, followups.error && `followups: ${followups.error}`, compare.error && `comparison: ${compare.error}`].filter(Boolean),
  };
}

function renderMarkdown(packet) {
  const lines = [];
  lines.push(`# AI Reflection Packet - ${basename(packet.profile)}`);
  lines.push("");
  lines.push(`Profile: ${packet.profile}`);
  lines.push(`Readiness: ${packet.readiness.join(", ")}`);
  lines.push(`Current scope findings: ${packet.current_scope.findings.length}`);
  lines.push(`Follow-up suggestions: ${packet.followups.suggestions.length}`);
  lines.push(`Suppressed historical findings: ${packet.followups.suppressed_historical_findings.join(", ") || "none"}`);
  lines.push(`Baseline comparison: ${packet.comparison.exists ? packet.comparison.verdict || "unknown" : "missing"}`);
  lines.push(`Current-scope regressions: ${packet.comparison.current_regressions.length}`);
  lines.push("");
  lines.push("## Current Scope");
  if (packet.current_scope.findings.length === 0) {
    lines.push("- no current-scope findings");
  } else {
    for (const finding of packet.current_scope.findings) lines.push(`- ${finding.type || "finding"}: ${finding.message || ""}`);
  }
  for (const action of packet.current_scope.suggested_actions) lines.push(`- action: ${action}`);
  lines.push("");
  lines.push("## Follow-ups");
  if (packet.followups.suggestions.length === 0) {
    lines.push("- none");
  } else {
    for (const suggestion of packet.followups.suggestions) lines.push(`- [${suggestion.priority || "P?"}] ${suggestion.title || "(untitled)"}: ${suggestion.next_action || ""}`);
  }
  lines.push("");
  lines.push("## Baseline Comparison");
  if (!packet.comparison.exists) {
    lines.push("- missing");
  } else if (packet.comparison.current_regressions.length === 0) {
    lines.push(`- verdict: ${packet.comparison.verdict || "unknown"}`);
    lines.push("- current-scope regressions: none");
  } else {
    lines.push(`- verdict: ${packet.comparison.verdict || "unknown"}`);
    for (const item of packet.comparison.current_regressions) lines.push(`- regression: ${item.label || item.key || "unknown"} ${item.baseline ?? "?"} -> ${item.current ?? "?"}`);
  }
  lines.push("");
  lines.push("## Artifacts");
  for (const [name, path] of Object.entries(packet.artifacts)) {
    lines.push(`- ${name}: ${path || "(none)"}`);
  }
  if (packet.errors.length > 0) {
    lines.push("");
    lines.push("## Errors");
    for (const error of packet.errors) lines.push(`- ${error}`);
  }
  return `${lines.join("\n")}\n`;
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help) usage();
const profilePath = resolve(positionals[0] || stringArg(values, "profile", defaultProfilePath()));
const outputFile = stringArg(values, "output", "");
const jsonOutputFile = stringArg(values, "json-output", "");
const packet = buildPacket(profilePath);
const rendered = renderMarkdown(packet);

if (outputFile) {
  const target = resolve(outputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, rendered, "utf8");
}
if (jsonOutputFile) {
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(packet, null, 2)}\n`, "utf8");
}

process.stdout.write(rendered);
