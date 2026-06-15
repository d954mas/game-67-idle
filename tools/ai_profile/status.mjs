#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { defaultProfilePath, parseArgs, readProfileScope, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/status.mjs [--profile <profile.jsonl>] [--json-output <status.json>] [--verbose] [--require-review-usable|--require-current-scope-usable]

Reports current AI profile health without appending records or generating a
closeout bundle. Default output is a short passive diagnostic; --verbose shows
the full reflection/baseline handoff state.

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

function coverageStats(records, gapThresholdMs = 5 * 60 * 1000) {
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
    return { wall_clock_span_ms: 0, merged_profiled_ms: 0, coverage_ratio: undefined, largest_gaps: [] };
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
    merged_profiled_ms: mergedProfiledMs,
    coverage_ratio: wallClockSpanMs > 0 ? Math.min(1, mergedProfiledMs / wallClockSpanMs) : undefined,
    largest_gaps: largestGaps.slice(0, 10),
  };
}

function fileMtimeMs(path) {
  try {
    return statSync(path).mtimeMs;
  } catch {
    return undefined;
  }
}

function bundleStatus(profilePath) {
  const profileMtimeMs = fileMtimeMs(profilePath);
  const artifacts = [
    ["summary", artifactPath(profilePath, "summary.md")],
    ["review", artifactPath(profilePath, "review.md")],
    ["review_json", artifactPath(profilePath, "review.json")],
    ["followups", artifactPath(profilePath, "followups.md")],
    ["followups_json", artifactPath(profilePath, "followups.json")],
  ].map(([name, path]) => {
    const mtimeMs = fileMtimeMs(path);
    const exists = mtimeMs !== undefined;
    const stale = exists && profileMtimeMs !== undefined && mtimeMs + 1 < profileMtimeMs;
    return { name, path, exists, mtime_ms: mtimeMs, stale };
  });
  const complete = artifacts.every((artifact) => artifact.exists);
  return {
    complete,
    fresh: complete && artifacts.every((artifact) => !artifact.stale),
    profile_mtime_ms: profileMtimeMs,
    stale_artifacts: artifacts.filter((artifact) => artifact.stale).map((artifact) => artifact.name),
    artifacts,
  };
}

function readBaselineStatus(profilePath) {
  const dir = join(dirname(profilePath), "baselines");
  if (!existsSync(dir)) return { dir, count: 0, latest_manifest: null, errors: [] };
  const manifests = [];
  const errors = [];
  for (const name of readdirSync(dir)) {
    if (!name.endsWith(".manifest.json")) continue;
    const path = join(dir, name);
    try {
      const parsed = JSON.parse(readFileSync(path, "utf8"));
      const capturedMs = Date.parse(parsed.captured_at || "");
      manifests.push({
        manifest_path: path,
        label: parsed.label || "",
        captured_at: parsed.captured_at || "",
        captured_ms: Number.isFinite(capturedMs) ? capturedMs : fileMtimeMs(path) || 0,
        source_review: parsed.source_review || "",
        baseline_review: parsed.baseline_review || "",
        compare_command: parsed.compare_command || "",
        summary: parsed.summary || {},
      });
    } catch (error) {
      errors.push(`${path}: ${error.message}`);
    }
  }
  manifests.sort((a, b) => b.captured_ms - a.captured_ms || b.manifest_path.localeCompare(a.manifest_path));
  const latest = manifests[0] || null;
  return {
    dir,
    count: manifests.length,
    latest_manifest: latest ? {
      manifest_path: latest.manifest_path,
      label: latest.label,
      captured_at: latest.captured_at,
      source_review: latest.source_review,
      baseline_review: latest.baseline_review,
      compare_command: latest.compare_command,
      summary: latest.summary,
    } : null,
    errors,
  };
}

function compareArtifactPaths(profilePath, baseline) {
  if (!baseline) return null;
  const label = baseline.label || basename(baseline.baseline_review || "baseline").replace(/\.review\.json$/i, "");
  const dir = dirname(profilePath);
  return {
    label,
    review_json: artifactPath(profilePath, "review.json"),
    compare_md: join(dir, `${label}.compare.md`),
    compare_json: join(dir, `${label}.compare.json`),
  };
}

