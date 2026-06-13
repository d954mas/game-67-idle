#!/usr/bin/env node
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/followups.mjs <review.json> [--output <followups.md>] [--json-output <followups.json>]

Reads review.mjs --json-output and generates reviewable follow-up action drafts.
It does not create task files.`);
  process.exit(2);
}

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function addSuggestion(suggestions, suggestion) {
  if (!suggestion || !suggestion.title) return;
  if (suggestions.some((item) => item.title === suggestion.title)) return;
  suggestions.push(suggestion);
}

function commandList(commands, limit = 5) {
  return asArray(commands)
    .slice(0, limit)
    .map((item) => `${item.count || 1}x ${item.command || item}`)
    .join("; ");
}

function formatPercent(value) {
  return Number.isFinite(value) ? `${(value * 100).toFixed(1)}%` : "unknown";
}

function unbatchedBroadFinalCommands(container) {
  if (Array.isArray(container?.repeated_unbatched_broad_final_commands)) {
    return container.repeated_unbatched_broad_final_commands;
  }
  return asArray(container?.repeated_broad_final_commands);
}

function buildSuggestions(review) {
  const suggestions = [];
  const suppressedHistorical = [];
  const findingTypes = new Set(asArray(review.findings).map((finding) => finding.type));
  const currentScope = review.current_scope && review.current_scope.enabled ? review.current_scope : null;
  const useCurrentScope = Boolean(currentScope && currentScope.records > 0);

  function currentScopeClean(field) {
    if (!useCurrentScope) return false;
    if (field === "repeated_broad_final") return unbatchedBroadFinalCommands(currentScope).length === 0;
    if (field === "missing_context_inputs") return Number(currentScope.missing_context_inputs || 0) === 0;
    if (field === "missing_work_item_metadata") return Number(currentScope.missing_work_item_records || 0) === 0;
    if (field === "low_profile_coverage") return currentScope.low_profile_coverage !== true;
    if (field === "recovered_failed_records") return asArray(currentScope.recovered_failed_records).length === 0;
    return false;
  }

  if (findingTypes.has("repeated_broad_final") || unbatchedBroadFinalCommands(review).length > 0) {
    if (currentScopeClean("repeated_broad_final")) {
      suppressedHistorical.push("repeated_broad_final_commands");
    } else {
      const commands = useCurrentScope ? unbatchedBroadFinalCommands(currentScope) : unbatchedBroadFinalCommands(review);
      addSuggestion(suggestions, {
      title: "Reduce repeated broad/final validation",
      priority: "P1",
      tags: ["pipeline", "validation", "profiling", "automation"],
      source: useCurrentScope ? "current_scope.repeated_unbatched_broad_final_commands" : "repeated_unbatched_broad_final_commands",
      why: `Unbatched broad/final commands repeated: ${commandList(commands) || "unknown"}.`,
      done_when: [
        "`node tools/ai.mjs validate --change <kind> --risk <risk>` is the default path for broad/final gates.",
        "A future profile shows broad/final repeats only after a failed gate, changed risk, or final batch boundary.",
      ],
      next_action: "Use `node tools/ai.mjs validate --change <kind> --risk <risk>` for the next validation loop and inspect validation batch evidence before adding more rules.",
      });
    }
  }

  if (findingTypes.has("missing_context_inputs") || asArray(review.missing_context_inputs).length > 0) {
    if (currentScopeClean("missing_context_inputs")) {
      suppressedHistorical.push("missing_context_inputs");
    } else {
      const missingCount = useCurrentScope ? Number(currentScope.missing_context_inputs || 0) : asArray(review.missing_context_inputs).length;
      addSuggestion(suggestions, {
      title: "Eliminate missing context input details",
      priority: "P1",
      tags: ["pipeline", "profiling", "context", "reflection"],
      source: useCurrentScope ? "current_scope.missing_context_inputs" : "missing_context_inputs",
      why: `${missingCount} medium/high context record(s) lacked measured context_inputs.`,
      done_when: [
        "Medium/high context events use context.mjs or explicit context_inputs.",
        "Profile review shows no missing context input details for new records.",
      ],
      next_action: "Use context.mjs for local files and update any wrapper or habit that records context_risk without context_inputs.",
      });
    }
  }

  if (findingTypes.has("missing_work_item_metadata")) {
    if (currentScopeClean("missing_work_item_metadata")) {
      suppressedHistorical.push("missing_work_item_metadata");
    } else {
    const missingFinding = asArray(review.findings).find((finding) => finding.type === "missing_work_item_metadata");
    addSuggestion(suggestions, {
      title: "Add work item metadata to future profile events",
      priority: "P1",
      tags: ["pipeline", "profiling", "context", "automation"],
      source: useCurrentScope ? "current_scope.missing_work_item_metadata" : "missing_work_item_metadata",
      why: useCurrentScope
        ? `${Number(currentScope.missing_work_item_records || 0)} current-scope record(s) lack work_item metadata.`
        : missingFinding?.message || "Multi-task profiles have records without work_item metadata.",
      done_when: [
        "Substantial profile events are started with `node tools/ai.mjs start <work-item> <iteration>` or `node tools/ai.mjs focus <iteration>`.",
        "A future profile review can separate repeated validation by work item.",
      ],
      next_action: "Run `node tools/ai.mjs start <work-item> <iteration>` for a new task or `node tools/ai.mjs focus <iteration>` for the next slice in the same task.",
    });
    }
  }

  if (findingTypes.has("low_profile_coverage")) {
    if (currentScopeClean("low_profile_coverage")) {
      suppressedHistorical.push("low_profile_coverage");
    } else {
    const coverage = review.wall_clock_coverage || {};
    addSuggestion(suggestions, {
      title: "Raise AI profile wall-clock coverage",
      priority: "P1",
      tags: ["pipeline", "profiling", "reflection", "observability"],
      source: useCurrentScope ? "current_scope.wall_clock_coverage" : "wall_clock_coverage",
      why: `Profile coverage was ${formatPercent((useCurrentScope ? currentScope.wall_clock_coverage : coverage).coverage_ratio)} across the recorded wall-clock span.`,
      done_when: [
        "Retrospectives explicitly explain low-coverage periods or mark them as unknown.",
        "Future long manual/research/design stretches add `node tools/ai.mjs checkpoint \"<intent>\"` records.",
      ],
      next_action: "Inspect wall_clock_coverage and use `node tools/ai.mjs checkpoint \"<intent>\"` after long manual/research/design stretches.",
    });
    }
  }

  if (findingTypes.has("failed_records") || asArray(review.unresolved_failed_records).length > 0) {
    addSuggestion(suggestions, {
      title: "Analyze failed AI profile records",
      priority: "P1",
      tags: ["pipeline", "profiling", "debug", "validation"],
      source: "unresolved_failed_records",
      why: `${asArray(review.unresolved_failed_records).length} unresolved failed record(s) need owner/decision.`,
      done_when: [
        "Each failed/blocked record is classified as code issue, environment issue, bad validation scope, or expected negative test.",
        "Recurring failure causes are converted into a rule, preflight, or task.",
      ],
      next_action: "Open the failed_or_blocked entries and classify the cause before continuing broad validation.",
    });
  }

  if (findingTypes.has("recovered_failed_records") || asArray(review.recovered_failed_records).length > 0) {
    if (currentScopeClean("recovered_failed_records")) {
      suppressedHistorical.push("recovered_failed_records");
    } else {
      addSuggestion(suggestions, {
      title: "Classify recovered AI profile failures",
      priority: "P2",
      tags: ["pipeline", "profiling", "reflection", "learning"],
      source: useCurrentScope ? "current_scope.recovered_failed_records" : "recovered_failed_records",
      why: `${asArray(useCurrentScope ? currentScope.recovered_failed_records : review.recovered_failed_records).length} failed record(s) later passed and should be treated as recovered rework or learning.`,
      done_when: [
        "Recovered failures are named in the retrospective with cause and faster path.",
        "Only recurring recovered failures become rules, preflights, or tasks.",
      ],
      next_action: "Review recovered_failed_records and decide whether they were useful negative feedback, avoidable rework, or tool noise.",
      });
    }
  }

  if (findingTypes.has("waste_or_rework") || asArray(review.waste_or_rework).length > 0) {
    addSuggestion(suggestions, {
      title: "Convert AI profile waste/rework into a process guard",
      priority: "P1",
      tags: ["pipeline", "profiling", "reflection", "process"],
      source: "waste_or_rework",
      why: `${asArray(review.waste_or_rework).length} waste/rework record(s) were found.`,
      done_when: [
        "Each waste/rework reason maps to a concrete prevention point: rule, skill, validator, checklist, or task.",
        "The prevention point is committed or explicitly deferred.",
      ],
      next_action: "Review waste_or_rework entries and choose the smallest prevention mechanism for each recurring cause.",
    });
  }

  if (findingTypes.has("repeated_commands") && !findingTypes.has("repeated_broad_final")) {
    addSuggestion(suggestions, {
      title: "Review repeated scoped/preflight commands",
      priority: "P2",
      tags: ["pipeline", "profiling", "validation"],
      source: "repeated_commands",
      why: `Repeated commands by scope: ${asArray(review.repeated_commands_by_scope)
        .map((item) => `${item.scope}:${item.count}`)
        .join(", ") || "unknown"}.`,
      done_when: [
        "Repeated scoped/preflight commands are either justified by fresh edits or batched.",
        "Unnecessary repeats become a validation ladder or wrapper rule.",
      ],
      next_action: "Check whether repeated scoped/preflight commands followed fresh edits; if not, add batching guidance.",
    });
  }

  if (suggestions.length === 0) {
    addSuggestion(suggestions, {
      title: "Use clean AI profile as baseline",
      priority: "P3",
      tags: ["pipeline", "profiling", "baseline"],
      source: "clean_profile",
      why: "No follow-up findings were detected in the review JSON.",
      done_when: ["A later profile is compared against this baseline."],
      next_action: "Capture the review with `node tools/ai_profile/capture_baseline.mjs <review.json> --label <name>`, then compare a later review with compare_reviews.mjs.",
    });
  }

  return { suggestions, suppressedHistorical };
}

