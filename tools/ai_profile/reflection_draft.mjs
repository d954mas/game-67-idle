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

function compactFinding(finding) {
  if (!finding || typeof finding !== "object") return { type: "finding", message: String(finding || "") };
  return {
    type: finding.type || "finding",
    message: finding.message || finding.label || "",
    ...finding,
  };
}

function lessonForFinding(finding) {
  const type = finding.type || "finding";
  const message = finding.message || "";
  const templates = {
    repeated_broad_final: {
      symptom: message,
      cause: "Broad/final validation was repeated in the historical profile instead of being batched or guarded by a validation plan.",
      fix: "Use validation planning and current-scope status before rerunning broad/final gates.",
    },
    missing_context_inputs: {
      symptom: message,
      cause: "Medium/high context reads were recorded without measured context_inputs.",
      fix: "Use context.mjs or context_command.mjs for medium/high context reads.",
    },
    missing_work_item_metadata: {
      symptom: message,
      cause: "Older profile records were created before persistent work-item scope was reliable.",
      fix: "Start each focused iteration with start.mjs or scope.mjs so future records inherit work-item metadata.",
    },
    low_profile_coverage: {
      symptom: message,
      cause: "Long manual/research/review stretches were not checkpointed in the historical profile.",
      fix: "Use checkpoint.mjs during long non-command stretches.",
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
  const currentFindings = asArray(packet.current_scope?.findings).map(compactFinding);
  const currentActions = asArray(packet.current_scope?.suggested_actions);
  const historicalLessons = findings.map(lessonForFinding);
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