function reflectionArtifactPaths(profilePath) {
  const base = basename(profilePath).replace(/\.jsonl$/i, "");
  const dir = dirname(profilePath);
  return {
    packet_md: join(dir, `${base}.reflection_packet.md`),
    packet_json: join(dir, `${base}.reflection_packet.json`),
    draft_md: join(dir, `${base}.reflection_draft.md`),
    draft_json: join(dir, `${base}.reflection_draft.json`),
    review_md: join(dir, `${base}.reflection_review.md`),
    review_json: join(dir, `${base}.reflection_review.json`),
  };
}

function latestMtime(paths) {
  const mtimes = paths.map(fileMtimeMs).filter((mtime) => mtime !== undefined);
  return mtimes.length > 0 ? Math.max(...mtimes) : undefined;
}

function artifactPairStatus(markdownPath, jsonPath, dependencyPaths, blockedReason = "") {
  const markdownMtime = fileMtimeMs(markdownPath);
  const jsonMtime = fileMtimeMs(jsonPath);
  const markdownExists = markdownMtime !== undefined;
  const jsonExists = jsonMtime !== undefined;
  if (blockedReason) {
    return {
      status: "waiting",
      reason: blockedReason,
      markdown: markdownPath,
      json: jsonPath,
      markdown_exists: markdownExists,
      json_exists: jsonExists,
      stale: false,
    };
  }
  if (!markdownExists || !jsonExists) {
    return {
      status: "missing",
      reason: !markdownExists && !jsonExists ? "markdown and json are missing" : markdownExists ? "json is missing" : "markdown is missing",
      markdown: markdownPath,
      json: jsonPath,
      markdown_exists: markdownExists,
      json_exists: jsonExists,
      stale: false,
    };
  }
  const dependencyMtime = latestMtime(dependencyPaths);
  const stale = dependencyMtime !== undefined && Math.min(markdownMtime, jsonMtime) + 1 < dependencyMtime;
  return {
    status: stale ? "stale" : "fresh",
    reason: stale ? "artifact is older than reflection inputs" : "artifact is fresh",
    markdown: markdownPath,
    json: jsonPath,
    markdown_exists: markdownExists,
    json_exists: jsonExists,
    stale,
  };
}

function readReflectionStatus(profilePath, comparison) {
  const paths = reflectionArtifactPaths(profilePath);
  const packetDependencies = [
    profilePath,
    artifactPath(profilePath, "review.json"),
    artifactPath(profilePath, "followups.json"),
  ];
  if (comparison?.paths?.compare_json) packetDependencies.push(comparison.paths.compare_json);
  const packet = artifactPairStatus(paths.packet_md, paths.packet_json, packetDependencies);
  const draftBlockedReason = packet.status === "fresh" ? "" : "packet is not fresh";
  const draft = artifactPairStatus(paths.draft_md, paths.draft_json, [paths.packet_json, artifactPath(profilePath, "review.json")], draftBlockedReason);
  const reviewBlockedReason = draft.status === "fresh" ? "" : "draft is not fresh";
  const review = artifactPairStatus(paths.review_md, paths.review_json, [paths.draft_json], reviewBlockedReason);
  return {
    packet: {
      ...packet,
      command: `node tools/ai_profile/reflection_packet.mjs ${profilePath} --output ${paths.packet_md} --json-output ${paths.packet_json}`,
    },
    draft: {
      ...draft,
      command: `node tools/ai_profile/reflection_draft.mjs ${paths.packet_json} --output ${paths.draft_md} --json-output ${paths.draft_json}`,
    },
    review: {
      ...review,
      command: `node tools/ai_profile/reflection_review.mjs ${paths.draft_json} --output ${paths.review_md} --json-output ${paths.review_json}`,
    },
  };
}

