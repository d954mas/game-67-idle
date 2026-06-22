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
export const DEFAULT_ORCHESTRATION_TOOL_USE_GUARD = "verify exact repo paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First for line windows; keep evidence commands read-only";

const ORCHESTRATION_REVIEW_STATUSES = new Set(["review", "done"]);
// Legacy compatibility thresholds: older tasks predate the lightweight guard and
// keep their original label history. T0028 introduced the guard; allowed-files
// bounds and broad-domain classification arrived later, so pre-threshold tasks
// stay exempt from those checks.
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
const ORCHESTRATION_ALLOWED_FILES_FIELD_PATTERN = /\b(?:allowed files?|inputs?)\b/i;
const ORCHESTRATION_PACKET_TEMPLATE = `- orchestration: used
  objective: <non-empty>
  allowed files: <non-empty>
  tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}
  expected output: <non-empty>
  evidence command: <non-empty>
  stop condition: <non-empty>
  independent reviewer: <non-empty>`;
const SUBAGENT_PACKET_TEMPLATE = `objective: <bounded subagent objective>
allowed files: <repo-local files or bounded patterns>
forbidden files: <files or areas the subagent must not touch>
tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}
expected output: <concise final report or changed files>
evidence command or artifact: <read-only command, focused test, or artifact path>
stop condition: <when the subagent must stop>
handoff:
  findings: <facts or verdict>
  files: <files inspected or changed>
  commands/evidence: <commands run and results>
  risks: <remaining risk>
  owner action: <what the lead must do next>
  not-done: <explicit gaps>`;

function orchestrationPreflightNextAction(taskId) {
  const selector = taskId || "<task-id>";
  return `add a complete orchestration packet from \`node tools/ai.mjs orchestration-template\`, then rerun \`node tools/ai.mjs orchestration-check ${selector} --json\``;
}

export function orchestrationPacketTemplate() {
  return ORCHESTRATION_PACKET_TEMPLATE;
}

export function subagentPacketTemplate() {
  return SUBAGENT_PACKET_TEMPLATE;
}

export function subagentPacketProblem(text) {
  const packet = String(text || "");
  const missing = [];
  const required = [
    ["objective", /\bobjective\b/i],
    ["allowed files", /\b(?:allowed files?|inputs?)\b/i],
    ["forbidden files", /\bforbidden files?\b/i],
    ["tool-use guard", /\btool-use\s+guard\b/i],
    ["expected output", /\bexpected output\b/i],
    ["evidence command or artifact", /\b(?:evidence command or artifact|evidence command|evidence artifact|artifact)\b/i],
    ["stop condition", /\bstop condition\b/i],
    ["handoff", /\bhandoff\b/i],
  ];
  for (const [name, pattern] of required) {
    if (!hasText(packetFieldValue(packet, pattern))) missing.push(name);
  }
  const allowed = packetFieldValue(packet, ORCHESTRATION_ALLOWED_FILES_FIELD_PATTERN);
  if (allowed && boundedAllowedFilesProblem(allowed)) missing.push("bounded allowed files");
  const handoff = packetFieldValue(packet, /\bhandoff\b/i);
  for (const label of ["findings", "files", "commands/evidence", "risks", "owner action", "not-done"]) {
    const pattern = new RegExp(`\\b${escapeRegExp(label)}\\b`, "i");
    if (!pattern.test(handoff)) missing.push(`handoff ${label}`);
  }
  if (!missing.length) return null;
  return {
    code: "subagent_packet_invalid",
    missingFields: [...new Set(missing)],
    template: SUBAGENT_PACKET_TEMPLATE,
    message: `subagent packet failed (missing/invalid: ${[...new Set(missing)].join(", ")})`,
  };
}

// Harness-neutral packet presets. They emit ready-to-spawn bounded packets the
// lead pastes into ANY harness (Claude Agent/Workflow tool, Codex spawn_agent).
// Parallel presets fan out one worker per target; the packet is the portable
// unit, the spawn mechanism is the thin per-harness wrapper. Advisory only.
const SUBAGENT_PACKET_LEAN_HANDOFF = `handoff:
  findings: <facts or verdict>
  files: <files inspected or changed>
  commands/evidence: <commands run and results, or artifact pointer>
  risks: <remaining risk>
  owner action: <what the lead decides/integrates>
  not-done: <explicit gaps>`;

