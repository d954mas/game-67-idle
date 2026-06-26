// Orchestration policy over the task store.
//
// Taskboard owns markdown task state. This module owns the optional
// orchestration-specific interpretation of those tasks.

import { listTasks } from "../../taskboard/lib.mjs";
import {
  ORCHESTRATION_PACKET_TEMPLATE,
  ORCHESTRATION_REQUIRED_FIELDS,
  missingOrchestrationFields,
  orchestrationPreflightProblem,
} from "./lib.mjs";

const ORCHESTRATION_REVIEW_STATUSES = new Set(["review", "done"]);
const ARCHIVED_ORCHESTRATION_GUARD_MIN_TASK_ID = 28;
const ORCHESTRATION_ALLOWED_FILES_BOUNDS_MIN_TASK_ID = 76;
const ORCHESTRATION_START_PREFLIGHT_MIN_TASK_ID = 78;
const ORCHESTRATION_KEYWORDS = [
  "pipeline",
  "orchestration",
  "subagent",
  "subagents",
  "taskboard",
  "AI_PIPELINE",
  "docs/ai-pipeline",
  "ai_studio/core_harness/validation/pipeline_validate",
  "tools/skills_eval",
  "tools/skills_sync",
  ".codex/skills",
  "skill entrypoint",
];
const SMALL_SCOPE_REASON_PATTERNS = [
  /^one-file\b/i,
  /^docs-only\b/i,
  /^no code\b/i,
];

export function taskboardOrchestrationProblems(root) {
  const problems = [];
  const tasks = listTasks(root);
  const archivedTasks = listTasks(root, { includeArchive: true }).filter((task) => task.archived);
  for (const task of tasks) {
    if (task.fields.status === "doing") {
      const problem = orchestrationTaskStartPreflightProblem(task);
      if (problem) problems.push(problem);
    }
    if (ORCHESTRATION_REVIEW_STATUSES.has(task.fields.status)) {
      const problem = orchestrationTaskEvidenceProblem(task);
      if (problem) problems.push(problem);
    }
  }
  for (const task of archivedTasks) {
    if (!isArchivedOrchestrationGuardCandidate(task)) continue;
    if (ORCHESTRATION_REVIEW_STATUSES.has(task.fields.status)) {
      const problem = orchestrationTaskEvidenceProblem(task);
      if (problem) problems.push(problem);
    }
  }
  return problems;
}

export function currentDoingOrchestrationTaskIds(root) {
  return listTasks(root)
    .filter((task) => task.fields.status === "doing")
    .filter((task) => isSubstantialOrchestrationTask(task))
    .map((task) => task.fields.id)
    .filter(Boolean);
}

export function orchestrationTaskStartPreflightProblem(doc) {
  if (
    doc.kind !== "task"
    || doc.fields.status !== "doing"
    || !requiresOrchestrationStartPreflight(doc)
    || !isSubstantialOrchestrationTask(doc)
  ) {
    return null;
  }
  const log = sectionText(doc.body, "Log");
  if (hasSmallScopeOrchestrationException(log)) return null;
  const problem = orchestrationPreflightProblem(doc);
  if (!problem) return null;
  return {
    ...problem,
    code: "orchestration_start_preflight_missing",
    message: `${doc.fields.id}: substantial pipeline/orchestration task needs orchestration preflight before doing (missing/invalid: ${problem.missingFields.join(", ")})`,
  };
}

export function orchestrationTaskEvidenceProblem(doc) {
  if (doc.kind !== "task" || !isSubstantialOrchestrationTask(doc)) return null;
  const log = sectionText(doc.body, "Log");
  if (hasSmallScopeOrchestrationException(log)) return null;
  const missing = missingOrchestrationFields(log, {
    requiredFields: ORCHESTRATION_REQUIRED_FIELDS,
    requireBoundedAllowedFiles: requiresOrchestrationAllowedFilesBounds(doc),
  });
  if (!missing.length) return null;
  const detail = missing.length ? ` (missing/invalid: ${missing.join(", ")})` : "";
  return {
    code: "orchestration_evidence_missing",
    taskId: doc.fields.id,
    status: doc.fields.status,
    missingFields: missing,
    acceptedFields: ORCHESTRATION_REQUIRED_FIELDS.map(([name]) => name),
    template: ORCHESTRATION_PACKET_TEMPLATE,
    message: `${doc.fields.id}: substantial pipeline/orchestration task needs orchestration evidence before review/done${detail}`,
  };
}

function isSubstantialOrchestrationTask(doc) {
  const haystack = [
    doc.fields.title || "",
    Array.isArray(doc.fields.tags) ? doc.fields.tags.join(" ") : doc.fields.tags || "",
    doc.body || "",
  ].join("\n").toLowerCase();
  return ORCHESTRATION_KEYWORDS.some((keyword) => haystack.includes(keyword.toLowerCase()));
}

function isArchivedOrchestrationGuardCandidate(doc) {
  return taskIdAtLeast(doc, ARCHIVED_ORCHESTRATION_GUARD_MIN_TASK_ID);
}

function hasSmallScopeOrchestrationException(log) {
  const match = String(log || "").match(/orchestration:\s*not needed\s*-\s*small scope:\s*(.+)/i);
  if (!match || !match[1].trim()) return false;
  return SMALL_SCOPE_REASON_PATTERNS.some((pattern) => pattern.test(match[1]));
}

function sectionText(body, title) {
  const pattern = new RegExp(`(?:^|\\n)## ${escapeRegExp(title)}[ \\t]*\\n([\\s\\S]*?)(?=\\n##\\s+|$)`, "i");
  const match = String(body || "").match(pattern);
  return match ? match[1].replace(/- \[ \]\s*$/, "").trim() : "";
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function taskIdAtLeast(doc, minTaskId) {
  const match = String(doc.fields.id || "").match(/^T(\d+)$/);
  return match ? Number(match[1]) >= minTaskId : true;
}

function requiresOrchestrationAllowedFilesBounds(doc) {
  return taskIdAtLeast(doc, ORCHESTRATION_ALLOWED_FILES_BOUNDS_MIN_TASK_ID);
}

function requiresOrchestrationStartPreflight(doc) {
  return taskIdAtLeast(doc, ORCHESTRATION_START_PREFLIGHT_MIN_TASK_ID);
}