function readComparisonStatus(profilePath, baseline) {
  const paths = compareArtifactPaths(profilePath, baseline);
  if (!paths || !baseline) {
    return { available: false, status: "none", reason: "no baseline captured", paths: null, verdict: "", current_regressions: 0, compare_command: "" };
  }
  const compareCommand = `node tools/ai_profile/compare_reviews.mjs ${baseline.baseline_review} ${paths.review_json} --output ${paths.compare_md} --json-output ${paths.compare_json}`;
  const reviewMtime = fileMtimeMs(paths.review_json);
  const baselineMtime = fileMtimeMs(baseline.baseline_review);
  const compareMtime = fileMtimeMs(paths.compare_json);
  if (compareMtime === undefined) {
    return { available: true, status: "missing", reason: "comparison json is missing", paths, verdict: "", current_regressions: 0, compare_command: compareCommand };
  }
  const staleAgainstReview = reviewMtime !== undefined && compareMtime + 1 < reviewMtime;
  const staleAgainstBaseline = baselineMtime !== undefined && compareMtime + 1 < baselineMtime;
  if (staleAgainstReview || staleAgainstBaseline) {
    return { available: true, status: "stale", reason: staleAgainstReview ? "comparison is older than current review" : "comparison is older than baseline", paths, verdict: "", current_regressions: 0, compare_command: compareCommand };
  }
  try {
    const parsed = JSON.parse(readFileSync(paths.compare_json, "utf8"));
    const regressions = Array.isArray(parsed.current_regressions) ? parsed.current_regressions.length : 0;
    return {
      available: true,
      status: regressions > 0 ? "regressed" : "fresh",
      reason: regressions > 0 ? "current-scope regressions present" : "comparison is fresh",
      paths,
      verdict: parsed.verdict || "",
      current_regressions: regressions,
      compare_command: compareCommand,
    };
  } catch (error) {
    return { available: true, status: "invalid", reason: `comparison json is invalid: ${error.message}`, paths, verdict: "", current_regressions: 0, compare_command: compareCommand };
  }
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
  return coverage.wall_clock_span_ms >= 30 * 60 * 1000 && Number.isFinite(coverage.coverage_ratio) && coverage.coverage_ratio < 0.25;
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

function buildStatus(profilePath) {
  const parsed = parseProfile(profilePath);
  const records = parsed.records;
  const closeoutSeen = records.some((record) => record.phase === "session_closeout");
  const missingWorkItem = countMissingWorkItem(records);
  const missingContextInputs = countMissingContextInputs(records);
  const failedRecords = records.filter((record) => record.result === "fail").length;
  const failedClassification = classifyFailedRecords(records);
  const coverage = coverageStats(records);
  const bundle = bundleStatus(profilePath);
  const baselines = readBaselineStatus(profilePath);
  const comparison = readComparisonStatus(profilePath, baselines.latest_manifest);
  const reflection = readReflectionStatus(profilePath, comparison);
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

  let nextAction = baselines.latest_manifest
    ? comparison.status === "fresh"
      ? `Use fresh comparison ${comparison.paths.compare_json} as baseline trend evidence.`
      : comparison.status === "regressed"
        ? `Inspect current-scope regressions in ${comparison.paths.compare_json} before writing reflection.`
        : `Run baseline comparison: ${comparison.compare_command}`
    : `Capture this clean review with \`node tools/ai_profile/capture_baseline.mjs ${artifactPath(profilePath, "review.json")} --label <name>\`.`;
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
    nextAction = "At session end, run closeout.mjs to generate the reflection bundle.";
  } else if (!bundle.complete) {
    nextAction = "Run closeout.mjs again, or rerun review/followups if the bundle was intentionally skipped.";
  } else if (!bundle.fresh) {
    nextAction = "Before reflection, rerun closeout.mjs so summary, review, and followups match the latest profile.";
  } else if (baselines.latest_manifest && comparison.status !== "fresh" && comparison.status !== "regressed") {
    nextAction = `Run baseline comparison: ${comparison.compare_command}`;
  } else if (baselines.latest_manifest && comparison.status === "regressed") {
    nextAction = `Inspect current-scope regressions in ${comparison.paths.compare_json} before writing reflection.`;
  } else if (baselines.latest_manifest && comparison.status === "fresh" && reflection.packet.status !== "fresh") {
    nextAction = `Generate reflection packet: ${reflection.packet.command}`;
  } else if (baselines.latest_manifest && comparison.status === "fresh" && reflection.draft.status !== "fresh") {
    nextAction = `Generate reflection draft: ${reflection.draft.command}`;
  } else if (baselines.latest_manifest && comparison.status === "fresh" && reflection.review.status !== "fresh") {
    nextAction = `Generate reflection review: ${reflection.review.command}`;
  } else if (baselines.latest_manifest && comparison.status === "fresh" && comparison.paths) {
    nextAction = `Use fresh reflection review ${reflection.review.markdown} as the first retrospective decision artifact.`;
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
    scope_task: scopeTask,
    stale_scope: staleScope,
    closeout_seen: closeoutSeen,
    bundle,
    baselines,
    comparison,
    reflection,
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
  lines.push(`Wall-clock coverage: ${formatPercent(status.wall_clock_coverage.coverage_ratio)} (${formatMs(status.wall_clock_coverage.merged_profiled_ms)} / ${formatMs(status.wall_clock_coverage.wall_clock_span_ms)})`);
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
  lines.push(`Bundle complete: ${status.bundle.complete ? "yes" : "no"}`);
  lines.push(`Bundle fresh: ${status.bundle.fresh ? "yes" : "no"}${status.bundle.stale_artifacts.length > 0 ? ` (${status.bundle.stale_artifacts.join(", ")} stale)` : ""}`);
  lines.push(`Captured baselines: ${status.baselines.count}${status.baselines.latest_manifest ? ` (latest: ${status.baselines.latest_manifest.label || basename(status.baselines.latest_manifest.manifest_path)})` : ""}`);
  lines.push(`Baseline comparison: ${status.comparison.status}${status.comparison.verdict ? ` (${status.comparison.verdict})` : ""}`);
  lines.push(`Reflection packet: ${status.reflection.packet.status}`);
  lines.push(`Reflection draft: ${status.reflection.draft.status}`);
  lines.push(`Reflection review: ${status.reflection.review.status}`);
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
  lines.push(`Current scope wall-clock coverage: ${formatPercent(status.current_scope.wall_clock_coverage.coverage_ratio)} (${formatMs(status.current_scope.wall_clock_coverage.merged_profiled_ms)} / ${formatMs(status.current_scope.wall_clock_coverage.wall_clock_span_ms)})`);
  lines.push(`Failed records: ${status.failed_records} (${status.recovered_failed_records} recovered, ${status.unresolved_failed_records} unresolved)`);
  lines.push(`Wall-clock coverage: ${formatPercent(status.wall_clock_coverage.coverage_ratio)} (${formatMs(status.wall_clock_coverage.merged_profiled_ms)} / ${formatMs(status.wall_clock_coverage.wall_clock_span_ms)})`);
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
  lines.push("## Bundle Artifacts");
  for (const artifact of status.bundle.artifacts) {
    lines.push(`- ${artifact.exists ? "yes" : "no"} ${artifact.name}: ${artifact.path}`);
  }
  lines.push("");
  lines.push("## Baselines");
  if (!status.baselines.latest_manifest) {
    lines.push("- none");
  } else {
    lines.push(`- latest label: ${status.baselines.latest_manifest.label || "(unlabeled)"}`);
    lines.push(`- captured at: ${status.baselines.latest_manifest.captured_at || "unknown"}`);
    lines.push(`- review: ${status.baselines.latest_manifest.baseline_review}`);
    lines.push(`- manifest: ${status.baselines.latest_manifest.manifest_path}`);
    if (status.baselines.latest_manifest.compare_command) {
      lines.push(`- compare command: ${status.baselines.latest_manifest.compare_command}`);
    }
  }
  if (status.baselines.errors.length > 0) {
    lines.push("- errors:");
    for (const error of status.baselines.errors) lines.push(`  - ${error}`);
  }
  lines.push("");
  lines.push("## Baseline Comparison");
  lines.push(`- status: ${status.comparison.status}`);
  lines.push(`- reason: ${status.comparison.reason}`);
  if (status.comparison.paths) {
    lines.push(`- compare json: ${status.comparison.paths.compare_json}`);
    lines.push(`- compare markdown: ${status.comparison.paths.compare_md}`);
  }
  if (status.comparison.verdict) lines.push(`- verdict: ${status.comparison.verdict}`);
  lines.push(`- current-scope regressions: ${status.comparison.current_regressions}`);
  if (status.comparison.compare_command) lines.push(`- command: ${status.comparison.compare_command}`);
  lines.push("");
  lines.push("## Reflection Artifacts");
  lines.push(`- packet: ${status.reflection.packet.status} (${status.reflection.packet.reason})`);
  lines.push(`  - markdown: ${status.reflection.packet.markdown}`);
  lines.push(`  - json: ${status.reflection.packet.json}`);
  lines.push(`  - command: ${status.reflection.packet.command}`);
  lines.push(`- draft: ${status.reflection.draft.status} (${status.reflection.draft.reason})`);
  lines.push(`  - markdown: ${status.reflection.draft.markdown}`);
  lines.push(`  - json: ${status.reflection.draft.json}`);
  lines.push(`  - command: ${status.reflection.draft.command}`);
  lines.push(`- review: ${status.reflection.review.status} (${status.reflection.review.reason})`);
  lines.push(`  - markdown: ${status.reflection.review.markdown}`);
  lines.push(`  - json: ${status.reflection.review.json}`);
  lines.push(`  - command: ${status.reflection.review.command}`);
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
