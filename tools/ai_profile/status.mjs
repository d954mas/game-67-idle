#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { defaultProfilePath, parseArgs, readProfileScope, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/status.mjs [--profile <profile.jsonl>] [--json-output <status.json>]

Reports current AI profile health without appending records or generating a
closeout bundle. Use it mid-session to decide the next profiling action.`);
  process.exit(2);
}

function parseProfile(file) {
  if (!existsSync(file)) return { records: [], errors: [], exists: false };
  const text = readFileSync(file, "utf8");
  const records = [];
  const errors = [];
  for (const [index, rawLine] of text.split(/\r?\n/).entries()) {
    const line = rawLine.trim();
    if (!line) continue;
    try {
      records.push({ ...JSON.parse(line), __line: index + 1 });
    } catch (error) {
      errors.push(`line ${index + 1}: invalid JSON: ${error.message}`);
    }
  }
  return { records, errors, exists: true };
}

function eventTime(record) {
  const parsed = Date.parse(record.ts || "");
  return Number.isFinite(parsed) ? parsed : undefined;
}

function formatMs(ms) {
  if (!Number.isFinite(ms) || ms <= 0) return "0s";
  const seconds = ms / 1000;
  if (seconds < 90) return `${seconds.toFixed(1)}s`;
  const minutes = seconds / 60;
  if (minutes < 90) return `${minutes.toFixed(1)}m`;
  return `${(minutes / 60).toFixed(2)}h`;
}

function formatPercent(value) {
  if (!Number.isFinite(value)) return "unknown";
  return `${(value * 100).toFixed(1)}%`;
}

function artifactPath(profilePath, suffix) {
  const parsed = basename(profilePath).replace(/\.jsonl$/i, "");
  return join(dirname(profilePath), `${parsed}.${suffix}`);
}

function coverageStats(records) {
  const intervals = [];
  for (const record of records) {
    const endMs = eventTime(record);
    if (endMs === undefined) continue;
    const durationMs = Math.max(0, Number(record.duration_ms || 0));
    intervals.push({ start_ms: endMs - durationMs, end_ms: endMs });
  }
  intervals.sort((a, b) => a.start_ms - b.start_ms || a.end_ms - b.end_ms);
  if (intervals.length === 0) {
    return { wall_clock_span_ms: 0, merged_profiled_ms: 0, coverage_ratio: undefined };
  }
  const merged = [];
  for (const interval of intervals) {
    const last = merged[merged.length - 1];
    if (!last || interval.start_ms > last.end_ms) {
      merged.push({ ...interval });
    } else if (interval.end_ms > last.end_ms) {
      last.end_ms = interval.end_ms;
    }
  }
  const firstMs = intervals[0].start_ms;
  const lastMs = intervals.reduce((max, interval) => Math.max(max, interval.end_ms), intervals[0].end_ms);
  const wallClockSpanMs = Math.max(0, lastMs - firstMs);
  const mergedProfiledMs = merged.reduce((sum, interval) => sum + Math.max(0, interval.end_ms - interval.start_ms), 0);
  return {
    wall_clock_span_ms: wallClockSpanMs,
    merged_profiled_ms: mergedProfiledMs,
    coverage_ratio: wallClockSpanMs > 0 ? Math.min(1, mergedProfiledMs / wallClockSpanMs) : undefined,
  };
}

function bundleStatus(profilePath) {
  const artifacts = [
    ["summary", artifactPath(profilePath, "summary.md")],
    ["review", artifactPath(profilePath, "review.md")],
    ["review_json", artifactPath(profilePath, "review.json")],
    ["followups", artifactPath(profilePath, "followups.md")],
    ["followups_json", artifactPath(profilePath, "followups.json")],
  ].map(([name, path]) => ({ name, path, exists: existsSync(path) }));
  return {
    complete: artifacts.every((artifact) => artifact.exists),
    artifacts,
  };
}

function latestRecord(records) {
  return [...records].sort((a, b) => (eventTime(b) || 0) - (eventTime(a) || 0))[0];
}

function normalizeCommand(command) {
  return String(command || "").replaceAll("\\", "/").replace(/\s+/g, " ").trim();
}

function commandKeys(record) {
  return (record.commands || []).map(normalizeCommand).filter(Boolean);
}

function classifyFailedRecords(records) {
  const passedLater = new Map();
  let recovered = 0;
  let unresolved = 0;
  for (const record of [...records].sort((a, b) => b.__line - a.__line)) {
    const keys = commandKeys(record);
    if (record.result === "pass") {
      for (const key of keys) passedLater.set(key, record);
      continue;
    }
    if (record.result !== "fail") continue;
    if (keys.some((key) => passedLater.has(key))) {
      recovered += 1;
    } else {
      unresolved += 1;
    }
  }
  return { recovered, unresolved };
}

function buildStatus(profilePath) {
  const parsed = parseProfile(profilePath);
  const records = parsed.records;
  const closeoutSeen = records.some((record) => record.phase === "session_closeout");
  const missingWorkItem = records.filter((record) => !record.work_item).length;
  const missingContextInputs = records.filter((record) => record.context_risk && record.context_risk !== "low" && !(record.context_inputs || []).length).length;
  const failedRecords = records.filter((record) => record.result === "fail").length;
  const failedClassification = classifyFailedRecords(records);
  const coverage = coverageStats(records);
  const bundle = bundleStatus(profilePath);
  const scope = readProfileScope();
  const latest = latestRecord(records);
  const lowCoverage = coverage.wall_clock_span_ms >= 30 * 60 * 1000 && Number.isFinite(coverage.coverage_ratio) && coverage.coverage_ratio < 0.25;

  let nextAction = "Use this profile as baseline; no urgent profiling action detected.";
  const scopeReady = scope.valid && Boolean(scope.work_item);

  if (!parsed.exists) {
    nextAction = "Start profiling with `node tools/ai_profile/start.mjs --work-item <id> --iteration <name>`.";
  } else if (parsed.errors.length > 0) {
    nextAction = "Fix invalid JSONL lines before using this profile for reflection.";
  } else if (records.length === 0) {
    nextAction = "Start the first checkpoint with `node tools/ai_profile/start.mjs --work-item <id> --iteration <name>`.";
  } else if (failedClassification.unresolved > 0) {
    nextAction = "Resolve or explain unresolved failed profile records before trusting this profile for reflection.";
  } else if (!scopeReady) {
    nextAction = "Start or reset current scope with `node tools/ai_profile/start.mjs --work-item <id> --iteration <name>`.";
  } else if (missingContextInputs > 0) {
    nextAction = "Use context.mjs for medium/high local context reads so context_inputs are measured.";
  } else if (lowCoverage) {
    nextAction = "Add sparse event.mjs checkpoints during long manual/research/design stretches.";
  } else if (!closeoutSeen) {
    nextAction = "At session end, run closeout.mjs to generate the reflection bundle.";
  } else if (!bundle.complete) {
    nextAction = "Run closeout.mjs again, or rerun review/followups if the bundle was intentionally skipped.";
  }

  return {
    schema_version: 1,
    profile: profilePath,
    exists: parsed.exists,
    valid: parsed.errors.length === 0,
    errors: parsed.errors,
    records: records.length,
    latest_record: latest ? {
      ts: latest.ts || "",
      line: latest.__line,
      phase: latest.phase || "",
      category: latest.category || "",
      intent: latest.intent || "",
      work_item: latest.work_item || "",
      iteration: latest.iteration || "",
    } : null,
    scope,
    closeout_seen: closeoutSeen,
    bundle,
    work_item_coverage: {
      missing_records: missingWorkItem,
      coverage_ratio: records.length > 0 ? (records.length - missingWorkItem) / records.length : undefined,
    },
    missing_context_inputs: missingContextInputs,
    failed_records: failedRecords,
    recovered_failed_records: failedClassification.recovered,
    unresolved_failed_records: failedClassification.unresolved,
    wall_clock_coverage: coverage,
    low_profile_coverage: lowCoverage,
    next_action: nextAction,
  };
}

function renderMarkdown(status) {
  const lines = [];
  lines.push(`# AI Profile Status - ${basename(status.profile)}`);
  lines.push("");
  lines.push(`Profile: ${status.profile}`);
  lines.push(`Exists: ${status.exists ? "yes" : "no"}`);
  lines.push(`Valid JSONL: ${status.valid ? "yes" : "no"}`);
  lines.push(`Records: ${status.records}`);
  if (status.latest_record) {
  lines.push(`Latest: line ${status.latest_record.line} ${status.latest_record.ts} [${status.latest_record.phase}/${status.latest_record.category}] ${status.latest_record.intent}`);
  } else {
    lines.push("Latest: none");
  }
  lines.push(`Closeout event: ${status.closeout_seen ? "yes" : "no"}`);
  lines.push(`Scope: ${status.scope.exists ? "set" : "none"}${status.scope.work_item ? ` (${status.scope.work_item}${status.scope.iteration ? `/${status.scope.iteration}` : ""})` : ""}`);
  lines.push(`Bundle complete: ${status.bundle.complete ? "yes" : "no"}`);
  lines.push(`Work-item coverage: ${formatPercent(status.work_item_coverage.coverage_ratio)} (${status.work_item_coverage.missing_records} missing)`);
  lines.push(`Missing context inputs: ${status.missing_context_inputs}`);
  lines.push(`Failed records: ${status.failed_records} (${status.recovered_failed_records} recovered, ${status.unresolved_failed_records} unresolved)`);
  lines.push(`Wall-clock coverage: ${formatPercent(status.wall_clock_coverage.coverage_ratio)} (${formatMs(status.wall_clock_coverage.merged_profiled_ms)} / ${formatMs(status.wall_clock_coverage.wall_clock_span_ms)})`);
  lines.push("");
  lines.push("## Bundle Artifacts");
  for (const artifact of status.bundle.artifacts) {
    lines.push(`- ${artifact.exists ? "yes" : "no"} ${artifact.name}: ${artifact.path}`);
  }
  if (status.errors.length > 0) {
    lines.push("");
    lines.push("## Errors");
    for (const error of status.errors) lines.push(`- ${error}`);
  }
  lines.push("");
  lines.push("## Next Action");
  lines.push(`- ${status.next_action}`);
  return `${lines.join("\n")}\n`;
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

const profilePath = resolve(stringArg(values, "profile", defaultProfilePath()));
const jsonOutputFile = stringArg(values, "json-output", "");
const status = buildStatus(profilePath);
const rendered = renderMarkdown(status);

if (jsonOutputFile) {
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(status, null, 2)}\n`, "utf8");
}
process.stdout.write(rendered);