function renderSubagentPacket(f) {
  return [
    `objective: ${f.objective}`,
    `allowed files: ${f.allowedFiles}`,
    `forbidden files: ${f.forbiddenFiles}`,
    `tool-use guard: ${f.toolGuard || DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}`,
    `expected output: ${f.expectedOutput}`,
    `evidence command or artifact: ${f.evidence}`,
    `stop condition: ${f.stop}`,
    SUBAGENT_PACKET_LEAN_HANDOFF,
  ].join("\n");
}

const PARALLEL_INTRO =
  "PARALLEL FAN-OUT: spawn one worker per packet below at the same time (Claude: multiple Agent-tool calls in one turn, or the Workflow tool; Codex: parallel spawn_agent). Each runs read-only in its own context and returns only its handoff; the lead concatenates and integrates. Disjoint scope, so workers cannot conflict. Lint one packet at a time via `node tools/ai.mjs subagent-packet-check --stdin`.";
const SINGLE_INTRO =
  "SINGLE WORKER: spawn one subagent (Claude Agent tool / Codex spawn_agent). It returns its handoff; the lead integrates.";
const SEQUENTIAL_INTRO =
  "SEQUENTIAL STAGES: run these packets in order; each stage's output feeds the next. The lead integrates between stages.";

const SUBAGENT_PACKET_PRESETS = {
  "codebase-map": {
    mode: "parallel",
    intro: PARALLEL_INTRO,
    defaultTargets: ["src/<area>/**", "tools/<area>/**"],
    build: (t) => ({
      label: `map ${t}`,
      text: renderSubagentPacket({
        objective: `Map how the ${t} area works and its public entry points; produce a <=200-word brief. Read only; make no edits.`,
        allowedFiles: t,
        forbiddenFiles: "everything outside the allowed files; make no edits anywhere",
        expectedOutput: "brief: entry points (file:line) + data flow + 3 risks or unknowns",
        evidence: "inline handoff (small)",
        stop: "entry points and data flow identified, or 10 files read",
      }),
    }),
  },
  review: {
    mode: "parallel",
    intro: PARALLEL_INTRO,
    defaultTargets: ["correctness", "readability", "scope"],
    build: (axis) => ({
      label: `review:${axis}`,
      text: renderSubagentPacket({
        objective: `Review <artifact-or-feature> on ONE axis: ${axis}. You are a fresh reviewer, not the builder. Give a verdict and concrete issues only.`,
        allowedFiles: "<dir>/<artifact-file.ext>",
        forbiddenFiles: "no edits anywhere",
        expectedOutput: "verdict (pass/concerns/fail) + issues (file:line + one-line fix), <=10 bullets",
        evidence: "inline handoff",
        stop: `the artifact is fully reviewed on the ${axis} axis`,
      }),
    }),
  },
  "asset-research": {
    mode: "single",
    intro: SINGLE_INTRO,
    build: () => ({
      label: "asset / source / license research",
      text: renderSubagentPacket({
        objective: "Find <N> CC0/CC-BY <asset-type> candidates for <game-id>; report license, provenance, and integrity for each. Do not import anything.",
        allowedFiles: "gamedesign/sources/**;gamedesign/knowledge/**;tmp/<game-id>-asset-candidates.md",
        forbiddenFiles: "src; state; any runtime pack; hot docs (AGENTS.md, AI_PIPELINE.md, tasks/STATUS.md)",
        toolGuard: `${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}; prefer authoritative sources over SEO content farms; verify each license URL`,
        expectedOutput: "<=5 candidates, each with URL + license + provenance + integrity check",
        evidence: "tmp/<game-id>-asset-candidates.md",
        stop: "5 viable candidates found OR 8 sources checked with none viable",
      }),
    }),
  },
  "texture-gen": {
    mode: "parallel",
    intro: `${PARALLEL_INTRO} Image gen uses the delegated-image-generation skill: Codex imagegen (Path A) first, Antigravity agy (Path B) as fallback; verify the PNG by size and eyeball.`,
    defaultTargets: ["<asset-a>", "<asset-b>"],
    build: (asset) => ({
      label: `gen ${asset}`,
      text: renderSubagentPacket({
        objective: `Generate ${asset} via the delegated-image-generation skill (Codex imagegen Path A; Antigravity agy Path B fallback). Propose a project-local path; do NOT wire it into runtime.`,
        allowedFiles: `tmp/gen/${asset}.png`,
        forbiddenFiles: "src; state; any runtime pack or manifest; hot docs",
        toolGuard: `${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}; verify the output PNG exists and its dimensions match before returning; never trust the CLI transcript`,
        expectedOutput: "handoff with artifact path + dimensions + a one-line fake-shot self-judgment",
        evidence: `tmp/gen/${asset}.png`,
        stop: "one verified asset produced OR 3 generation attempts fail",
      }),
    }),
  },
  "asset-intake": {
    mode: "sequential",
    intro: SEQUENTIAL_INTRO,
    stages: () => [
      {
        label: "stage 1: source research",
        text: renderSubagentPacket({
          objective: "Decide a source for <asset> in <game-id>: find a CC0/CC-BY asset (URL+license+provenance) OR write a concrete generation prompt. Do not import or generate yet.",
          allowedFiles: "gamedesign/sources/**;gamedesign/knowledge/**;tmp/<game-id>-<asset>-intake.md",
          forbiddenFiles: "src; state; runtime packs; hot docs",
          toolGuard: `${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}; prefer authoritative sources; verify each license URL`,
          expectedOutput: "a licensed source OR a ready generation prompt, written to the intake note",
          evidence: "tmp/<game-id>-<asset>-intake.md",
          stop: "a source or a prompt is decided",
        }),
      },
      {
        label: "stage 2: generate",
        text: renderSubagentPacket({
          objective: "Generate <asset> from stage 1's source/prompt via the delegated-image-generation skill (Codex imagegen Path A; Antigravity agy Path B fallback).",
          allowedFiles: "tmp/gen/<asset>.png",
          forbiddenFiles: "src; state; runtime packs or manifests; hot docs",
          toolGuard: `${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}; verify the output PNG exists and dimensions match before returning; never trust the CLI transcript`,
          expectedOutput: "the generated PNG + path + dimensions",
          evidence: "tmp/gen/<asset>.png",
          stop: "one verified PNG produced OR 3 attempts fail",
        }),
      },
      {
        label: "stage 3: verify + propose",
        text: renderSubagentPacket({
          objective: "Verify the generated <asset> (dimensions, transparency, look vs the art reference) and propose a project-local destination + provenance note. Do NOT wire it into runtime.",
          allowedFiles: "tmp/gen/<asset>.png;tmp/<game-id>-<asset>-intake.md",
          forbiddenFiles: "src; state; runtime packs or manifests; hot docs",
          expectedOutput: "verdict + proposed destination path + provenance line",
          evidence: "tmp/<game-id>-<asset>-intake.md",
          stop: "verdict and proposed destination recorded",
        }),
      },
    ],
  },
};

