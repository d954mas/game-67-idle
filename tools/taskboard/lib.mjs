// Taskboard core: markdown + frontmatter task store.
// One task = one task .md file in tasks/active/ or tasks/archive/.
// One epic = one .md file in tasks/epics/.
// tasks/README.md and tasks/STATUS.md are operational docs, not board items.
// No external dependencies; frontmatter is a strict YAML subset (key: value,
// arrays as [a, b]). Keep it strict so agents and humans stay compatible.

import { mkdirSync, readdirSync, readFileSync, writeFileSync, existsSync, statSync, renameSync } from "node:fs";
import { basename, join, resolve, dirname } from "node:path";
import { LIVE_STATUS_MAX_CHARS } from "../context_budget_config.mjs";

export { LIVE_STATUS_MAX_CHARS };

export const TASK_STATUSES = ["idea", "backlog", "todo", "doing", "review", "done", "dropped"];
export const EPIC_STATUSES = ["idea", "active", "done", "dropped"];
export const PRIORITIES = ["P0", "P1", "P2", "P3"];

const ORCHESTRATION_REVIEW_STATUSES = new Set(["review", "done"]);
// T0028 introduced the mechanical guard; older archives keep their legacy logs.
const ARCHIVED_ORCHESTRATION_GUARD_MIN_TASK_ID = 28;
// T0031 introduced machine-readable orchestration trace evidence. Older review
// tasks keep their label-only packet history; newer substantial orchestration
// tasks must include an executable trace/status evidence command and a recorded
// PASS result for one such command.
const ORCHESTRATION_MACHINE_EVIDENCE_MIN_TASK_ID = 31;
const ORCHESTRATION_KEYWORDS = [
  "pipeline",
  "orchestration",
  "subagent",
  "subagents",
  "taskboard",
  "AI_PIPELINE",
  "docs/ai-pipeline",
  "tools/pipeline_validate",
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
const ORCHESTRATION_REQUIRED_FIELDS = [
  ["objective", /\bobjective\b/i],
  ["allowed files", /\b(?:allowed files?|inputs?)\b/i],
  ["expected output", /\bexpected output\b/i],
  ["evidence command", /\b(?:evidence command|evidence artifact|artifact)\b/i],
  ["stop condition", /\bstop condition\b/i],
  ["independent reviewer", /\bindependent\s+(?:reviewer|verifier)\b/i],
];
const ORCHESTRATION_PREFLIGHT_FIELDS = [
  ...ORCHESTRATION_REQUIRED_FIELDS.slice(0, 2),
  ["tool-use guard", /\btool-use\s+guard\b/i],
  ...ORCHESTRATION_REQUIRED_FIELDS.slice(2),
];
const ORCHESTRATION_MACHINE_EVIDENCE_PATTERNS = [
  /\bnode\s+tools\/ai\.mjs\s+orchestration-trace\b/i,
];
const ORCHESTRATION_EVIDENCE_FIELD_PATTERN = /\b(?:evidence command|evidence artifact|artifact)\b/i;
const ORCHESTRATION_PACKET_TEMPLATE = `- orchestration: used
  objective: <non-empty>
  allowed files: <non-empty>
  tool-use guard: exact paths or discovery (rg --files/Test-Path) before reads; use Select-Object -Skip/-First for ranges; trace commands need an evidence source and --json-output
  expected output: <non-empty>
  evidence command: <non-empty>
  stop condition: <non-empty>
  independent reviewer: <non-empty>`;

function orchestrationPreflightNextAction(taskId) {
  const selector = taskId || "<task-id>";
  return `add a complete orchestration packet from \`node tools/ai.mjs orchestration-template\`, then rerun \`node tools/ai.mjs orchestration-check ${selector} --json\``;
}

export function orchestrationPacketTemplate() {
  return ORCHESTRATION_PACKET_TEMPLATE;
}

export function orchestrationPreflightProblem(doc) {
  const log = sectionText(doc.body || "", "Log");
  const missing = missingOrchestrationFields(log, {
    requireMachineEvidence: true,
    requireMachineEvidencePass: false,
    requiredFields: ORCHESTRATION_PREFLIGHT_FIELDS,
  });
  if (!missing.length) return null;
  const taskId = doc.fields?.id || "";
  return {
    code: "orchestration_preflight_missing",
    taskId,
    status: doc.fields?.status || "",
    missingFields: missing,
    acceptedFields: ORCHESTRATION_PREFLIGHT_FIELDS.map(([name]) => name),
    template: ORCHESTRATION_PACKET_TEMPLATE,
    message: `${taskId || "task"}: orchestration packet preflight failed (missing/invalid: ${missing.join(", ")})`,
    nextAction: orchestrationPreflightNextAction(taskId),
  };
}

export function findRoot(start = process.cwd()) {
  if (process.env.TASKBOARD_ROOT) {
    return resolve(process.env.TASKBOARD_ROOT);
  }
  let dir = resolve(start);
  for (;;) {
    if (existsSync(join(dir, "tasks"))) {
      return dir;
    }
    if (existsSync(join(dir, ".git"))) {
      return dir;
    }
    const parent = dirname(dir);
    if (parent === dir) {
      return resolve(start);
    }
    dir = parent;
  }
}

export function taskDir(root) {
  return join(root, "tasks");
}

export function activeTaskDir(root) {
  return join(taskDir(root), "active");
}

export function archiveTaskDir(root) {
  return join(taskDir(root), "archive");
}

export function epicDir(root) {
  return join(root, "tasks", "epics");
}

export function todayStamp() {
  return new Date().toISOString().slice(0, 10);
}

// --- frontmatter -----------------------------------------------------------

export function parseDoc(text) {
  const lines = text.split(/\r?\n/);
  if (lines[0] !== "---") {
    return { fields: {}, body: text };
  }
  const end = lines.indexOf("---", 1);
  if (end === -1) {
    return { fields: {}, body: text };
  }
  const fields = {};
  for (const line of lines.slice(1, end)) {
    const m = line.match(/^([A-Za-z_][\w-]*):\s*(.*)$/);
    if (!m) {
      continue;
    }
    fields[m[1]] = parseValue(m[2]);
  }
  return { fields, body: lines.slice(end + 1).join("\n").replace(/^\n/, "") };
}

function parseValue(raw) {
  const v = raw.trim();
  if (v.startsWith("[") && v.endsWith("]")) {
    const inner = v.slice(1, -1).trim();
    if (!inner) {
      return [];
    }
    return inner.split(",").map((s) => unquote(s.trim()));
  }
  return unquote(v);
}

function unquote(v) {
  if (v.startsWith('"') && v.endsWith('"')) {
    try {
      return JSON.parse(v); // serializer quotes via JSON.stringify, so unescape the same way
    } catch {
      return v.slice(1, -1);
    }
  }
  if (v.startsWith("'") && v.endsWith("'")) {
    return v.slice(1, -1);
  }
  return v;
}

function quoteIfNeeded(v) {
  const s = String(v);
  if (s === "" || /[:#\[\]{}"']|^\s|\s$/.test(s)) {
    return JSON.stringify(s);
  }
  return s;
}

export function serializeDoc(fields, body) {
  const out = ["---"];
  for (const [key, value] of Object.entries(fields)) {
    if (value === undefined || value === null) {
      continue;
    }
    if (Array.isArray(value)) {
      out.push(`${key}: [${value.map(quoteIfNeeded).join(", ")}]`);
    } else {
      out.push(`${key}: ${quoteIfNeeded(value)}`);
    }
  }
  out.push("---", "");
  out.push(body.replace(/\s+$/, ""), "");
  return out.join("\n");
}

// --- store -----------------------------------------------------------------

function listDocs(dir, kind, options = {}) {
  if (!existsSync(dir)) {
    return [];
  }
  const ignoredDocs = new Set(["readme.md", "status.md"]);
  const docs = [];
  for (const name of readdirSync(dir)) {
    if (!name.endsWith(".md") || ignoredDocs.has(name.toLowerCase())) {
      continue;
    }
    const file = join(dir, name);
    const { fields, body } = parseDoc(readFileSync(file, "utf8"));
    // rev = file mtime; used for optimistic-lock conflict detection on update.
    docs.push({
      kind,
      file,
      name,
      fields,
      body,
      archived: options.archived === true,
      rev: String(statSync(file).mtimeMs),
    });
  }
  docs.sort((a, b) => String(a.fields.id || a.name).localeCompare(String(b.fields.id || b.name)));
  return docs;
}

function listArchiveTasks(root) {
  const archive = archiveTaskDir(root);
  if (!existsSync(archive)) {
    return [];
  }
  const docs = [];
  for (const group of readdirSync(archive)) {
    const dir = join(archive, group);
    if (!statSync(dir).isDirectory()) {
      continue;
    }
    docs.push(...listDocs(dir, "task", { archived: true }));
  }
  docs.sort((a, b) => String(a.fields.id || a.name).localeCompare(String(b.fields.id || b.name)));
  return docs;
}

export function listTasks(root, options = {}) {
  const active = [
    ...listDocs(activeTaskDir(root), "task"),
    // Legacy root-level task files are read until migrated.
    ...listDocs(taskDir(root), "task"),
  ];
  if (options.includeArchive) {
    return [...active, ...listArchiveTasks(root)];
  }
  return active;
}

export function listEpics(root) {
  return listDocs(epicDir(root), "epic");
}

export function findDoc(root, id) {
  const all = [...listTasks(root, { includeArchive: true }), ...listEpics(root)];
  return all.find((d) => d.fields.id === id) || null;
}

function nextId(docs, prefix, pad) {
  let max = 0;
  for (const d of docs) {
    const m = String(d.fields.id || d.name).match(new RegExp(`^${prefix}(\\d+)`));
    if (m) {
      max = Math.max(max, Number(m[1]));
    }
  }
  return prefix + String(max + 1).padStart(pad, "0");
}

export function slugify(title) {
  const slug = String(title)
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 48);
  return slug || "item";
}

const TASK_BODY_TEMPLATE = `## What

## Done when

- [ ]

## Open questions

## Log
`;

const EPIC_BODY_TEMPLATE = `## Goal

## In scope

## Out of scope

## Log
`;

export function createTask(root, input = {}) {
  const dir = activeTaskDir(root);
  mkdirSync(dir, { recursive: true });
  const tasks = listTasks(root, { includeArchive: true });
  const id = nextId(tasks, "T", 4);
  const fields = {
    id,
    title: input.title || "Untitled task",
    status: TASK_STATUSES.includes(input.status) ? input.status : "idea",
    epic: input.epic || "",
    priority: PRIORITIES.includes(input.priority) ? input.priority : "P2",
    tags: Array.isArray(input.tags) ? input.tags : [],
    created: todayStamp(),
    updated: todayStamp(),
  };
  const file = join(dir, `${id}-${slugify(fields.title)}.md`);
  writeFileSync(file, serializeDoc(fields, input.body || TASK_BODY_TEMPLATE));
  return { kind: "task", file, fields, body: input.body || TASK_BODY_TEMPLATE };
}

export function createEpic(root, input = {}) {
  const dir = epicDir(root);
  mkdirSync(dir, { recursive: true });
  const epics = listEpics(root);
  const id = nextId(epics, "E", 3);
  const fields = {
    id,
    title: input.title || "Untitled epic",
    status: EPIC_STATUSES.includes(input.status) ? input.status : "idea",
    priority: PRIORITIES.includes(input.priority) ? input.priority : "P2",
    tags: Array.isArray(input.tags) ? input.tags : [],
    created: todayStamp(),
    updated: todayStamp(),
  };
  const file = join(dir, `${id}-${slugify(fields.title)}.md`);
  writeFileSync(file, serializeDoc(fields, input.body || EPIC_BODY_TEMPLATE));
  return { kind: "epic", file, fields, body: input.body || EPIC_BODY_TEMPLATE };
}

export function updateDoc(root, id, patch = {}) {
  const doc = findDoc(root, id);
  if (!doc) {
    throw new Error(`No task or epic with id ${id}`);
  }
  if (patch.rev !== undefined && patch.rev !== doc.rev) {
    const err = new Error(`${id} changed on disk since it was loaded; reload and retry`);
    err.conflict = true;
    throw err;
  }
  const statuses = doc.kind === "task" ? TASK_STATUSES : EPIC_STATUSES;
  const fields = { ...doc.fields };
  for (const [key, value] of Object.entries(patch.fields || {})) {
    if (key === "id" || key === "created") {
      continue;
    }
    if (key === "status" && !statuses.includes(value)) {
      throw new Error(`Invalid status "${value}" for ${doc.kind} (allowed: ${statuses.join(", ")})`);
    }
    if (key === "priority" && value && !PRIORITIES.includes(value)) {
      throw new Error(`Invalid priority "${value}" (allowed: ${PRIORITIES.join(", ")})`);
    }
    fields[key] = value;
  }
  fields.updated = todayStamp();
  const body = patch.body !== undefined ? patch.body : doc.body;
  if (requiresOrchestrationTransitionGuard(doc, fields)) {
    const problem = orchestrationEvidenceProblem({ ...doc, fields, body });
    if (problem) {
      throw validationError(problem);
    }
  }
  writeFileSync(doc.file, serializeDoc(fields, body));
  let file = doc.file;
  if (doc.kind === "task") {
    const targetDir = taskStorageDir(root, fields);
    mkdirSync(targetDir, { recursive: true });
    const targetFile = join(targetDir, basename(doc.file));
    if (resolve(targetFile) !== resolve(doc.file)) {
      if (existsSync(targetFile)) {
        throw new Error(`Cannot move ${id}; target already exists: ${targetFile}`);
      }
      renameSync(doc.file, targetFile);
      file = targetFile;
    }
  }
  return { ...doc, file, fields, body };
}

function taskStorageDir(root, fields) {
  if (["done", "dropped"].includes(fields.status)) {
    return join(archiveTaskDir(root), fields.epic || "unassigned");
  }
  return activeTaskDir(root);
}

export function validateStore(root) {
  return validateStoreDetailed(root).map(problemMessage);
}

export function validateStoreDetailed(root) {
  const problems = [];
  const tasks = listTasks(root);
  const archivedTasks = listTasks(root, { includeArchive: true }).filter((t) => t.archived);
  const epics = listEpics(root);
  const statusFile = join(taskDir(root), "STATUS.md");
  if (existsSync(statusFile)) {
    const statusChars = readFileSync(statusFile, "utf8").length;
    if (statusChars > LIVE_STATUS_MAX_CHARS) {
      problems.push(genericProblem(`tasks/STATUS.md exceeds live status budget (${statusChars}/${LIVE_STATUS_MAX_CHARS} chars); move historical evidence to tasks/archive/ or gamedesign/projects/`));
    }
  }
  const seen = new Map();
  for (const d of [...tasks, ...epics]) {
    const id = d.fields.id;
    if (!id) {
      problems.push(genericProblem(`${d.name}: missing id`));
      continue;
    }
    if (seen.has(id)) {
      problems.push(genericProblem(`${d.name}: duplicate id ${id} (also in ${seen.get(id)})`));
    }
    seen.set(id, d.name);
    if (!d.fields.title) {
      problems.push(genericProblem(`${id}: missing title`, { taskId: id }));
    }
    const statuses = d.kind === "task" ? TASK_STATUSES : EPIC_STATUSES;
    if (!statuses.includes(d.fields.status)) {
      problems.push(genericProblem(`${id}: invalid status "${d.fields.status}"`, { taskId: id }));
    }
    if (d.kind === "epic" && d.fields.status === "active" && !hasActiveEpicBody(d.body)) {
      problems.push(genericProblem(`${id}: active epic needs non-empty Goal, In scope, and Out of scope sections`, { taskId: id }));
    }
  }
  const epicIds = new Set(epics.map((e) => e.fields.id));
  for (const t of tasks) {
    if (t.fields.epic && !epicIds.has(t.fields.epic)) {
      problems.push(genericProblem(`${t.fields.id}: references missing epic "${t.fields.epic}"`, { taskId: t.fields.id }));
    }
    if (isActionableTask(t) && !hasActionableTaskBody(t.body)) {
      problems.push(genericProblem(`${t.fields.id}: actionable task needs non-empty What and Done when sections`, { taskId: t.fields.id }));
    }
    if (ORCHESTRATION_REVIEW_STATUSES.has(t.fields.status)) {
      const problem = orchestrationEvidenceProblem(t);
      if (problem) {
        problems.push(problem);
      }
    }
  }
  for (const t of archivedTasks) {
    if (!isArchivedOrchestrationGuardCandidate(t)) {
      continue;
    }
    if (ORCHESTRATION_REVIEW_STATUSES.has(t.fields.status)) {
      const problem = orchestrationEvidenceProblem(t);
      if (problem) {
        problems.push(problem);
      }
    }
  }
  return problems;
}

function requiresOrchestrationTransitionGuard(doc, fields) {
  return (
    doc.kind === "task" &&
    doc.fields.status !== fields.status &&
    ORCHESTRATION_REVIEW_STATUSES.has(fields.status)
  );
}

function orchestrationEvidenceProblem(doc) {
  if (doc.kind !== "task" || !isSubstantialOrchestrationTask(doc)) {
    return null;
  }
  const log = sectionText(doc.body, "Log");
  const requireMachineEvidence = requiresOrchestrationMachineEvidence(doc);
  if (
    hasOrchestrationUsedEvidence(log, requireMachineEvidence)
    && (!requireMachineEvidence || hasMatchingMachineEvidencePassAfterOrchestration(log))
  ) {
    return null;
  }
  if (hasSmallScopeOrchestrationException(log)) {
    return null;
  }
  const missing = missingOrchestrationFields(log, {
    requireMachineEvidence,
    requireMachineEvidencePass: requireMachineEvidence,
  });
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

function genericProblem(message, extras = {}) {
  return { code: "taskboard_problem", message, ...extras };
}

function problemMessage(problem) {
  return typeof problem === "string" ? problem : problem.message;
}

function validationError(problem) {
  const err = new Error(problemMessage(problem));
  err.problem = problem;
  return err;
}

function isSubstantialOrchestrationTask(doc) {
  const haystack = [
    doc.fields.title || "",
    Array.isArray(doc.fields.tags) ? doc.fields.tags.join(" ") : doc.fields.tags || "",
    doc.body || "",
  ].join("\n");
  return ORCHESTRATION_KEYWORDS.some((keyword) => haystack.toLowerCase().includes(keyword.toLowerCase()));
}

export function currentDoingOrchestrationTaskIds(root) {
  return listTasks(root)
    .filter((task) => task.fields.status === "doing")
    .filter((task) => isSubstantialOrchestrationTask(task))
    .map((task) => task.fields.id)
    .filter(Boolean);
}

export function inferCurrentDoingOrchestrationTaskId(root) {
  const candidates = currentDoingOrchestrationTaskIds(root);
  return candidates.length === 1 ? candidates[0] : "";
}

function isArchivedOrchestrationGuardCandidate(doc) {
  const match = String(doc.fields.id || "").match(/^T(\d+)$/);
  return match ? Number(match[1]) >= ARCHIVED_ORCHESTRATION_GUARD_MIN_TASK_ID : true;
}

function hasOrchestrationUsedEvidence(log, requireMachineEvidence = false) {
  return orchestrationUsedBlocks(log).some((block) =>
    ORCHESTRATION_REQUIRED_FIELDS.every(([, pattern]) => hasMeaningfulFieldValue(block, pattern))
      && (!requireMachineEvidence || hasMachineEvidenceCommand(block)),
  );
}

function missingOrchestrationFields(log, options = {}) {
  const {
    requireMachineEvidence = false,
    requireMachineEvidencePass = requireMachineEvidence,
    requiredFields = ORCHESTRATION_REQUIRED_FIELDS,
  } = typeof options === "boolean" ? { requireMachineEvidence: options } : options;
  const blocks = orchestrationUsedBlocks(log);
  if (!blocks.length) {
    return ["orchestration: used packet"];
  }
  const baseline = requiredFields.map(([name]) => name);
  if (requireMachineEvidence) baseline.push("machine evidence command");
  if (requireMachineEvidencePass) baseline.push("machine evidence pass");
  let bestMissing = baseline;
  for (const block of blocks) {
    const missing = requiredFields
      .filter(([, pattern]) => !hasMeaningfulFieldValue(block, pattern))
      .map(([name]) => name);
    if (requireMachineEvidence && !hasMachineEvidenceCommand(block)) {
      missing.push("machine evidence command");
    }
    if (requireMachineEvidencePass && !hasMatchingMachineEvidencePassAfterOrchestration(log, block)) {
      missing.push("machine evidence pass");
    }
    if (missing.length < bestMissing.length) {
      bestMissing = missing;
    }
  }
  return bestMissing;
}

function orchestrationUsedBlocks(log) {
  const lines = String(log || "").split(/\r?\n/);
  const blocks = [];
  for (let i = 0; i < lines.length; i += 1) {
    if (!/orchestration:\s*used\b/i.test(lines[i])) continue;
    const block = [lines[i]];
    for (let j = i + 1; j < lines.length; j += 1) {
      if (/^\s*[-*]\s+\S/.test(lines[j])) break;
      block.push(lines[j]);
    }
    blocks.push(block.join("\n"));
  }
  return blocks;
}

function hasMeaningfulFieldValue(text, fieldPattern) {
  for (const line of String(text || "").split(/\r?\n/)) {
    const match = line.match(/^\s*(?:[-*]\s*)?(.+?)\s*:\s*(.*)$/);
    if (!match || !fieldPattern.test(match[1])) continue;
    const value = match[2].trim();
    if (!value || /^(tbd|todo|none|n\/a|na|unknown|\.\.\.)$/i.test(value)) {
      return false;
    }
    return true;
  }
  return false;
}

function hasMachineEvidenceCommand(block) {
  return machineEvidenceSignatures(fieldValue(block, ORCHESTRATION_EVIDENCE_FIELD_PATTERN)).length > 0;
}

function hasMatchingMachineEvidencePassAfterOrchestration(log, declaredBlock = "") {
  const declared = declaredBlock
    ? machineEvidenceSignatures(fieldValue(declaredBlock, ORCHESTRATION_EVIDENCE_FIELD_PATTERN))
    : orchestrationUsedBlocks(log)
      .flatMap((block) => machineEvidenceSignatures(fieldValue(block, ORCHESTRATION_EVIDENCE_FIELD_PATTERN)));
  if (declared.length === 0) return false;
  const passSignatures = evidencePassBlocksAfterOrchestration(log).flatMap(machineEvidenceSignatures);
  return declared.some((expected) => passSignatures.some((actual) => machineEvidenceSignaturesMatch(expected, actual)));
}

function machineEvidenceSignatures(text) {
  const evidence = String(text || "").replaceAll("\\", "/");
  const signatures = [];
  const chunks = evidence.split(/(?=\bnode\s+tools\/ai\.mjs\s+(?:orchestration-trace|status)\b)/i);
  for (const chunk of chunks) {
    const command = chunk.split(/(?:\s*;\s*|\r?\n\s*[-*]\s+)/, 1)[0] || "";
    if (ORCHESTRATION_MACHINE_EVIDENCE_PATTERNS.some((pattern) => pattern.test(command))) {
      const source = commandSourceSignature(command, ["--parent-thread-id", "--session"]);
      const artifact = commandSourceSignature(command, ["--json-output"]);
      if (!source || !artifact) continue;
      signatures.push({
        kind: "orchestration-trace",
        source,
        artifact,
      });
      continue;
    }
    if (
      /\bnode\s+tools\/ai\.mjs\s+status\b/i.test(command)
      && /\s--agent-rollup\b/i.test(command)
      && /\s--require-agent-rollup-ok\b/i.test(command)
      && (/\s--parent-thread-id\b/i.test(command) || /\s--trace-session\b/i.test(command))
    ) {
      signatures.push({
        kind: "status-agent-rollup",
        source: commandSourceSignature(command, ["--parent-thread-id", "--trace-session"]),
      });
    }
  }
  return signatures;
}

function commandSourceSignature(command, flags) {
  for (const flag of flags) {
    const escaped = flag.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    const match = command.match(new RegExp(`(?:^|\\s)${escaped}(?:=|\\s+)(?:"([^"]+)"|'([^']+)'|(\\S+))`, "i"));
    if (match) return `${flag}=${normalizeMachineEvidenceValue(match[1] || match[2] || match[3] || "")}`;
  }
  return "";
}

function normalizeMachineEvidenceValue(value) {
  return String(value || "")
    .replaceAll("\\", "/")
    .trim()
    .replace(/^[`'"]+|[`'",.;)]+$/g, "")
    .toLowerCase();
}

function machineEvidenceSignaturesMatch(expected, actual) {
  if (!expected || !actual || expected.kind !== actual.kind) return false;
  if (expected.artifact || actual.artifact) return expected.artifact === actual.artifact && expected.source === actual.source;
  if (expected.source || actual.source) return expected.source === actual.source;
  return (
    expected.kind === actual.kind
  );
}

function evidencePassBlocksAfterOrchestration(log) {
  const lines = String(log || "").split(/\r?\n/);
  const blocks = [];
  let sawOrchestrationPacket = false;
  for (let i = 0; i < lines.length; i += 1) {
    if (/orchestration:\s*used\b/i.test(lines[i])) {
      sawOrchestrationPacket = true;
    }
    if (!sawOrchestrationPacket) continue;
    if (!/^\s*[-*]\s+evidence\s*:\s*PASS\b/i.test(lines[i])) continue;
    const block = [lines[i]];
    for (let j = i + 1; j < lines.length; j += 1) {
      if (/^\s*[-*]\s+\S/.test(lines[j])) break;
      block.push(lines[j]);
    }
    blocks.push(block.join("\n"));
  }
  return blocks;
}

function requiresOrchestrationMachineEvidence(doc) {
  const match = String(doc.fields.id || "").match(/^T(\d+)$/);
  return match ? Number(match[1]) >= ORCHESTRATION_MACHINE_EVIDENCE_MIN_TASK_ID : true;
}

function fieldValue(text, fieldPattern) {
  const lines = String(text || "").split(/\r?\n/);
  const out = [];
  let collecting = false;
  for (const line of lines) {
    const match = line.match(/^\s*(?:[-*]\s*)?(.+?)\s*:\s*(.*)$/);
    if (match) {
      if (collecting && isOrchestrationFieldLabel(match[1])) break;
      if (fieldPattern.test(match[1])) {
        collecting = true;
        out.push(match[2].trim());
        continue;
      }
    }
    if (collecting) out.push(line.trim());
  }
  return out.join(" ").trim();
}

function isOrchestrationFieldLabel(label) {
  return ORCHESTRATION_REQUIRED_FIELDS.some(([, pattern]) => pattern.test(label));
}

function hasSmallScopeOrchestrationException(log) {
  const match = log.match(/orchestration:\s*not needed\s*-\s*small scope:\s*(.+)/i);
  if (!match || !match[1].trim()) {
    return false;
  }
  return SMALL_SCOPE_REASON_PATTERNS.some((pattern) => pattern.test(match[1]));
}

function isActionableTask(doc) {
  return ["backlog", "todo", "doing", "review"].includes(doc.fields.status);
}

function hasActionableTaskBody(body) {
  const what = sectionText(body, "What");
  const doneWhen = sectionText(body, "Done when");
  return what.length > 0 && /- \[[ xX]\]\s+\S/.test(doneWhen);
}

function hasActiveEpicBody(body) {
  return ["Goal", "In scope", "Out of scope"].every((section) => sectionText(body, section).length > 0);
}

function sectionText(body, title) {
  const pattern = new RegExp(`(?:^|\\n)## ${escapeRegExp(title)}[ \\t]*\\n([\\s\\S]*?)(?=\\n##\\s+|$)`, "i");
  const match = body.match(pattern);
  return match ? match[1].replace(/- \[ \]\s*$/, "").trim() : "";
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
