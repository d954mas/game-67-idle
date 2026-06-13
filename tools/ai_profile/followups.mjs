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

function buildSuggestions(review) {
  const suggestions = [];
  const findingTypes = new Set(asArray(review.findings).map((finding) => finding.type));

  if (findingTypes.has("repeated_broad_final") || asArray(review.repeated_broad_final_commands).length > 0) {
    addSuggestion(suggestions, {
      title: "Reduce repeated broad/final validation",
      priority: "P1",
      tags: ["pipeline", "validation", "profiling", "automation"],
      source: "repeated_broad_final_commands",
      why: `Broad/final commands repeated: ${commandList(review.repeated_broad_final_commands) || "unknown"}.`,
      done_when: [
        "Validation planner or review rules identify when broad/final gates are allowed to rerun.",
        "A future profile shows broad/final repeats only after a failed gate, changed risk, or final batch boundary.",
      ],
      next_action: "Inspect the repeated broad/final commands and decide whether to add a batching rule, narrower preflight, or review threshold.",
    });
  }

  if (findingTypes.has("missing_context_inputs") || asArray(review.missing_context_inputs).length > 0) {
    addSuggestion(suggestions, {
      title: "Eliminate missing context input details",
      priority: "P1",
      tags: ["pipeline", "profiling", "context", "reflection"],
      source: "missing_context_inputs",
      why: `${asArray(review.missing_context_inputs).length} medium/high context record(s) lacked measured context_inputs.`,
      done_when: [
        "Medium/high context events use context.mjs or explicit context_inputs.",
        "Profile review shows no missing context input details for new records.",
      ],
      next_action: "Use context.mjs for local files and update any wrapper or habit that records context_risk without context_inputs.",
    });
  }

  if (findingTypes.has("missing_work_item_metadata")) {
    const missingFinding = asArray(review.findings).find((finding) => finding.type === "missing_work_item_metadata");
    addSuggestion(suggestions, {
      title: "Add work item metadata to future profile events",
      priority: "P1",
      tags: ["pipeline", "profiling", "context", "automation"],
      source: "missing_work_item_metadata",
      why: missingFinding?.message || "Multi-task profiles have records without work_item metadata.",
      done_when: [
        "Substantial profile events include work-item metadata from scope.mjs, AI_PROFILE_* env vars, or explicit flags.",
        "A future profile review can separate repeated validation by work item.",
      ],
      next_action: "Run `node tools/ai_profile/scope.mjs set --work-item <id> --iteration <name>` for the next focused session, or use explicit flags for exceptions.",
    });
  }

  if (findingTypes.has("low_profile_coverage")) {
    const coverage = review.wall_clock_coverage || {};
    addSuggestion(suggestions, {
      title: "Raise AI profile wall-clock coverage",
      priority: "P1",
      tags: ["pipeline", "profiling", "reflection", "observability"],
      source: "wall_clock_coverage",
      why: `Profile coverage was ${formatPercent(coverage.coverage_ratio)} across the recorded wall-clock span.`,
      done_when: [
        "Retrospectives explicitly explain low-coverage periods or mark them as unknown.",
        "Future long manual/research/design stretches add sparse event.mjs checkpoints.",
      ],
      next_action: "Inspect wall_clock_coverage and decide whether the next cycle needs checkpoint prompts, a wrapper, or a lower-overhead capture habit.",
    });
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
    addSuggestion(suggestions, {
      title: "Classify recovered AI profile failures",
      priority: "P2",
      tags: ["pipeline", "profiling", "reflection", "learning"],
      source: "recovered_failed_records",
      why: `${asArray(review.recovered_failed_records).length} failed record(s) later passed and should be treated as recovered rework or learning.`,
      done_when: [
        "Recovered failures are named in the retrospective with cause and faster path.",
        "Only recurring recovered failures become rules, preflights, or tasks.",
      ],
      next_action: "Review recovered_failed_records and decide whether they were useful negative feedback, avoidable rework, or tool noise.",
    });
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
      next_action: "Keep the review JSON as scratch evidence and compare trend after the next long session.",
    });
  }

  return suggestions;
}

function renderMarkdown(reviewFile, review, suggestions) {
  const lines = [];
  lines.push(`# AI Profile Follow-up Drafts - ${basename(reviewFile)}`);
  lines.push("");
  lines.push(`Profile: ${review.profile || "(unknown)"}`);
  lines.push(`Review schema: ${review.schema_version || "(unknown)"}`);
  lines.push(`Suggestions: ${suggestions.length}`);
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

const suggestions = buildSuggestions(review);
const rendered = renderMarkdown(reviewFile, review, suggestions);
if (outputFile) {
  const target = resolve(outputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, rendered, "utf8");
}
if (jsonOutputFile) {
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify({ schema_version: 1, review: reviewFile, suggestions }, null, 2)}\n`, "utf8");
}
process.stdout.write(rendered);
