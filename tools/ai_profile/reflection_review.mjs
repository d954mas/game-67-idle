#!/usr/bin/env node
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/reflection_review.mjs <reflection_draft.json> [--output <review.md>] [--json-output <review.json>]

Builds a compact current reflection review from a reflection_draft.mjs JSON
artifact. This is a decision aid for the final retrospective, not the final
retrospective itself.`);
  process.exit(2);
}

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function commandScopeCount(draft, scope) {
  const item = asArray(draft.repeated_commands?.by_scope).find((entry) => entry.scope === scope);
  return Number(item?.count || 0);
}

function formatMs(ms) {
  const value = Number(ms || 0);
  if (!Number.isFinite(value) || value <= 0) return "0s";
  const seconds = value / 1000;
  if (seconds < 90) return `${seconds.toFixed(1)}s`;
  const minutes = seconds / 60;
  if (minutes < 90) return `${minutes.toFixed(1)}m`;
  return `${(minutes / 60).toFixed(2)}h`;
}

function formatPercent(value) {
  const number = Number(value);
  if (!Number.isFinite(number)) return "unknown";
  return `${(number * 100).toFixed(1)}%`;
}

function formatShare(value, total) {
  const numerator = Number(value || 0);
  const denominator = Number(total || 0);
  if (!Number.isFinite(numerator) || !Number.isFinite(denominator) || denominator <= 0) return "0.0%";
  return `${((numerator / denominator) * 100).toFixed(1)}%`;
}

function coverageConfidenceLine(snapshot) {
  const ratio = Number(snapshot?.coverage_ratio);
  const wallClockMs = Number(snapshot?.wall_clock_ms || 0);
  if (!Number.isFinite(ratio)) {
    return "Current coverage confidence is unknown; do not make precise time-spend claims from this scope.";
  }
  if (wallClockMs >= 5 * 60 * 1000 && ratio < 0.25) {
    return "Current coverage confidence is partial; explain unprofiled wall-clock time before making precise time-spend claims.";
  }
  if (ratio >= 0.5) {
    return "Current coverage confidence is usable for rough time-spend claims.";
  }
  return "Current coverage confidence is limited; use tool durations as partial evidence, not total iteration time.";
}

function toolCapturedElapsed(item) {
  return (item?.duration_kind || "") === "captured_elapsed"
    || (Number(item?.captured_elapsed_ms || 0) > 0 && Number(item?.runtime_ms || 0) === 0);
}

function toolDurationSuffix(item) {
  const kind = item?.duration_kind || "runtime";
  if (kind === "captured_elapsed") return "captured elapsed";
  if (kind === "mixed") return `mixed; captured=${formatMs(item.captured_elapsed_ms || 0)}, runtime=${formatMs(item.runtime_ms || 0)}`;
  return Number(item?.command_runtime_ms || 0) > 0 ? "command/runtime" : "runtime";
}

function sortByDuration(items, field) {
  return [...asArray(items)].sort((a, b) => Number(b?.[field] || 0) - Number(a?.[field] || 0) || Number(b?.records || 0) - Number(a?.records || 0));
}

function runtimeToolSummary(tools) {
  return sortByDuration(asArray(tools).filter((item) => !toolCapturedElapsed(item)), "runtime_ms");
}

function capturedElapsedSummary(tools) {
  return sortByDuration(asArray(tools).filter((item) => Number(item?.captured_elapsed_ms || 0) > 0), "captured_elapsed_ms");
}

function toolRuntimeMs(item) {
  return Number(item?.runtime_ms || item?.duration_ms || 0);
}

function toolCapturedElapsedMs(item) {
  return Number(item?.captured_elapsed_ms || item?.duration_ms || 0);
}

function sumField(items, field) {
  return asArray(items).reduce((sum, item) => sum + Number(item?.[field] || 0), 0);
}

function contextSummaryWithTotals(summary) {
  const hotspots = asArray(summary?.hotspots);
  return {
    ...(summary || {}),
    hotspots,
    high_context: asArray(summary?.high_context),
    missing_inputs: asArray(summary?.missing_inputs),
    total_hotspot_chars: sumField(hotspots, "chars"),
  };
}

function repeatedCommandsWithTotals(summary) {
  const byScope = asArray(summary?.by_scope);
  const classification = asArray(summary?.classification);
  return {
    ...(summary || {}),
    by_scope: byScope,
    classification,
    validation_batches: asArray(summary?.validation_batches),
    repeated_total_occurrences: sumField(byScope, "count"),
    classification_total_occurrences: sumField(classification, "count"),
  };
}

function largestByField(items, field) {
  return asArray(items).reduce((best, item) => {
    const value = Number(item?.[field] || 0);
    if (!best || value > Number(best?.[field] || 0)) return item;
    return best;
  }, null);
}

function hasCapturedElapsedTools(tools) {
  return asArray(tools).some((item) => toolCapturedElapsed(item));
}

function currentScopeReadout(currentClean, snapshot, tools, contextSummary, validationBatches) {
  if (!snapshot?.enabled) {
    return ["No current-scope snapshot is available; start or focus the next iteration before relying on generated reflection."];
  }
  const lines = [];
  const scopeName = `${snapshot.work_item || ""}${snapshot.iteration ? `/${snapshot.iteration}` : ""}` || "unknown";
  lines.push(`Current scope ${scopeName} is ${currentClean ? "clean" : "actionable"}: ${snapshot.records || 0} record(s), ${formatMs(snapshot.profiled_ms || 0)} profiled over ${formatMs(snapshot.wall_clock_ms || 0)} wall-clock (${formatPercent(snapshot.coverage_ratio)}).`);
  lines.push(coverageConfidenceLine(snapshot));
  const largestGap = asArray(snapshot.largest_gaps)[0];
  if (largestGap) {
    lines.push(`Largest current wall-clock gap: ${formatMs(largestGap.duration_ms)} from ${largestGap.start_ts || "unknown"} to ${largestGap.end_ts || "unknown"}.`);
  }
  const gapTotal = Number(snapshot.missing_context_inputs || 0) + Number(snapshot.missing_work_item_records || 0) + Number(snapshot.missing_tool_records || 0);
  if (gapTotal === 0) {
    lines.push("Current telemetry has no context, work-item, or tool metadata gaps.");
  } else {
    lines.push(`Current telemetry gaps: context=${snapshot.missing_context_inputs || 0}, work_item=${snapshot.missing_work_item_records || 0}, tools=${snapshot.missing_tool_records || 0}.`);
  }
  const unresolved = Number(snapshot.unresolved_failed_records || 0);
  const recovered = Number(snapshot.recovered_failed_records || 0);
  lines.push(`Current failures: unresolved=${unresolved}, recovered=${recovered}.`);
  const toolRows = asArray(tools);
  const topRuntimeTool = toolRows.find((item) => !toolCapturedElapsed(item));
  const topCapturedElapsed = toolRows.find((item) => toolCapturedElapsed(item));
  if (topRuntimeTool) {
    lines.push(`Largest current tool runtime: ${topRuntimeTool.tool || "unknown"} (${formatMs(topRuntimeTool.runtime_ms || topRuntimeTool.duration_ms)}, ${topRuntimeTool.records || 0} record(s)).`);
  }
  if (topCapturedElapsed) {
    lines.push(`Largest current captured elapsed checkpoint: ${topCapturedElapsed.tool || "unknown"} (${formatMs(topCapturedElapsed.captured_elapsed_ms || topCapturedElapsed.duration_ms)}, ${topCapturedElapsed.records || 0} record(s)).`);
  }
  const batches = asArray(validationBatches);
  if (batches.length > 0) {
    const records = batches.reduce((sum, item) => sum + Number(item.records || 0), 0);
    const broadFinal = batches.reduce((sum, item) => sum + Number(item.broad_final_commands || 0), 0);
    const failed = batches.reduce((sum, item) => sum + Number(item.failed || 0), 0);
    lines.push(`Current validation was batched: ${batches.length} batch(es), ${records} record(s), broad/final=${broadFinal}, failed=${failed}.`);
  }
  const hotspot = asArray(contextSummary?.hotspots)[0];
  if (hotspot) {
    lines.push(`Largest current context input: ${hotspot.path || "unknown"} (${hotspot.chars || 0} chars).`);
  } else {
    lines.push("Current scope has no measured context hotspots.");
  }
  return lines;
}

function topImprovements(draft, currentClean) {
  const improvements = [];
  const historical = asArray(draft.historical_lessons);
  const hasLesson = (type) => historical.some((lesson) => lesson.type === type);
  const repeatedCommands = repeatedCommandsWithTotals(draft.repeated_commands);
  const topRepeatedScope = largestByField(repeatedCommands.by_scope, "count");
  const toolUseSummary = asArray(draft.tool_use_summary);
  const runtimeTools = runtimeToolSummary(toolUseSummary);
  const runtimeTotal = sumField(runtimeTools, "runtime_ms");
  const topRuntimeTool = largestByField(runtimeTools, "runtime_ms");
  const capturedElapsed = capturedElapsedSummary(toolUseSummary);
  const capturedTotal = sumField(capturedElapsed, "captured_elapsed_ms");
  const topCapturedElapsed = largestByField(capturedElapsed, "captured_elapsed_ms");
  const contextSummary = contextSummaryWithTotals(draft.context_use_summary);
  const topContextHotspot = largestByField(contextSummary.hotspots, "chars");
  if (!currentClean) {
    improvements.push("Resolve current-scope findings, regressions, and pending follow-ups before treating historical lessons as process work.");
  }
  if (hasLesson("repeated_commands")) {
    if (topRepeatedScope) {
      improvements.push(`Use Repeated Command Review shares to triage repeats before adding process tasks; largest scope is ${topRepeatedScope.scope || "unknown"} (${formatShare(topRepeatedScope.count, repeatedCommands.repeated_total_occurrences)} of repeated occurrences).`);
    } else {
      improvements.push("Use repeated_command_classification to triage repeats before adding process tasks.");
    }
  }
  if (asArray(draft.repeated_commands?.unbatched_broad_final_commands).length > 0 || hasLesson("repeated_broad_final")) {
    const occurrences = Number(draft.repeated_commands?.unbatched_broad_final_occurrences || 0);
    const evidenceScope = currentClean ? "historical whole-profile review shows" : "review shows";
    improvements.push(`Batch broad/final validation with node tools/ai.mjs validate and rerun it only after a failed gate, changed risk, or final handoff${occurrences > 0 ? `; ${evidenceScope} ${occurrences} unbatched broad/final occurrence(s)` : ""}.`);
  }
  if (asArray(draft.repeated_commands?.validation_batches).length > 0) {
    improvements.push("Use validation batch evidence to separate planned validation runs from ad hoc repeated commands.");
  }
  if (toolUseSummary.length > 0) {
    if (hasCapturedElapsedTools(toolUseSummary)) {
      const runtimeDetail = topRuntimeTool ? `; top runtime is ${topRuntimeTool.tool || "unknown"} (${formatShare(topRuntimeTool.runtime_ms || topRuntimeTool.duration_ms, runtimeTotal)})` : "";
      const elapsedDetail = topCapturedElapsed ? `; top captured elapsed is ${topCapturedElapsed.tool || "unknown"} (${formatShare(topCapturedElapsed.captured_elapsed_ms || topCapturedElapsed.duration_ms, capturedTotal)})` : "";
      improvements.push(`Use Tool Runtime Review for actual command/tool cost and Captured Elapsed Review for checkpointed manual, research, design, or review spans${runtimeDetail}${elapsedDetail}.`);
    } else {
      improvements.push("Use tool_use_summary to explain which tool classes consumed time, failed, or produced context.");
    }
  }
  if (topContextHotspot) {
    improvements.push(`Use context_use_summary to explain context pressure; largest input is ${topContextHotspot.path || "unknown"} (${topContextHotspot.chars || 0} chars, ${formatShare(topContextHotspot.chars, contextSummary.total_hotspot_chars)} of hotspot chars).`);
  }
  if (hasLesson("missing_context_inputs")) {
    improvements.push("Use node tools/ai.mjs context --path <file> or node tools/ai.mjs context -- <command> so reflection can measure context cost.");
  }
  if (hasLesson("missing_work_item_metadata")) {
    improvements.push("Start each focused work item with node tools/ai.mjs start, then use node tools/ai.mjs focus for later slices before running substantial commands.");
  }
  if (hasLesson("missing_tool_metadata")) {
    improvements.push("Use ai.mjs facades or profiler wrappers that populate tools so tool_use_summary stays complete.");
  }
  if (hasLesson("low_profile_coverage")) {
    improvements.push("Place node tools/ai.mjs checkpoint records during long manual, research, design, or review stretches.");
  }
  if (hasLesson("recovered_failed_records")) {
    improvements.push("Use recovered_failure_classification to separate useful validation feedback, avoidable rework, and tool/environment noise.");
  }
  if (asArray(draft.current_state?.satisfied_followups).length > 0) {
    improvements.push("Keep satisfied follow-ups out of new task creation unless fresh evidence reopens them.");
  }
  improvements.push("Use prepare_reflection.mjs as the normal handoff entrypoint before writing the final retrospective.");
  improvements.push("Edit generated draft/review artifacts with judgment; do not paste generated text as the final retrospective.");
  return [...new Set(improvements)].slice(0, 10);
}

function buildReview(draft, draftPath) {
  const currentFindings = asArray(draft.current_state?.current_scope_findings);
  const currentRegressions = asArray(draft.current_state?.current_regressions);
  const pendingFollowups = asArray(draft.current_state?.pending_followups);
  const currentClean = currentFindings.length === 0 && currentRegressions.length === 0 && pendingFollowups.length === 0;
  const historicalLessons = asArray(draft.historical_lessons).map((lesson) => ({
    type: lesson.type || "lesson",
    symptom: lesson.symptom || "",
    cause: lesson.cause || "",
    fix: lesson.fix || "",
    current_action: currentClean ? "historical_only" : "review_after_current_items",
  }));
  const currentActions = [];
  for (const finding of currentFindings) currentActions.push(finding.message || finding.type || "Resolve current-scope finding.");
  for (const regression of currentRegressions) currentActions.push(`Inspect current-scope regression: ${regression.label || regression.key || "unknown"}.`);
  for (const followup of pendingFollowups) currentActions.push(followup.next_action || followup.title || "Review pending follow-up.");
  const currentStatusMessage = currentActions.length === 0
    ? "No current action items; use historical lessons as next-cycle process guidance."
    : "Resolve current action items before treating historical lessons as next-cycle guidance.";
  const currentSnapshot = draft.current_state?.current_scope_snapshot || { enabled: false };
  const currentTools = asArray(draft.current_state?.current_scope_tool_use_summary);
  const currentRuntimeTools = runtimeToolSummary(currentTools);
  const currentCapturedElapsed = capturedElapsedSummary(currentTools);
  const currentContext = contextSummaryWithTotals(draft.current_state?.current_scope_context_use_summary);
  const currentValidationBatches = asArray(draft.current_state?.current_scope_validation_batches);
  const toolUseSummary = asArray(draft.tool_use_summary);
  const toolRuntimeSummary = runtimeToolSummary(toolUseSummary);
  const capturedElapsed = capturedElapsedSummary(toolUseSummary);
  const contextSummary = contextSummaryWithTotals(draft.context_use_summary);
  const repeatedCommands = repeatedCommandsWithTotals(draft.repeated_commands);

  return {
    schema_version: 1,
    draft: draftPath,
    verdict: currentClean ? "current_clean" : "current_action_required",
    current: {
      findings: currentFindings,
      regressions: currentRegressions,
      pending_followups: pendingFollowups,
      actions: currentActions,
      status_message: currentStatusMessage,
      readout: currentScopeReadout(currentClean, currentSnapshot, currentTools, currentContext, currentValidationBatches),
      snapshot: currentSnapshot,
      tool_use_summary: currentTools,
      tool_runtime_total_ms: sumField(currentRuntimeTools, "runtime_ms"),
      captured_elapsed_total_ms: sumField(currentCapturedElapsed, "captured_elapsed_ms"),
      context_use_summary: currentContext,
      validation_batches: currentValidationBatches,
    },
    historical_lessons: historicalLessons,
    suppressed_historical_findings: asArray(draft.suppressed_historical_findings),
    repeated_commands: repeatedCommands,
    tool_use_summary: toolUseSummary,
    tool_runtime_summary: toolRuntimeSummary,
    tool_runtime_total_ms: sumField(toolRuntimeSummary, "runtime_ms"),
    captured_elapsed_summary: capturedElapsed,
    captured_elapsed_total_ms: sumField(capturedElapsed, "captured_elapsed_ms"),
    context_use_summary: contextSummary,
    recovered_failure_classification: asArray(draft.recovered_failure_classification),
    satisfied_followups: asArray(draft.current_state?.satisfied_followups),
    top_improvements: topImprovements(draft, currentClean),
    caveats: [
      "Generated review only; use it to structure the final retrospective, not as a substitute for judgment.",
      "Historical-only lessons should not create current tasks unless the same issue recurs in the current scope.",
    ],
  };
}

function renderMarkdown(review, draftPath) {
  const lines = [];
  lines.push(`# AI Reflection Review - ${basename(draftPath)}`);
  lines.push("");
  lines.push("Generated review: decision aid for the final retrospective, not final prose.");
  lines.push(`Verdict: ${review.verdict}`);
  lines.push(`Current actions: ${review.current.actions.length}`);
  lines.push(`Historical lessons: ${review.historical_lessons.length}`);
  lines.push(`Top improvements: ${review.top_improvements.length}`);
  lines.push("");
  lines.push("## Current Decision");
  if (review.current.actions.length === 0) {
    lines.push(`- ${review.current.status_message}`);
  } else {
    for (const action of review.current.actions) lines.push(`- ${action}`);
  }
  lines.push("");
  lines.push("## Current Scope Readout");
  for (const item of asArray(review.current.readout)) lines.push(`- ${item}`);
  lines.push("");
  lines.push("## Current Scope Snapshot");
  const snapshot = review.current.snapshot || {};
  if (!snapshot.enabled) {
    lines.push("- none");
  } else {
    const scopeName = `${snapshot.work_item || ""}${snapshot.iteration ? `/${snapshot.iteration}` : ""}` || "unknown";
    lines.push(`- scope: ${scopeName}`);
    lines.push(`- records: ${snapshot.records || 0}`);
    lines.push(`- profiled/wall-clock: ${formatMs(snapshot.profiled_ms || 0)} / ${formatMs(snapshot.wall_clock_ms || 0)} (${formatPercent(snapshot.coverage_ratio)})`);
    lines.push(`- telemetry gaps: context=${snapshot.missing_context_inputs || 0}, work_item=${snapshot.missing_work_item_records || 0}, tools=${snapshot.missing_tool_records || 0}`);
    lines.push(`- failures: unresolved=${snapshot.unresolved_failed_records || 0}, recovered=${snapshot.recovered_failed_records || 0}`);
    const largestGaps = asArray(snapshot.largest_gaps);
    if (largestGaps.length > 0) {
      lines.push("- largest gaps:");
      for (const gap of largestGaps) {
        lines.push(`  - ${formatMs(gap.duration_ms || 0)} from ${gap.start_ts || "unknown"} to ${gap.end_ts || "unknown"}`);
      }
    }
  }
  lines.push("");
  lines.push("## Current Scope Tool Use");
  const currentTools = asArray(review.current.tool_use_summary);
  if (currentTools.length === 0) {
    lines.push("- none");
  } else {
    const currentRuntimeTotalMs = Number(review.current.tool_runtime_total_ms || sumField(runtimeToolSummary(currentTools), "runtime_ms"));
    const currentCapturedTotalMs = Number(review.current.captured_elapsed_total_ms || sumField(capturedElapsedSummary(currentTools), "captured_elapsed_ms"));
    if (currentRuntimeTotalMs > 0) lines.push(`- total current runtime: ${formatMs(currentRuntimeTotalMs)}`);
    if (currentCapturedTotalMs > 0) lines.push(`- total current captured elapsed: ${formatMs(currentCapturedTotalMs)}`);
    for (const item of currentTools) {
      const captured = toolCapturedElapsed(item);
      const duration = captured ? toolCapturedElapsedMs(item) : toolRuntimeMs(item);
      const shareTotal = captured ? currentCapturedTotalMs : currentRuntimeTotalMs;
      lines.push(`- ${item.tool || "unknown"}: ${item.records || 0} record(s), ${formatMs(duration)} ${toolDurationSuffix(item)}, share=${formatShare(duration, shareTotal)}, failed=${item.failed || 0}, waste/rework=${item.waste_or_rework || 0}`);
    }
  }
  lines.push("");
  lines.push("## Current Scope Context Use");
  const currentContext = review.current.context_use_summary || { hotspots: [], high_context: [], missing_inputs: [] };
  if (asArray(currentContext.hotspots).length === 0 && asArray(currentContext.high_context).length === 0 && asArray(currentContext.missing_inputs).length === 0) {
    lines.push("- none");
  } else {
    if (asArray(currentContext.hotspots).length > 0) {
      const totalChars = Number(currentContext.total_hotspot_chars || sumField(currentContext.hotspots, "chars"));
      lines.push(`- total hotspot chars: ${totalChars}`);
      lines.push("- hotspots:");
      for (const item of asArray(currentContext.hotspots)) {
        const chars = Number(item.chars || 0);
        lines.push(`  - ${item.path || "unknown"}: ${chars} chars, share=${formatShare(chars, totalChars)}`);
      }
    }
    if (asArray(currentContext.high_context).length > 0) {
      lines.push("- high context:");
      for (const item of asArray(currentContext.high_context)) lines.push(`  - line ${item.line || 0}: ${item.intent || ""}`);
    }
    if (asArray(currentContext.missing_inputs).length > 0) {
      lines.push("- missing inputs:");
      for (const item of asArray(currentContext.missing_inputs)) lines.push(`  - line ${item.line || 0} [${item.context_risk || "unknown"}]: ${item.intent || ""}`);
    }
  }
  lines.push("");
  lines.push("## Current Scope Validation");
  const currentValidationBatches = asArray(review.current.validation_batches);
  if (currentValidationBatches.length === 0) {
    lines.push("- none");
  } else {
    for (const item of currentValidationBatches) {
      const result = Number(item.failed || 0) > 0 ? `${item.failed} failed` : "pass";
      const changes = asArray(item.changes).length > 0 ? asArray(item.changes).join(", ") : "unknown";
      lines.push(`- ${item.batch_id || "unknown"}: ${item.records || 0} record(s), ${formatMs(item.duration_ms)}, ${result}, risk=${item.risk || "unknown"}, changes=${changes}, broad/final=${item.broad_final_commands || 0}`);
    }
  }
  lines.push("");
  lines.push("## Historical Lessons");
  if (review.historical_lessons.length === 0) {
    lines.push("- none");
  } else {
    for (const lesson of review.historical_lessons) {
      lines.push(`- ${lesson.type} (${lesson.current_action})`);
      lines.push(`  - symptom: ${lesson.symptom}`);
      lines.push(`  - fix: ${lesson.fix}`);
    }
  }
  lines.push("");
  lines.push("## Tool Runtime Review");
  const tools = asArray(review.tool_use_summary);
  const runtimeTools = asArray(review.tool_runtime_summary).length > 0 ? asArray(review.tool_runtime_summary) : runtimeToolSummary(tools);
  const runtimeTotalMs = Number(review.tool_runtime_total_ms || sumField(runtimeTools, "runtime_ms"));
  if (runtimeTools.length === 0) {
    lines.push("- none");
  } else {
    lines.push(`- total runtime: ${formatMs(runtimeTotalMs)}`);
    for (const item of runtimeTools) {
      const duration = Number(item.runtime_ms || item.duration_ms || 0);
      lines.push(`- ${item.tool || "unknown"}: ${item.records || 0} record(s), ${formatMs(duration)} ${toolDurationSuffix(item)}, share=${formatShare(duration, runtimeTotalMs)}, failed=${item.failed || 0}, waste/rework=${item.waste_or_rework || 0}`);
    }
  }
  lines.push("");
  lines.push("## Captured Elapsed Review");
  const capturedElapsed = asArray(review.captured_elapsed_summary).length > 0 ? asArray(review.captured_elapsed_summary) : capturedElapsedSummary(tools);
  const capturedElapsedTotalMs = Number(review.captured_elapsed_total_ms || sumField(capturedElapsed, "captured_elapsed_ms"));
  if (capturedElapsed.length === 0) {
    lines.push("- none");
  } else {
    lines.push(`- total captured elapsed: ${formatMs(capturedElapsedTotalMs)}`);
    for (const item of capturedElapsed) {
      const duration = Number(item.captured_elapsed_ms || item.duration_ms || 0);
      lines.push(`- ${item.tool || "unknown"}: ${item.records || 0} record(s), ${formatMs(duration)} captured elapsed, share=${formatShare(duration, capturedElapsedTotalMs)}`);
    }
  }
  lines.push("");
  lines.push("## Context Use Review");
  const contextSummary = review.context_use_summary || { hotspots: [], missing_inputs: [] };
  if (asArray(contextSummary.hotspots).length === 0 && asArray(contextSummary.missing_inputs).length === 0) {
    lines.push("- none");
  } else {
    if (asArray(contextSummary.hotspots).length > 0) {
      const totalChars = Number(contextSummary.total_hotspot_chars || sumField(contextSummary.hotspots, "chars"));
      lines.push(`- total hotspot chars: ${totalChars}`);
      lines.push("- hotspots:");
      for (const item of asArray(contextSummary.hotspots)) {
        const chars = Number(item.chars || 0);
        lines.push(`  - ${item.path || "unknown"}: ${chars} chars, share=${formatShare(chars, totalChars)}`);
      }
    }
    if (asArray(contextSummary.missing_inputs).length > 0) {
      lines.push("- missing inputs:");
      for (const item of asArray(contextSummary.missing_inputs)) lines.push(`  - line ${item.line || 0} [${item.context_risk || "unknown"}]: ${item.intent || ""}`);
    }
  }
  lines.push("");
  lines.push("## Recovered Failure Review");
  const recovered = asArray(review.recovered_failure_classification);
  if (recovered.length === 0) {
    lines.push("- none");
  } else {
    for (const item of recovered) {
      lines.push(`- line ${item.line || 0} -> ${item.classification || "unknown"}: ${item.reason || ""}`);
      lines.push(`  - next: ${item.next_action || ""}`);
    }
  }
  lines.push("");
  lines.push("## Repeated Command Review");
  const repeatedCommands = review.repeated_commands || {};
  const byScope = asArray(repeatedCommands.by_scope);
  const repeatedTotal = Number(repeatedCommands.repeated_total_occurrences || sumField(byScope, "count"));
  if (byScope.length === 0) {
    lines.push("- none");
  } else {
    lines.push(`- total repeated occurrences: ${repeatedTotal}`);
    for (const item of byScope) {
      const count = Number(item.count || 0);
      lines.push(`- ${item.scope || "unknown"}: ${count}, share=${formatShare(count, repeatedTotal)}`);
    }
    if (Number(repeatedCommands.unbatched_broad_final_occurrences || 0) > 0) {
      lines.push(`- unbatched broad/final occurrences: ${repeatedCommands.unbatched_broad_final_occurrences}`);
    }
  }
  const validationBatches = asArray(repeatedCommands.validation_batches);
  const classifications = asArray(repeatedCommands.classification);
  const classificationTotal = Number(repeatedCommands.classification_total_occurrences || sumField(classifications, "count"));
  if (classifications.length > 0) {
    lines.push(`- classified repeated occurrences: ${classificationTotal}`);
    lines.push("- classification:");
    for (const item of classifications) {
      const count = Number(item.count || 0);
      lines.push(`  - ${count}x ${item.classification || "unknown"}, share=${formatShare(count, classificationTotal)} [${item.scope || "unknown"}]: ${item.command || ""}`);
    }
  }
  if (validationBatches.length > 0) {
    lines.push("- validation batches:");
    for (const item of validationBatches) lines.push(`  - ${item.batch_id || "unknown"}: ${item.records || 0} record(s), broad/final=${item.broad_final_commands || 0}, failed=${item.failed || 0}`);
  }
  lines.push("");
  lines.push("## Top Improvements");
  for (const [index, improvement] of review.top_improvements.entries()) lines.push(`${index + 1}. ${improvement}`);
  lines.push("");
  lines.push("## Caveats");
  for (const caveat of review.caveats) lines.push(`- ${caveat}`);
  return `${lines.join("\n")}\n`;
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help) usage();
const draftFile = positionals[0];
if (!draftFile) usage();
const outputFile = stringArg(values, "output", "");
const jsonOutputFile = stringArg(values, "json-output", "");

let draft;
try {
  draft = readJson(draftFile);
} catch (error) {
  console.error(`reflection review failed for ${draftFile}: ${error.message}`);
  process.exit(1);
}

const review = buildReview(draft, draftFile);
const rendered = renderMarkdown(review, draftFile);

if (outputFile) {
  const target = resolve(outputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, rendered, "utf8");
}
if (jsonOutputFile) {
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(review, null, 2)}\n`, "utf8");
}

process.stdout.write(rendered);