function renderMarkdown(reviewFile, review, suggestions, suppressedHistorical) {
  const lines = [];
  lines.push(`# AI Profile Follow-up Drafts - ${basename(reviewFile)}`);
  lines.push("");
  lines.push(`Profile: ${review.profile || "(unknown)"}`);
  lines.push(`Review schema: ${review.schema_version || "(unknown)"}`);
  lines.push(`Suggestions: ${suggestions.length}`);
  if (suppressedHistorical.length > 0) {
    lines.push(`Suppressed historical-only findings: ${suppressedHistorical.join(", ")}`);
  }
  lines.push("");
  lines.push("These are draft actions. Promote only the items that are still relevant after checking current tasks.");
  for (const suggestion of suggestions) {
    lines.push("");
    lines.push(`## ${suggestion.title}`);
    lines.push("");
    lines.push(`- priority: ${suggestion.priority}`);
    lines.push(`- tags: ${suggestion.tags.join(", ")}`);
    lines.push(`- source: ${suggestion.source}`);
    lines.push(`- why: ${suggestion.why}`);
    lines.push(`- next action: ${suggestion.next_action}`);
    lines.push("- done when:");
    for (const item of suggestion.done_when) lines.push(`  - ${item}`);
  }
  return `${lines.join("\n")}\n`;
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help) usage();
const reviewFile = positionals[0];
if (!reviewFile) usage();
const outputFile = stringArg(values, "output", "");
const jsonOutputFile = stringArg(values, "json-output", "");

let review;
try {
  review = JSON.parse(readFileSync(reviewFile, "utf8"));
} catch (error) {
  console.error(`profile followups failed for ${reviewFile}: ${error.message}`);
  process.exit(1);
}

const { suggestions, suppressedHistorical } = buildSuggestions(review);
const rendered = renderMarkdown(reviewFile, review, suggestions, suppressedHistorical);
if (outputFile) {
  const target = resolve(outputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, rendered, "utf8");
}
if (jsonOutputFile) {
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify({ schema_version: 1, review: reviewFile, suggestions, suppressed_historical_findings: suppressedHistorical }, null, 2)}\n`, "utf8");
}
process.stdout.write(rendered);
