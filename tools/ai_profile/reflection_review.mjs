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

function topImprovements(draft, currentClean) {
  const improvements = [];
  const historical = asArray(draft.historical_lessons);
  const hasLesson = (type) => historical.some((lesson) => lesson.type === type);
  if (!currentClean) {
    improvements.push("Resolve current-scope findings, regressions, and pending follow-ups before treating historical lessons as process work.");
  }
  if (hasLesson("repeated_commands")) {
    improvements.push("Use repeated_command_classification to triage repeats before adding process tasks.");
  }
  if (asArray(draft.repeated_commands?.unbatched_broad_final_commands).length > 0 || hasLesson("repeated_broad_final")) {
    const occurrences = Number(draft.repeated_commands?.unbatched_broad_final_occurrences || 0);
    const evidenceScope = currentClean ? "historical whole-profile review shows" : "review shows";
    improvements.push(`Batch broad/final validation with node tools/ai.mjs validate and rerun it only after a failed gate, changed risk, or final handoff${occurrences > 0 ? `; ${evidenceScope} ${occurrences} unbatched broad/final occurrence(s)` : ""}.`);
  }
  if (asArray(draft.repeated_commands?.validation_batches).length > 0) {
    improvements.push("Use validation batch evidence to separate planned validation runs from ad hoc repeated commands.");
  }
  if (asArray(draft.tool_use_summary).length > 0) {
    improvements.push("Use tool_use_summary to explain which tool classes consumed time, failed, or produced context.");
  }
  if (asArray(draft.context_use_summary?.hotspots).length > 0) {
    const hotspot = draft.context_use_summary.hotspots[0];
    improvements.push(`Use context_use_summary to explain context pressure; largest input is ${hotspot.path || "unknown"} (${hotspot.chars || 0} chars).`);
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
      tool_use_summary: asArray(draft.current_state?.current_scope_tool_use_summary),
      context_use_summary: draft.current_state?.current_scope_context_use_summary || { hotspots: [], high_context: [], missing_inputs: [] },
    },
    historical_lessons: historicalLessons,
    suppressed_historical_findings: asArray(draft.suppressed_historical_findings),
    repeated_commands: draft.repeated_commands || {},
    tool_use_summary: asArray(draft.tool_use_summary),
    context_use_summary: draft.context_use_summary || { hotspots: [], missing_inputs: [] },
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
  lines.push("## Current Scope Tool Use");
  const currentTools = asArray(review.current.tool_use_summary);
  if (currentTools.length === 0) {
    lines.push("- none");
  } else {
    for (const item of currentTools) lines.push(`- ${item.tool || "unknown"}: ${item.records || 0} record(s), ${formatMs(item.duration_ms)}, failed=${item.failed || 0}, waste/rework=${item.waste_or_rework || 0}`);
  }
  lines.push("");
  lines.push("## Current Scope Context Use");
  const currentContext = review.current.context_use_summary || { hotspots: [], high_context: [], missing_inputs: [] };
  if (asArray(currentContext.hotspots).length === 0 && asArray(currentContext.high_context).length === 0 && asArray(currentContext.missing_inputs).length === 0) {
    lines.push("- none");
  } else {
    if (asArray(currentContext.hotspots).length > 0) {
      lines.push("- hotspots:");
      for (const item of asArray(currentContext.hotspots)) lines.push(`  - ${item.path || "unknown"}: ${item.chars || 0} chars`);
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
  lines.push("## Tool Use Review");
  const tools = asArray(review.tool_use_summary);
  if (tools.length === 0) {
    lines.push("- none");
  } else {
    for (const item of tools) lines.push(`- ${item.tool || "unknown"}: ${item.records || 0} record(s), ${formatMs(item.duration_ms)}, failed=${item.failed || 0}, waste/rework=${item.waste_or_rework || 0}`);
  }
  lines.push("");
  lines.push("## Context Use Review");
  const contextSummary = review.context_use_summary || { hotspots: [], missing_inputs: [] };
  if (asArray(contextSummary.hotspots).length === 0 && asArray(contextSummary.missing_inputs).length === 0) {
    lines.push("- none");
  } else {
    if (asArray(contextSummary.hotspots).length > 0) {
      lines.push("- hotspots:");
      for (const item of asArray(contextSummary.hotspots)) lines.push(`  - ${item.path || "unknown"}: ${item.chars || 0} chars`);
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
  const byScope = asArray(review.repeated_commands.by_scope);
  if (byScope.length === 0) {
    lines.push("- none");
  } else {
    for (const item of byScope) lines.push(`- ${item.scope || "unknown"}: ${item.count || 0}`);
    if (Number(review.repeated_commands.unbatched_broad_final_occurrences || 0) > 0) {
      lines.push(`- unbatched broad/final occurrences: ${review.repeated_commands.unbatched_broad_final_occurrences}`);
    }
  }
  const validationBatches = asArray(review.repeated_commands.validation_batches);
  const classifications = asArray(review.repeated_commands.classification);
  if (classifications.length > 0) {
    lines.push("- classification:");
    for (const item of classifications) lines.push(`  - ${item.count || 0}x ${item.classification || "unknown"} [${item.scope || "unknown"}]: ${item.command || ""}`);
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
