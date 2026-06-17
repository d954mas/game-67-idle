#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import {
  defaultProfilePath,
  latestSessionProfilePath,
  listSessionProfiles,
  parseArgs,
  readProfileScope,
  stringArg,
  todaySessionProfiles,
} from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/status.mjs [--profile <profile.jsonl>] [--session <id>] [--all] [--json-output <status.json>] [--verbose] [--require-review-usable|--require-current-scope-usable]

Default reads the ACTIVE session log (newest tmp/session_profiles/sessions/*.jsonl);
--all aggregates today's session logs (+ the legacy daily file); --session <id>
picks one session; --profile <p> reads an explicit file.

Reports current AI profile health without appending records. Default output is
a short passive diagnostic; --verbose shows the full per-record breakdown.

Use --require-current-scope-usable before AI workflow/profiler review handoff
so old low-coverage history does not hide an unmeasured current slice.`);
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

/* Pair each tool_call_start with its following tool_call_result (sequential per
 * session) to derive the command's own duration_ms, then DROP the start records
 * so counts/coverage stay clean. Lets "Slowest Recorded Work" surface the tools
 * that actually take time. Backward-compatible: files with no start events
 * (PreToolUse not wired) are unchanged. */
function attachDurations(records) {
  const out = [];
  let pending = null;
  for (const record of records) {
    if (record.event_type === "tool_call_start") {
      pending = record;
      continue;
    }
    if (record.event_type === "tool_call_result" && pending) {
      const resultCmd = (record.commands && record.commands[0]) || "";
      const startCmd = (pending.commands && pending.commands[0]) || "";
      const sameSession = (record.session_id || "") === (pending.session_id || "");
      if (resultCmd === startCmd && sameSession) {
        const delta = Date.parse(record.ts) - Date.parse(pending.ts);
        if (Number.isFinite(delta) && delta >= 0) record.duration_ms = delta;
      }
      pending = null;
    }
    out.push(record);
  }
  return out;
}

/* Merge several per-session logs into one time-ordered record set. */
function parseProfiles(files) {
  const records = [];
  const errors = [];
  let exists = false;
  for (const file of files) {
    const parsed = parseProfile(file);
    if (parsed.exists) exists = true;
    records.push(...parsed.records);
    for (const error of parsed.errors) errors.push(`${file}: ${error}`);
  }
  records.sort((a, b) => String(a.ts).localeCompare(String(b.ts)));
  return { records: attachDurations(records), errors, exists };
}

/* Resolve which log file(s) `status` reads:
 *   --profile <p>  -> that file
 *   --session <id> -> the matching per-session log (any day)
 *   --all          -> all of today's per-session logs + the legacy daily file
 *   (default)      -> the active session (latest per-session log); legacy daily
 *                     file as a fallback when no per-session logs exist yet. */
function resolveProfilePaths(values) {
  const explicit = stringArg(values, "profile", "");
  if (explicit) return [resolve(explicit)];

  const sessionId = stringArg(values, "session", "");
  if (sessionId) {
    const matches = listSessionProfiles().filter((path) => basename(path).includes(sessionId));
    if (matches.length > 0) return matches;
  }

  if (values.all === true) {
    const all = [...todaySessionProfiles()];
    const daily = defaultProfilePath();
    if (existsSync(daily)) all.push(daily);
    return all.length > 0 ? all : [daily];
  }

  const latest = latestSessionProfilePath();
  return [latest || defaultProfilePath()];
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

function coverageStats(records, gapThresholdMs = 5 * 60 * 1000, idleGapMs = 60 * 60 * 1000) {
  const intervals = [];
  for (const record of records) {
    const endMs = eventTime(record);
    if (endMs === undefined) continue;
    const durationMs = Math.max(0, Number(record.duration_ms || 0));
    intervals.push({
      start_ms: endMs - durationMs,
      end_ms: endMs,
      line: record.__line,
      intent: record.intent || "",
    });
  }
  intervals.sort((a, b) => a.start_ms - b.start_ms || a.end_ms - b.end_ms);
  if (intervals.length === 0) {
    return { wall_clock_span_ms: 0, effective_span_ms: 0, active_ms: 0, idle_ms: 0, merged_profiled_ms: 0, coverage_ratio: undefined, largest_gaps: [] };
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
  /* Records carry no per-call duration, so "active" time is the sum of SHORT
   * gaps between consecutive events (hands-on work). Long gaps split into idle
   * (>= idleGapMs, e.g. overnight) vs uncaptured (between). Coverage is measured
   * against the IDLE-EXCLUDED span, so an overnight pause no longer makes a
   * fully-recorded work session look like "low coverage" (retro 2026-06-17). */
  let activeMs = 0;
  let idleMs = 0;
  for (let index = 1; index < merged.length; index += 1) {
    const gap = merged[index].start_ms - merged[index - 1].end_ms;
    if (gap <= 0) continue;
    if (gap < gapThresholdMs) activeMs += gap;
    else if (gap >= idleGapMs) idleMs += gap;
  }
  /* Add command execution time (interval widths) when durations are present; it
   * is 0 without them, so the short-gap proxy still stands alone. */
  activeMs += merged.reduce((sum, interval) => sum + Math.max(0, interval.end_ms - interval.start_ms), 0);
  const effectiveSpanMs = Math.max(0, wallClockSpanMs - idleMs);
  const mergedProfiledMs = activeMs;
  const largestGaps = [];
  for (let index = 1; index < merged.length; index += 1) {
    const previous = merged[index - 1];
    const current = merged[index];
    const durationMs = current.start_ms - previous.end_ms;
    if (durationMs >= gapThresholdMs) {
      largestGaps.push({
        start_ts: new Date(previous.end_ms).toISOString(),
        end_ts: new Date(current.start_ms).toISOString(),
        duration_ms: durationMs,
        previous_line: previous.line,
        next_line: current.line,
        previous_intent: previous.intent,
        next_intent: current.intent,
      });
    }
  }
  largestGaps.sort((a, b) => b.duration_ms - a.duration_ms);
  return {
    wall_clock_span_ms: wallClockSpanMs,
    effective_span_ms: effectiveSpanMs,
    active_ms: activeMs,
    idle_ms: idleMs,
    merged_profiled_ms: mergedProfiledMs,
    coverage_ratio: effectiveSpanMs > 0 ? Math.min(1, activeMs / effectiveSpanMs) : undefined,
    largest_gaps: largestGaps.slice(0, 10),
  };
}

function latestRecord(records) {
  return [...records].sort((a, b) => (eventTime(b) || 0) - (eventTime(a) || 0))[0];
}

function normalizeCommand(command) {
  return String(command || "").replaceAll("\\", "/").replace(/\s+/g, " ").trim();
}

function commandKeys(record) {
  const keys = (record.commands || []).map(normalizeCommand).filter(Boolean);
  if (record.validation_check_id) keys.push(`validation_check:${record.validation_check_id}`);
  return keys;
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

function countMissingWorkItem(records) {
  return records.filter((record) => !record.work_item).length;
}

function countMissingContextInputs(records) {
  return records.filter((record) => record.context_risk && record.context_risk !== "low" && !(record.context_inputs || []).length).length;
}

function currentScopeRecords(records, scope) {
  const updatedMs = Date.parse(scope.updated_at || "");
  if (!Number.isFinite(updatedMs)) return [];
  return records.filter((record) => {
    const ts = eventTime(record);
    return ts !== undefined && ts >= updatedMs;
  });
}

function findTaskStatus(workItem, taskRoot = process.env.AI_PROFILE_TASK_ROOT || process.cwd()) {
  if (!workItem) return { found: false, active: false, status: "", path: "" };
  const taskIds = taskIdsForWorkItem(workItem);
  const roots = [
    join(taskRoot, "tasks", "active"),
    join(taskRoot, "tasks", "archive"),
  ];
  for (const root of roots) {
    if (!existsSync(root)) continue;
    const stack = [root];
    while (stack.length > 0) {
      const dir = stack.pop();
      for (const entry of readdirSync(dir, { withFileTypes: true })) {
        const path = join(dir, entry.name);
        if (entry.isDirectory()) {
          stack.push(path);
          continue;
        }
        if (!entry.isFile() || !entry.name.endsWith(".md")) continue;
        const text = readFileSync(path, "utf8");
        if (!taskIds.some((taskId) => new RegExp(`^id:\\s*${escapeRegExp(taskId)}\\s*$`, "m").test(text))) continue;
        const status = (text.match(/^status:\s*(.+?)\s*$/m) || [])[1] || "";
        return {
          found: true,
          active: path.includes(`${join("tasks", "active")}${separatorForPath(path)}`) && !["done", "dropped"].includes(status),
          status,
          path,
        };
      }
    }
  }
  return { found: false, active: false, status: "", path: "" };
}

function taskIdsForWorkItem(workItem) {
  const text = String(workItem || "").trim();
  const ids = [];
  if (text) ids.push(text);
  const match = text.match(/^(T\d+)(?:[\/:\s-]|$)/i);
  if (match && !ids.includes(match[1])) ids.push(match[1]);
  return ids;
}

function separatorForPath(path) {
  return path.includes("\\") ? "\\" : "/";
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function isLowCoverage(coverage) {
  return coverage.effective_span_ms >= 30 * 60 * 1000 && Number.isFinite(coverage.coverage_ratio) && coverage.coverage_ratio < 0.25;
}

function buildReviewConfidence({
  parsed,
  records,
  scopeReady,
  staleScope,
  scope,
  scopeTask,
  hasCurrentScopeWindow,
  scopedRecords,
  lowCoverage,
  currentLowCoverage,
  failedClassification,
  missingContextInputs,
  scopedMissingContextInputs,
}) {
  const blockingReasons = [];
  const partialReasons = [];
  const actions = [];

  if (!parsed.exists) {
    blockingReasons.push("profile_missing");
    actions.push("Start profiling with `node tools/ai.mjs start <work-item> <iteration>`.");
  } else if (records.length === 0) {
    blockingReasons.push("profile_empty");
    actions.push("Record the first checkpoint with `node tools/ai.mjs start <work-item> <iteration>`.");
  }

  if (failedClassification.unresolved > 0) {
    blockingReasons.push("unresolved_failed_records");
    actions.push("Resolve or explain unresolved failed records before using the profile as review evidence.");
  }

  if (!scopeReady) {
    blockingReasons.push("scope_missing");
    actions.push("Set the current work scope with `node tools/ai.mjs start <work-item> <iteration>`.");
  } else if (staleScope) {
    blockingReasons.push("scope_stale");
    actions.push(`Reset profiling scope: ${scope.work_item} is ${scopeTask.status || "not active"}.`);
  } else if (hasCurrentScopeWindow && scopedRecords.length === 0) {
    blockingReasons.push("current_scope_empty");
    actions.push("Append a current-scope checkpoint before reviewing this work.");
  }

  if (currentLowCoverage) {
    blockingReasons.push("current_scope_low_wall_clock_coverage");
    actions.push("Add checkpoints during long manual/research/design stretches with `node tools/ai.mjs checkpoint \"<intent>\"`.");
  } else if (lowCoverage) {
    partialReasons.push("whole_profile_low_wall_clock_coverage");
    actions.push("Treat historical whole-session conclusions as incomplete; prefer current-scope findings after resetting scope.");
  }

  if (hasCurrentScopeWindow ? scopedMissingContextInputs > 0 : missingContextInputs > 0) {
    partialReasons.push("unmeasured_context_inputs");
    actions.push("Measure medium/high context reads with `node tools/ai.mjs context --path <file>` or `node tools/ai.mjs context -- <command>`.");
  }

  const level = blockingReasons.length > 0 ? "broken" : partialReasons.length > 0 ? "partial" : "usable";
  return {
    level,
    usable_for_review: level === "usable",
    current_scope_preferred: scopeReady && !staleScope,
    blocking_reasons: blockingReasons,
    partial_reasons: partialReasons,
    actions: [...new Set(actions)],
  };
}

function buildCurrentScopeReviewConfidence({
  parsed,
  scopeReady,
  staleScope,
  scope,
  scopeTask,
  hasCurrentScopeWindow,
  scopedRecords,
  scopedCoverage,
  currentLowCoverage,
  scopedFailedClassification,
  scopedMissingContextInputs,
}) {
  const blockingReasons = [];
  const actions = [];

  if (!parsed.exists) {
    blockingReasons.push("profile_missing");
    actions.push("Start profiling with `node tools/ai.mjs start <work-item> <iteration>`.");
  }

  if (!scopeReady) {
    blockingReasons.push("scope_missing");
    actions.push("Set the current work scope with `node tools/ai.mjs start <work-item> <iteration>`.");
  } else if (staleScope) {
    blockingReasons.push("scope_stale");
    actions.push(`Reset profiling scope: ${scope.work_item} is ${scopeTask.status || "not active"}.`);
  } else if (hasCurrentScopeWindow && scopedRecords.length === 0) {
    blockingReasons.push("current_scope_empty");
    actions.push("Append a current-scope checkpoint before review with `node tools/ai.mjs checkpoint \"<intent>\" --force` or run a profiled command.");
  } else if (scopedRecords.length < 2) {
    blockingReasons.push("current_scope_too_shallow");
    actions.push("Record at least one current-scope checkpoint after `start` with `node tools/ai.mjs checkpoint \"<intent>\" --force` before using the profile as review evidence.");
  }

  if (scopedFailedClassification.unresolved > 0) {
    blockingReasons.push("current_scope_unresolved_failed_records");
    actions.push("Resolve or explain current-scope failed records before using the profile as review evidence.");
  }

  if (currentLowCoverage) {
    blockingReasons.push("current_scope_low_wall_clock_coverage");
    actions.push("Capture long manual/research/design gaps with `node tools/ai.mjs checkpoint \"<intent>\"`.");
  }

  if (scopedMissingContextInputs > 0) {
    blockingReasons.push("current_scope_unmeasured_context_inputs");
    actions.push("Measure medium/high context reads with `node tools/ai.mjs context --path <file>` or `node tools/ai.mjs context -- <command>`.");
  }

  const level = blockingReasons.length > 0 ? "broken" : "usable";
  return {
    level,
    usable_for_review: level === "usable",
    blocking_reasons: blockingReasons,
    actions: [...new Set(actions)],
    coverage_ratio: scopedCoverage.coverage_ratio,
    records: scopedRecords.length,
  };
}

function buildStatus(profilePaths) {
  const files = Array.isArray(profilePaths) ? profilePaths : [profilePaths];
  const parsed = parseProfiles(files);
  const records = parsed.records;
  const closeoutSeen = records.some((record) => record.phase === "session_closeout");
  const missingWorkItem = countMissingWorkItem(records);
  const missingContextInputs = countMissingContextInputs(records);
  const failedRecords = records.filter((record) => record.result === "fail").length;
  const failedClassification = classifyFailedRecords(records);
  const coverage = coverageStats(records);
  const scope = readProfileScope();
  const latest = latestRecord(records);
  const slowest = records
    .filter((record) => Number(record.duration_ms || 0) > 0)
    .sort((a, b) => Number(b.duration_ms || 0) - Number(a.duration_ms || 0))[0] || null;
  const largestContext = records
    .flatMap((record) => (record.context_inputs || []).map((input) => ({
      ...input,
      record_line: record.__line,
      intent: record.intent || "",
    })))
    .sort((a, b) => Number(b.chars || 0) - Number(a.chars || 0))[0] || null;

  let nextAction = "No profiling maintenance needed for normal game work.";
  const scopeReady = scope.valid && Boolean(scope.work_item);
  const scopeTask = scopeReady ? findTaskStatus(scope.work_item) : { found: false, active: false, status: "", path: "" };
  const staleScope = scopeReady && scopeTask.found && !scopeTask.active;
  const scopedRecords = scopeReady ? currentScopeRecords(records, scope) : [];
  const scopedCoverage = coverageStats(scopedRecords);
  const scopedMissingContextInputs = countMissingContextInputs(scopedRecords);
  const scopedMissingWorkItem = countMissingWorkItem(scopedRecords);
  const scopedFailedClassification = classifyFailedRecords(scopedRecords);
  const hasCurrentScopeWindow = scopeReady && Boolean(scope.updated_at);
  const actionableMissingContextInputs = hasCurrentScopeWindow ? scopedMissingContextInputs : missingContextInputs;
  const lowCoverage = isLowCoverage(coverage);
  const actionableLowCoverage = hasCurrentScopeWindow ? isLowCoverage(scopedCoverage) : lowCoverage;
  const reviewConfidence = buildReviewConfidence({
    parsed,
    records,
    scopeReady,
    staleScope,
    scope,
    scopeTask,
    hasCurrentScopeWindow,
    scopedRecords,
    lowCoverage,
    currentLowCoverage: isLowCoverage(scopedCoverage),
    failedClassification,
    missingContextInputs,
    scopedMissingContextInputs,
  });
  const currentScopeReviewConfidence = buildCurrentScopeReviewConfidence({
    parsed,
    scopeReady,
    staleScope,
    scope,
    scopeTask,
    hasCurrentScopeWindow,
    scopedRecords,
    scopedCoverage,
    currentLowCoverage: isLowCoverage(scopedCoverage),
    scopedFailedClassification,
    scopedMissingContextInputs,
  });

  if (!parsed.exists) {
    nextAction = "Start profiling with `node tools/ai.mjs start <work-item> <iteration>`.";
  } else if (parsed.errors.length > 0) {
    nextAction = "Fix invalid JSONL lines before using this profile for reflection.";
  } else if (records.length === 0) {
    nextAction = "Start the first checkpoint with `node tools/ai.mjs start <work-item> <iteration>`.";
  } else if (failedClassification.unresolved > 0) {
    nextAction = "Resolve or explain unresolved failed profile records before trusting this profile for reflection.";
  } else if (!scopeReady) {
    nextAction = "Start or reset current scope with `node tools/ai.mjs start <work-item> <iteration>`, or use `node tools/ai.mjs focus <iteration>` inside the same work item.";
  } else if (staleScope) {
    nextAction = `Reset profiling scope: current scope ${scope.work_item} is ${scopeTask.status || "not active"} at ${scopeTask.path}. Run \`node tools/ai.mjs start <current-work-item> <iteration>\` for the active pipeline slice.`;
  } else if (hasCurrentScopeWindow && scopedRecords.length === 0) {
    nextAction = "Append a current-scope record with `node tools/ai.mjs checkpoint \"<intent>\"`, `node tools/ai.mjs context`, or `node tools/ai.mjs run -- <command>`.";
  } else if (actionableMissingContextInputs > 0) {
    nextAction = "Use `node tools/ai.mjs context --path <file>` for medium/high local context reads so context_inputs are measured.";
  } else if (actionableLowCoverage) {
    nextAction = "Use node tools/ai.mjs checkpoint \"<intent>\" during long manual/research/design stretches so elapsed time is recorded with duration_ms.";
  } else if (!closeoutSeen) {
    nextAction = "At session end, run `node tools/ai.mjs reflect` to write a short session closeout.";
  } else {
    nextAction = "No profiling maintenance needed for normal game work.";
  }

  const passiveNextAction = failedClassification.unresolved > 0
    ? "Inspect unresolved failed records."
    : currentScopeReviewConfidence.blocking_reasons.length > 0
      ? currentScopeReviewConfidence.actions[0] || nextAction
    : lowCoverage
      ? "Historical wall-clock coverage is incomplete; use current-scope evidence for review and inspect the largest coverage gaps before making bottleneck claims."
    : slowest
      ? "No profiling maintenance needed for normal game work; use the slowest record only if you are investigating a slowdown."
      : "No profiling maintenance needed for normal game work.";

  return {
    schema_version: 1,
    profile: files.length === 1 ? files[0] : `${files.length} session logs`,
    profile_files: files,
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
    scope_task: scopeTask,
    stale_scope: staleScope,
    closeout_seen: closeoutSeen,
    work_item_coverage: {
      missing_records: missingWorkItem,
      coverage_ratio: records.length > 0 ? (records.length - missingWorkItem) / records.length : undefined,
    },
    current_scope: {
      since: scope.updated_at || "",
      records: scopedRecords.length,
      missing_work_item_records: scopedMissingWorkItem,
      missing_context_inputs: scopedMissingContextInputs,
      unresolved_failed_records: scopedFailedClassification.unresolved,
      recovered_failed_records: scopedFailedClassification.recovered,
      wall_clock_coverage: scopedCoverage,
      low_profile_coverage: isLowCoverage(scopedCoverage),
    },
    missing_context_inputs: missingContextInputs,
    failed_records: failedRecords,
    recovered_failed_records: failedClassification.recovered,
    unresolved_failed_records: failedClassification.unresolved,
    wall_clock_coverage: coverage,
    low_profile_coverage: lowCoverage,
    review_confidence: reviewConfidence,
    current_scope_review_confidence: currentScopeReviewConfidence,
    passive_summary: {
      mode: "passive",
      slowest_record: slowest ? {
        line: slowest.__line,
        duration_ms: Number(slowest.duration_ms || 0),
        phase: slowest.phase || "",
        category: slowest.category || "",
        intent: slowest.intent || "",
        result: slowest.result || "",
        commands: slowest.commands || [],
        passive_reason: slowest.passive_reason || "",
      } : null,
      largest_context_input: largestContext ? {
        path: largestContext.path || "",
        chars: Number(largestContext.chars || 0),
        reason: largestContext.reason || "",
        record_line: largestContext.record_line,
        intent: largestContext.intent,
      } : null,
      normal_work_next_action: passiveNextAction,
    },
    next_action: nextAction,
  };
}

function renderPassiveMarkdown(status) {
  const lines = [];
  const slowest = status.passive_summary.slowest_record;
  const largestContext = status.passive_summary.largest_context_input;
  lines.push(`# AI Profile Passive Status - ${basename(status.profile)}`);
  lines.push("");
  lines.push(`Mode: passive`);
  lines.push(`Records: ${status.records}`);
  lines.push(`Unresolved failures: ${status.unresolved_failed_records}`);
  lines.push(`Recovered failures: ${status.recovered_failed_records}`);
  lines.push(`Active work: ${formatMs(status.wall_clock_coverage.active_ms)} of ${formatMs(status.wall_clock_coverage.effective_span_ms)} effective (${formatPercent(status.wall_clock_coverage.coverage_ratio)})${status.wall_clock_coverage.idle_ms > 0 ? `; ${formatMs(status.wall_clock_coverage.idle_ms)} idle excluded` : ""}`);
  lines.push(`Review confidence: ${status.review_confidence.level}${status.review_confidence.usable_for_review ? " (usable)" : " (do not treat as complete review evidence)"}`);
  lines.push(`Current scope review confidence: ${status.current_scope_review_confidence.level}${status.current_scope_review_confidence.usable_for_review ? " (usable)" : " (not review evidence yet)"}`);
  if (status.scope.work_item) {
    lines.push(`Current scope: ${status.scope.work_item}${status.scope.iteration ? `/${status.scope.iteration}` : ""}`);
    if (status.scope_task?.found) {
      lines.push(`Current scope task: ${status.scope_task.status || "unknown"} ${status.scope_task.path}${status.stale_scope ? " (stale)" : ""}`);
    }
  } else {
    lines.push("Current scope: none");
  }
  lines.push("");
  lines.push("## Review Confidence");
  if (status.review_confidence.blocking_reasons.length === 0 && status.review_confidence.partial_reasons.length === 0) {
    lines.push("- usable: current profile health is sufficient for normal review.");
  } else {
    for (const reason of status.review_confidence.blocking_reasons) lines.push(`- blocking: ${reason}`);
    for (const reason of status.review_confidence.partial_reasons) lines.push(`- partial: ${reason}`);
    for (const action of status.review_confidence.actions) lines.push(`- action: ${action}`);
  }
  if (status.current_scope_review_confidence.blocking_reasons.length > 0) {
    lines.push("");
    lines.push("## Current Scope Guard");
    for (const reason of status.current_scope_review_confidence.blocking_reasons) lines.push(`- blocking: ${reason}`);
    for (const action of status.current_scope_review_confidence.actions) lines.push(`- action: ${action}`);
  }
  const showWholeProfileGaps = status.low_profile_coverage && status.wall_clock_coverage.largest_gaps.length > 0;
  const showCurrentScopeGaps = status.current_scope.low_profile_coverage && status.current_scope.wall_clock_coverage.largest_gaps.length > 0;
  if (showWholeProfileGaps || showCurrentScopeGaps) {
    lines.push("");
    lines.push("## Largest Coverage Gaps");
    if (showCurrentScopeGaps) {
      lines.push("- current scope:");
      for (const gap of status.current_scope.wall_clock_coverage.largest_gaps.slice(0, 5)) {
        lines.push(`  - ${formatMs(gap.duration_ms)} from ${gap.start_ts} to ${gap.end_ts} (lines ${gap.previous_line}-${gap.next_line})`);
      }
    }
    if (showWholeProfileGaps) {
      lines.push("- whole profile:");
      for (const gap of status.wall_clock_coverage.largest_gaps.slice(0, 5)) {
        lines.push(`  - ${formatMs(gap.duration_ms)} from ${gap.start_ts} to ${gap.end_ts} (lines ${gap.previous_line}-${gap.next_line})`);
      }
    }
  }
  lines.push("");
  lines.push("## Slowest Recorded Work");
  if (slowest) {
    lines.push(`- line ${slowest.line}: ${formatMs(slowest.duration_ms)} [${slowest.phase}/${slowest.category}] ${slowest.intent}`);
    if (slowest.commands.length > 0) lines.push(`- command: ${slowest.commands[0]}`);
    if (slowest.passive_reason) lines.push(`- reason: ${slowest.passive_reason}`);
  } else {
    lines.push("- none recorded");
  }
  lines.push("");
  lines.push("## Largest Context Input");
  if (largestContext) {
    lines.push(`- line ${largestContext.record_line}: ${largestContext.path} (${largestContext.chars} chars)`);
    if (largestContext.reason) lines.push(`- reason: ${largestContext.reason}`);
  } else {
    lines.push("- none recorded");
  }
  lines.push("");
  lines.push("## Next Action");
  lines.push(`- ${status.passive_summary.normal_work_next_action}`);
  lines.push("- Use `node tools/ai.mjs status --verbose` only for AI-workflow retrospectives.");
  return `${lines.join("\n")}\n`;
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
  if (status.scope_task?.found) {
    lines.push(`Scope task: ${status.scope_task.status || "unknown"} ${status.scope_task.path}${status.stale_scope ? " (stale)" : ""}`);
  }
  lines.push(`Work-item coverage: ${formatPercent(status.work_item_coverage.coverage_ratio)} (${status.work_item_coverage.missing_records} missing)`);
  lines.push(`Missing context inputs: ${status.missing_context_inputs}`);
  lines.push(`Review confidence: ${status.review_confidence.level} (${status.review_confidence.usable_for_review ? "usable" : "not complete review evidence"})`);
  lines.push(`Current scope review confidence: ${status.current_scope_review_confidence.level} (${status.current_scope_review_confidence.usable_for_review ? "usable" : "not review evidence yet"})`);
  if (status.review_confidence.blocking_reasons.length > 0) {
    lines.push(`Review confidence blocking reasons: ${status.review_confidence.blocking_reasons.join(", ")}`);
  }
  if (status.review_confidence.partial_reasons.length > 0) {
    lines.push(`Review confidence partial reasons: ${status.review_confidence.partial_reasons.join(", ")}`);
  }
  if (status.current_scope_review_confidence.blocking_reasons.length > 0) {
    lines.push(`Current scope review confidence blocking reasons: ${status.current_scope_review_confidence.blocking_reasons.join(", ")}`);
  }
  lines.push(`Current scope records: ${status.current_scope.records} (${status.current_scope.missing_context_inputs} missing context inputs, ${status.current_scope.missing_work_item_records} missing work items)`);
  lines.push(`Current scope active work: ${formatMs(status.current_scope.wall_clock_coverage.active_ms)} of ${formatMs(status.current_scope.wall_clock_coverage.effective_span_ms)} effective (${formatPercent(status.current_scope.wall_clock_coverage.coverage_ratio)})${status.current_scope.wall_clock_coverage.idle_ms > 0 ? `; ${formatMs(status.current_scope.wall_clock_coverage.idle_ms)} idle excluded` : ""}`);
  lines.push(`Failed records: ${status.failed_records} (${status.recovered_failed_records} recovered, ${status.unresolved_failed_records} unresolved)`);
  lines.push(`Active work: ${formatMs(status.wall_clock_coverage.active_ms)} of ${formatMs(status.wall_clock_coverage.effective_span_ms)} effective (${formatPercent(status.wall_clock_coverage.coverage_ratio)})${status.wall_clock_coverage.idle_ms > 0 ? `; ${formatMs(status.wall_clock_coverage.idle_ms)} idle excluded` : ""}`);
  if (status.current_scope.wall_clock_coverage.largest_gaps.length > 0 || status.wall_clock_coverage.largest_gaps.length > 0) {
    lines.push("");
    lines.push("## Largest Coverage Gaps");
    if (status.current_scope.wall_clock_coverage.largest_gaps.length > 0) {
      lines.push("- current scope:");
      for (const gap of status.current_scope.wall_clock_coverage.largest_gaps.slice(0, 5)) {
        lines.push(`  - ${formatMs(gap.duration_ms)} from ${gap.start_ts} to ${gap.end_ts} (lines ${gap.previous_line}-${gap.next_line})`);
      }
    }
    if (status.wall_clock_coverage.largest_gaps.length > 0) {
      lines.push("- whole profile:");
      for (const gap of status.wall_clock_coverage.largest_gaps.slice(0, 5)) {
        lines.push(`  - ${formatMs(gap.duration_ms)} from ${gap.start_ts} to ${gap.end_ts} (lines ${gap.previous_line}-${gap.next_line})`);
      }
    }
  }
  lines.push("");
  lines.push("## Slowest Recorded Work");
  const slowest = status.passive_summary.slowest_record;
  if (slowest) {
    lines.push(`- line ${slowest.line}: ${formatMs(slowest.duration_ms)} [${slowest.phase}/${slowest.category}] ${slowest.intent}`);
    if (slowest.commands.length > 0) lines.push(`- command: ${slowest.commands[0]}`);
    if (slowest.passive_reason) lines.push(`- reason: ${slowest.passive_reason}`);
  } else {
    lines.push("- none recorded");
  }
  lines.push("");
  lines.push("## Largest Context Input");
  const largestContext = status.passive_summary.largest_context_input;
  if (largestContext) {
    lines.push(`- line ${largestContext.record_line}: ${largestContext.path} (${largestContext.chars} chars)`);
    if (largestContext.reason) lines.push(`- reason: ${largestContext.reason}`);
  } else {
    lines.push("- none recorded");
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

const profilePaths = resolveProfilePaths(values);
const jsonOutputFile = stringArg(values, "json-output", "");
const status = buildStatus(profilePaths);
const rendered = values.verbose === true ? renderMarkdown(status) : renderPassiveMarkdown(status);

if (jsonOutputFile) {
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(status, null, 2)}\n`, "utf8");
}
process.stdout.write(rendered);

if (values["require-review-usable"] === true && !status.review_confidence.usable_for_review) {
  console.error(`profile guard failed: review confidence is ${status.review_confidence.level}`);
  for (const reason of status.review_confidence.blocking_reasons) console.error(`- blocking: ${reason}`);
  for (const reason of status.review_confidence.partial_reasons) console.error(`- partial: ${reason}`);
  for (const action of status.review_confidence.actions) console.error(`- action: ${action}`);
  process.exit(3);
}

if (values["require-current-scope-usable"] === true && !status.current_scope_review_confidence.usable_for_review) {
  console.error(`profile guard failed: current scope review confidence is ${status.current_scope_review_confidence.level}`);
  for (const reason of status.current_scope_review_confidence.blocking_reasons) console.error(`- blocking: ${reason}`);
  for (const action of status.current_scope_review_confidence.actions) console.error(`- action: ${action}`);
  process.exit(3);
}
