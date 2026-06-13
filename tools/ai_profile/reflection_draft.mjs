#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/reflection_draft.mjs <reflection_packet.json> [--output <draft.md>] [--json-output <draft.json>]

Builds a structured retrospective starter from a reflection packet and its
referenced review JSON. This is scratch evidence, not the final reflection.`);
  process.exit(2);
}

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function loadOptionalJson(path) {
  if (!path || !existsSync(path)) return null;
  try {
    return readJson(path);
  } catch {
    return null;
  }
}

function formatMs(ms) {
  if (!Number.isFinite(ms) || ms <= 0) return "0s";
  const seconds = ms / 1000;
  if (seconds < 90) return `${seconds.toFixed(1)}s`;
  const minutes = seconds / 60;
  if (minutes < 90) return `${minutes.toFixed(1)}m`;
  return `${(minutes / 60).toFixed(2)}h`;
}

function compactFinding(finding) {
  if (!finding || typeof finding !== "object") return { type: "finding", message: String(finding || "") };
  return {
    type: finding.type || "finding",
    message: finding.message || finding.label || "",
    ...finding,
  };
}

function repeatedCommandSummary(review) {
  const commands = asArray(review?.repeated_commands);
  const byScope = asArray(review?.repeated_commands_by_scope);
  const broadFinal = asArray(review?.repeated_broad_final_commands);
  const unbatchedBroadFinal = asArray(review?.repeated_unbatched_broad_final_commands);
  const batchedBroadFinal = asArray(review?.batched_broad_final_commands);
  const broadFinalByWorkItem = asArray(review?.repeated_broad_final_by_work_item);
  const validationBatches = asArray(review?.validation_batches);
  const classifications = asArray(review?.repeated_command_classification);
  return {
    total_distinct: commands.length,
    by_scope: byScope,
    classification: classifications.slice(0, 8).map((item) => ({
      command: item.command || "",
      count: Number(item.count || 0),
      scope: item.scope || "unknown",
      classification: item.classification || "needs_manual_classification",
      reason: item.reason || "",
      next_action: item.next_action || "",
      batched: Number(item.batched || 0),
      unbatched: Number(item.unbatched || 0),
      failed: Number(item.failed || 0),
    })),
    top_commands: commands.slice(0, 8).map((item) => ({
      command: item.command || "",
      count: Number(item.count || 0),
      scope: item.scope || "unknown",
    })),
    broad_final_commands: broadFinal.slice(0, 8).map((item) => ({
      command: item.command || "",
      count: Number(item.count || 0),
      scope: item.scope || "broad/final",
    })),
    unbatched_broad_final_commands: unbatchedBroadFinal.slice(0, 8).map((item) => ({
      command: item.command || "",
      count: Number(item.count || 0),
      scope: item.scope || "broad/final",
    })),
    batched_broad_final_commands: batchedBroadFinal.slice(0, 8).map((item) => ({
      command: item.command || "",
      count: Number(item.count || 0),
      scope: item.scope || "broad/final",
    })),
    broad_final_by_work_item: broadFinalByWorkItem.slice(0, 8).map((item) => ({
      work_item: item.work_item || item.segment || "",
      command: item.command || "",
      count: Number(item.count || 0),
    })),
    validation_batches: validationBatches.slice(0, 8).map((item) => ({
      batch_id: item.batch_id || "",
      records: Number(item.records || 0),
      duration_ms: Number(item.duration_ms || 0),
      failed: Number(item.failed || 0),
      broad_final_commands: Number(item.broad_final_commands || 0),
      risk: item.risk || "",
      changes: asArray(item.changes),
    })),
  };
}

function toolUseSummary(review) {
  return asArray(review?.tool_use_summary).slice(0, 8).map((item) => ({
    tool: item.tool || "",
    records: Number(item.records || 0),
    duration_ms: Number(item.duration_ms || 0),
    failed: Number(item.failed || 0),
    blocked: Number(item.blocked || 0),
    waste_or_rework: Number(item.waste_or_rework || 0),
    contexts: Number(item.contexts || 0),
    commands: Number(item.commands || 0),
  }));
}

function scopeSummaryText(summary) {
  if (!summary.by_scope.length) return "no scope breakdown";
  return summary.by_scope.map((item) => `${item.scope || "unknown"}=${item.count || 0}`).join(", ");
}

function lessonForFinding(finding, context = {}) {
  const type = finding.type || "finding";
  const message = finding.message || "";
  const repeatedSummary = context.repeated_commands || repeatedCommandSummary(null);
  const templates = {
    repeated_commands: {
      symptom: `${message} Scope mix: ${scopeSummaryText(repeatedSummary)}. Planned validation batches: ${repeatedSummary.validation_batches.length}.`,
      cause: "Commands are being rerun across the whole profile; the generated classification separates planned validation, validation-waste risk, failure/rework signals, scoped guardrail reruns, and manual-review cases.",
      fix: "Use repeated_command_classification before adding process tasks; batch broad/final gates and keep preflight/scoped reruns close to changed files.",
    },
    repeated_broad_final: {
      symptom: message,
      cause: "Unbatched broad/final validation was repeated in the historical profile instead of being guarded by a validation plan or captured as a planned validation batch.",
      fix: "Use node tools/ai.mjs validate --change <kind> --risk <risk> before rerunning broad/final gates; treat planned validation batches separately from ad hoc repeats.",
    },
    missing_context_inputs: {
      symptom: message,
      cause: "Medium/high context reads were recorded without measured context_inputs.",
      fix: "Use node tools/ai.mjs context --path <file> for local files, or node tools/ai.mjs context -- <command> for read-only context commands.",
    },
    missing_work_item_metadata: {
      symptom: message,
      cause: "Older profile records were created before persistent work-item scope was reliable.",
      fix: "Start each focused work item with node tools/ai.mjs start, and use node tools/ai.mjs focus for later slices inside the same work item.",
    },
    low_profile_coverage: {
      symptom: message,
      cause: "Long manual/research/review stretches were not checkpointed in the historical profile.",
      fix: "Use node tools/ai.mjs checkpoint \"<intent>\" during long non-command stretches.",
    },
    recovered_failed_records: {
      symptom: message,
      cause: "Some failed commands later passed; they are historical rework or useful negative feedback, not current blockers.",
      fix: "Classify recovered failures in retrospectives and only promote recurring current-scope failures.",
    },
  };
  return {
    type,
    ...(templates[type] || {
      symptom: message,
      cause: "Cause needs human review against the linked profile evidence.",
      fix: "Decide whether this finding needs a rule, task, tool, or no action.",
    }),
  };
}

function buildDraft(packet, review) {
  const currentRegressions = asArray(packet.comparison?.current_regressions);
  const pending = asArray(packet.followups?.pending_suggestions);
  const satisfied = asArray(packet.followups?.satisfied_suggestions);
  const suppressed = asArray(packet.followups?.suppressed_historical_findings);
  const findings = asArray(review?.findings);
  const repeatedSummary = repeatedCommandSummary(review);
  const toolsSummary = toolUseSummary(review);
  const currentFindings = asArray(packet.current_scope?.findings).map(compactFinding);
  const currentActions = asArray(packet.current_scope?.suggested_actions);
  const historicalLessons = findings.map((finding) => lessonForFinding(finding, { repeated_commands: repeatedSummary }));
  const nextActions = [];
  if (currentRegressions.length > 0) {
    nextActions.push("Inspect current-scope regressions before writing the final retrospective.");
  }
  for (const finding of currentFindings) {
    nextActions.push(finding.message || `Resolve current-scope finding: ${finding.type}`);
  }
  for (const action of currentActions) {
    nextActions.push(action);
  }
  for (const suggestion of pending) {
    nextActions.push(suggestion.next_action || suggestion.title || "Review pending follow-up.");
  }
  if (nextActions.length === 0) {
    nextActions.push("Use this clean packet as the baseline for the next game-development iteration.");
  }

  return {
    schema_version: 1,
    packet: packet.profile || "",
    review: packet.artifacts?.review_json || "",
    readiness: asArray(packet.readiness),
    evidence: packet.artifacts || {},
    caveats: [
      "Generated draft only; edit with human/agent judgment before sharing.",
      "Packet and review artifacts are scratch evidence unless explicitly promoted.",
    ],
    current_state: {
      current_scope_findings: currentFindings,
      current_scope_actions: currentActions,
      baseline_verdict: packet.comparison?.verdict || "",
      current_regressions: currentRegressions,
      pending_followups: pending,
      satisfied_followups: satisfied,
    },
    historical_lessons: historicalLessons,
    suppressed_historical_findings: suppressed,
    repeated_commands: repeatedSummary,
    tool_use_summary: toolsSummary,
    next_cycle_actions: nextActions,
  };
}

function renderMarkdown(draft, packetFile) {
  const lines = [];
  lines.push(`# AI Development Reflection Draft - ${basename(packetFile)}`);
  lines.push("");
  lines.push("Draft status: generated starter from profiling artifacts; edit it with judgment before sharing.");
  lines.push(`Readiness: ${draft.readiness.join(", ") || "unknown"}`);
  lines.push(`Baseline verdict: ${draft.current_state.baseline_verdict || "unknown"}`);
  lines.push(`Current-scope findings: ${draft.current_state.current_scope_findings.length}`);
  lines.push(`Current-scope regressions: ${draft.current_state.current_regressions.length}`);
  lines.push(`Pending follow-ups: ${draft.current_state.pending_followups.length}`);
  lines.push(`Satisfied follow-ups: ${draft.current_state.satisfied_followups.length}`);
  if (
    draft.readiness.includes("ready")
    && draft.current_state.current_scope_findings.length === 0
    && draft.current_state.current_regressions.length === 0
    && draft.current_state.pending_followups.length === 0
  ) {
    lines.push("Current reflection state is clean: no active current-scope findings, regressions, or pending follow-ups.");
  }
  lines.push("");
  lines.push("## Current State");
  if (draft.current_state.current_scope_findings.length === 0) lines.push("- Current scope has no active findings.");
  for (const finding of draft.current_state.current_scope_findings) lines.push(`- ${finding.message || finding.type || "finding"}`);
  for (const action of draft.current_state.current_scope_actions) lines.push(`- action: ${action}`);
  if (draft.current_state.current_regressions.length > 0) {
    for (const item of draft.current_state.current_regressions) lines.push(`- regression: ${item.label || item.key || "unknown"}`);
  }
  lines.push("");
  lines.push("## Follow-ups");
  if (draft.current_state.pending_followups.length === 0) {
    lines.push("- pending: none");
  } else {
    for (const suggestion of draft.current_state.pending_followups) lines.push(`- pending [${suggestion.priority || "P?"}] ${suggestion.title || "(untitled)"}: ${suggestion.next_action || ""}`);
  }
  for (const suggestion of draft.current_state.satisfied_followups) lines.push(`- satisfied [${suggestion.priority || "P?"}] ${suggestion.title || "(untitled)"}: ${suggestion.packet_reason || ""}`);
  lines.push("");
  lines.push("## Historical Lessons");
  if (draft.historical_lessons.length === 0) {
    lines.push("- none");
  } else {
    for (const lesson of draft.historical_lessons) {
      lines.push(`- ${lesson.type}`);
      lines.push(`  - symptom: ${lesson.symptom}`);
      lines.push(`  - cause: ${lesson.cause}`);
      lines.push(`  - fix: ${lesson.fix}`);
    }
  }
  lines.push("");
  lines.push("## Suppressed Historical Findings");
  if (draft.suppressed_historical_findings.length === 0) {
    lines.push("- none");
  } else {
    for (const finding of draft.suppressed_historical_findings) lines.push(`- ${finding}`);
  }
  lines.push("");
  lines.push("## Tool Use Summary");
  if (draft.tool_use_summary.length === 0) {
    lines.push("- none");
  } else {
    for (const item of draft.tool_use_summary) lines.push(`- ${item.tool}: ${item.records} record(s), ${formatMs(item.duration_ms)}, failed=${item.failed}, waste/rework=${item.waste_or_rework}, commands=${item.commands}, context=${item.contexts}`);
  }
  lines.push("");
  lines.push("## Repeated Command Evidence");
  if (draft.repeated_commands.total_distinct === 0) {
    lines.push("- none");
  } else {
    lines.push(`- distinct repeated commands: ${draft.repeated_commands.total_distinct}`);
    if (draft.repeated_commands.by_scope.length > 0) {
      lines.push("- by scope:");
      for (const item of draft.repeated_commands.by_scope) lines.push(`  - ${item.scope || "unknown"}: ${item.count || 0}`);
    }
    if (draft.repeated_commands.classification.length > 0) {
      lines.push("- classification:");
      for (const item of draft.repeated_commands.classification) lines.push(`  - ${item.count}x ${item.classification} [${item.scope}]: ${item.command}`);
    }
    if (draft.repeated_commands.top_commands.length > 0) {
      lines.push("- top commands:");
      for (const item of draft.repeated_commands.top_commands) lines.push(`  - ${item.count}x [${item.scope}] ${item.command}`);
    }
    if (draft.repeated_commands.broad_final_by_work_item.length > 0) {
      lines.push("- broad/final by work item:");
      for (const item of draft.repeated_commands.broad_final_by_work_item) lines.push(`  - ${item.work_item || "unknown"}: ${item.count}x ${item.command}`);
    }
    if (draft.repeated_commands.unbatched_broad_final_commands.length > 0) {
      lines.push("- unbatched broad/final repeated:");
      for (const item of draft.repeated_commands.unbatched_broad_final_commands) lines.push(`  - ${item.count}x ${item.command}`);
    }
    if (draft.repeated_commands.batched_broad_final_commands.length > 0) {
      lines.push("- batched broad/final:");
      for (const item of draft.repeated_commands.batched_broad_final_commands) lines.push(`  - ${item.count}x ${item.command}`);
    }
    if (draft.repeated_commands.validation_batches.length > 0) {
      lines.push("- validation batches:");
      for (const item of draft.repeated_commands.validation_batches) {
        const result = item.failed > 0 ? `${item.failed} failed` : "pass";
        lines.push(`  - ${item.batch_id || "unknown"}: ${item.records} record(s), ${result}, broad/final=${item.broad_final_commands}`);
      }
    }
  }
  lines.push("");
  lines.push("## Next Cycle Actions");
  for (const action of draft.next_cycle_actions) lines.push(`- ${action}`);
  lines.push("");
  lines.push("## Caveats");
  for (const caveat of draft.caveats) lines.push(`- ${caveat}`);
  lines.push("");
  lines.push("## Evidence");
  for (const [name, path] of Object.entries(draft.evidence)) lines.push(`- ${name}: ${path || "(none)"}`);
  if (draft.review) lines.push(`- loaded_review_json: ${draft.review}`);
  return `${lines.join("\n")}\n`;
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help) usage();
const packetFile = positionals[0];
if (!packetFile) usage();
const outputFile = stringArg(values, "output", "");
const jsonOutputFile = stringArg(values, "json-output", "");

let packet;
try {
  packet = readJson(packetFile);
} catch (error) {
  console.error(`reflection draft failed for ${packetFile}: ${error.message}`);
  process.exit(1);
}
const review = loadOptionalJson(packet.artifacts?.review_json);
const draft = buildDraft(packet, review);
const rendered = renderMarkdown(draft, packetFile);

if (outputFile) {
  const target = resolve(outputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, rendered, "utf8");
}
if (jsonOutputFile) {
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(draft, null, 2)}\n`, "utf8");
}

process.stdout.write(rendered);