export function subagentPacketPresetNames() {
  return Object.keys(SUBAGENT_PACKET_PRESETS);
}

export function subagentPacketPreset(name, targets = []) {
  const def = SUBAGENT_PACKET_PRESETS[name];
  if (!def) {
    const error = new Error(`unknown subagent packet preset: ${name}`);
    error.code = "unknown_preset";
    error.presets = subagentPacketPresetNames();
    throw error;
  }
  let packets;
  if (def.mode === "sequential") {
    packets = def.stages();
  } else if (def.mode === "single") {
    packets = [def.build()];
  } else {
    const list = Array.isArray(targets) && targets.length ? targets : def.defaultTargets;
    packets = list.map((t) => def.build(t));
  }
  return { name, mode: def.mode, intro: def.intro, packets };
}

export function renderSubagentPacketPreset(name, targets = []) {
  const { intro, packets } = subagentPacketPreset(name, targets);
  const total = packets.length;
  const blocks = packets.map((p, i) => `# packet ${i + 1}/${total} - ${p.label}\n${p.text}`);
  return `${intro}\n\n${blocks.join("\n\n")}`;
}

function packetFieldValue(text, fieldPattern) {
  const lines = String(text || "").split(/\r?\n/);
  const out = [];
  let collecting = false;
  for (const line of lines) {
    const match = line.match(/^\s*(?:[-*]\s*)?(.+?)\s*:\s*(.*)$/);
    if (match) {
      if (collecting && !/^\s/.test(line)) break;
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

export function orchestrationPreflightProblem(doc) {
  const log = sectionText(doc.body || "", "Log");
  const missing = missingOrchestrationFields(log, {
    requiredFields: ORCHESTRATION_PREFLIGHT_FIELDS,
    requireBoundedAllowedFiles: true,
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
  const body = input.body || TASK_BODY_TEMPLATE;
  const problem = orchestrationStartPreflightProblem({ kind: "task", file: "", fields, body });
  if (problem) {
    throw validationError(problem);
  }
  const file = join(dir, `${id}-${slugify(fields.title)}.md`);
  writeFileSync(file, serializeDoc(fields, body));
  return { kind: "task", file, fields, body };
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
  if (requiresOrchestrationStartPreflightGuard(doc, fields)) {
    const problem = orchestrationStartPreflightProblem({ ...doc, fields, body });
    if (problem) {
      throw validationError(problem);
    }
  }
  if (requiresOrchestrationTransitionGuard(doc, fields)) {
    const problem = orchestrationEvidenceProblem({ ...doc, fields, body }, root);
    if (problem) {
      throw validationError(problem);
    }
  }
  if (requiresOrchestrationCurrentCloseoutGuard(doc, fields, body)) {
    const problem = orchestrationEvidenceProblem({ ...doc, fields, body }, root);
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
    if (t.fields.status === "doing") {
      const problem = orchestrationStartPreflightProblem(t);
      if (problem) {
        problems.push(problem);
      }
    }
    if (ORCHESTRATION_REVIEW_STATUSES.has(t.fields.status)) {
      const problem = orchestrationEvidenceProblem(t, root);
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
      const problem = orchestrationEvidenceProblem(t, root);
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

function requiresOrchestrationCurrentCloseoutGuard(doc, fields, body) {
  return (
    doc.kind === "task"
    && doc.fields.status === fields.status
    && ORCHESTRATION_REVIEW_STATUSES.has(fields.status)
    && body !== doc.body
  );
}

function requiresOrchestrationStartPreflightGuard(doc, fields) {
  return doc.kind === "task" && doc.fields.status !== fields.status && fields.status === "doing";
}

function orchestrationStartPreflightProblem(doc) {
  if (
    doc.kind !== "task"
    || doc.fields.status !== "doing"
    || !requiresOrchestrationStartPreflight(doc)
    || !isSubstantialOrchestrationTask(doc)
  ) {
    return null;
  }
  const log = sectionText(doc.body, "Log");
  if (hasSmallScopeOrchestrationException(log)) {
    return null;
  }
  const problem = orchestrationPreflightProblem(doc);
  if (!problem) {
    return null;
  }
  return {
    ...problem,
    code: "orchestration_start_preflight_missing",
    message: `${doc.fields.id}: substantial pipeline/orchestration task needs orchestration preflight before doing (missing/invalid: ${problem.missingFields.join(", ")})`,
  };
}

function orchestrationEvidenceProblem(doc, root = "") {
  if (doc.kind !== "task" || !isSubstantialOrchestrationTask(doc)) {
    return null;
  }
  const log = sectionText(doc.body, "Log");
  if (hasSmallScopeOrchestrationException(log)) {
    return null;
  }
  const requiredFields = ORCHESTRATION_REQUIRED_FIELDS;
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
    acceptedFields: requiredFields.map(([name]) => name),
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

// The orchestration label guard applies ONLY to genuine pipeline/orchestration
// meta-work (keyword match). Plain game / visual / asset slices are coupled
// single-agent work — the lead delegates by judgment, not by mandate — so they
// are NOT force-gated with an orchestration packet. (The old broad domain+scope
// classifier swept game tasks in; that was force-gating-era over-reach, removed.)
function isSubstantialOrchestrationTask(doc) {
  const haystack = [
    doc.fields.title || "",
    Array.isArray(doc.fields.tags) ? doc.fields.tags.join(" ") : doc.fields.tags || "",
    doc.body || "",
  ].join("\n").toLowerCase();
  return ORCHESTRATION_KEYWORDS.some((keyword) => haystack.includes(keyword.toLowerCase()));
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
  return taskIdAtLeast(doc, ARCHIVED_ORCHESTRATION_GUARD_MIN_TASK_ID);
}

function missingOrchestrationFields(log, options = {}) {
  const {
    requireBoundedAllowedFiles = false,
    requiredFields = ORCHESTRATION_REQUIRED_FIELDS,
  } = typeof options === "boolean" ? {} : options;
  const blocks = orchestrationUsedBlocks(log);
  if (!blocks.length) {
    return ["orchestration: used packet"];
  }
  const baseline = requiredFields.map(([name]) => name);
  if (requireBoundedAllowedFiles) baseline.push("allowed files bounds");
  let bestMissing = baseline;
  for (const block of blocks) {
    const missing = requiredFields
      .filter(([, pattern]) => !hasMeaningfulFieldValue(block, pattern))
      .map(([name]) => name);
    if (
      requireBoundedAllowedFiles
      && hasMeaningfulFieldValue(block, ORCHESTRATION_ALLOWED_FILES_FIELD_PATTERN)
      && !isBoundedOrchestrationAllowedFiles(fieldValue(block, ORCHESTRATION_ALLOWED_FILES_FIELD_PATTERN))
    ) {
      missing.push("allowed files bounds");
    }
    if (missing.length < bestMissing.length) {
      bestMissing = missing;
    }
  }
  return bestMissing;
}

function hasText(value) {
  return typeof value === "string" && value.trim() && !/^(tbd|todo|none|n\/a|na|unknown|\.\.\.)$/i.test(value.trim());
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

export function isBoundedOrchestrationAllowedFiles(text) {
  return boundedAllowedFilesProblem(text) === "";
}

function boundedAllowedFilesProblem(text) {
  const value = String(text || "").trim();
  if (!value || /^(tbd|todo|none|n\/a|na|unknown|\.\.\.)$/i.test(value)) return "empty allowed files";
  const entries = value
    .split(/[;,]/)
    .map((entry) => entry.trim().replace(/^[`'"]+|[`'"]+$/g, ""))
    .filter(Boolean);
  if (entries.length === 0) return "empty allowed files";
  if (entries.length > 16) return "too many allowed file entries";
  for (const entry of entries) {
    const normalized = entry.replaceAll("\\", "/");
    if (!normalized || /\s/.test(normalized)) return `invalid allowed file entry: ${entry}`;
    if (/^[a-z][a-z0-9+.-]*:/i.test(normalized)) return `non-local allowed file entry: ${entry}`;
    if (/^(?:[a-z]:|\/)/i.test(normalized)) return `absolute allowed file entry: ${entry}`;
    if (normalized.includes("//")) return `invalid allowed file entry: ${entry}`;
    if (normalized === "." || normalized === "*" || normalized === "**") return `too broad allowed file entry: ${entry}`;
    if (normalized.endsWith("/")) return `directory-only allowed file entry: ${entry}`;
    const segments = normalized.split("/");
    if (segments.some((segment) => !segment || segment === "." || segment === "..")) {
      return `path traversal allowed file entry: ${entry}`;
    }
    const recursiveIndex = segments.indexOf("**");
    if (recursiveIndex >= 0) {
      if (recursiveIndex !== segments.length - 1 || segments.length < 3) return `too broad allowed file entry: ${entry}`;
      continue;
    }
    if (segments.some((segment) => segment.includes("**"))) return `too broad allowed file entry: ${entry}`;
    const wildcardSegments = segments.filter((segment) => /[*?[\]{}]/.test(segment));
    if (wildcardSegments.length > 1 || (wildcardSegments.length === 1 && wildcardSegments[0] !== segments[segments.length - 1])) {
      return `unbounded wildcard allowed file entry: ${entry}`;
    }
    const leaf = segments[segments.length - 1];
    if (/[*?[\]{}]/.test(leaf) && (leaf === "*" || !/\.[^./*?[\]{}]+$/.test(leaf))) {
      return `unbounded wildcard allowed file entry: ${entry}`;
    }
    if (!/[*?[\]{}]/.test(leaf) && !leaf.includes(".")) {
      return `directory-like allowed file entry: ${entry}`;
    }
  }
  return "";
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
  return ORCHESTRATION_PREFLIGHT_FIELDS.some(([, pattern]) => pattern.test(label));
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

