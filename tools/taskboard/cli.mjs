#!/usr/bin/env node
// Taskboard CLI for humans and agents.
//
//   node tools/taskboard/cli.mjs list [--status s] [--epic E001] [--tag t] [--ideas] [--review] [--all] [--archive]
//   node tools/taskboard/cli.mjs summary [--tasks-limit 5]
//   node tools/taskboard/cli.mjs show T0001
//   node tools/taskboard/cli.mjs new task --title "..." [--epic E001] [--priority P1] [--status backlog] [--tags a,b]
//   node tools/taskboard/cli.mjs new epic --title "..." [--status active]
//   node tools/taskboard/cli.mjs set T0001 --status doing [--epic E001] [--priority P1] [--title "..."] [--log "evidence line"] [--json]
//   node tools/taskboard/cli.mjs context [--status-max-chars 2400] [--tasks-limit 25]
//   node tools/taskboard/cli.mjs orchestration-template
//   node tools/taskboard/cli.mjs subagent-packet-template|subagent-template
//   node tools/taskboard/cli.mjs subagent-packet-check|subagent-check --file packet.txt|--text "..."|--stdin [--json]
//   node tools/taskboard/cli.mjs orchestration-workflow-template [--task-id T0001] [--json]
//   node tools/taskboard/cli.mjs orchestration-workflow-init <task-id>|--id <task-id>|--file tasks/active/T0001-example.md|--current [--status review] [--packet-status integrated] [--output tasks/workflows/T0001.json] [--write] [--force] [--json]
//   node tools/taskboard/cli.mjs orchestration-workflow-check <task-id>|--id <task-id>|--file tasks/active/T0001-example.md|--current [--json]
//   node tools/taskboard/cli.mjs orchestration-bootstrap --title "..." --objective "..." --allowed-files "..." --expected-output "..." --evidence-command "..." --stop-condition "..." --independent-reviewer "..." [--tool-use-guard "..."] [--tags a,b] [--json]
//   node tools/taskboard/cli.mjs orchestration-check <task-id>|--id <task-id>|--file tasks/active/T0001-example.md|--current [--json]
//   node tools/taskboard/cli.mjs validate [--json]
//
// Agents: prefer `new` over hand-writing files so IDs never collide.

import {
  findRoot, listTasks, listEpics, findDoc, createTask, createEpic,
  updateDoc, validateStore, validateStoreDetailed, TASK_STATUSES,
  LIVE_STATUS_MAX_CHARS, orchestrationPacketTemplate,
  subagentPacketTemplate, subagentPacketProblem, orchestrationWorkflowTemplate,
  orchestrationWorkflowInitPayload,
  orchestrationPreflightProblem, orchestrationWorkflowManifestProblem,
  parseDoc, currentDoingOrchestrationTaskIds,
  isCloseoutReadyMachineEvidenceCommand, isBoundedOrchestrationAllowedFiles,
  DEFAULT_ORCHESTRATION_TOOL_USE_GUARD,
} from "./lib.mjs";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join, relative, resolve } from "node:path";

const root = findRoot();
const [cmd, ...rest] = process.argv.slice(2);

function parseArgs(args) {
  const out = { _: [] };
  for (let i = 0; i < args.length; i++) {
    if (args[i].startsWith("--")) {
      const key = args[i].slice(2);
      const next = args[i + 1];
      if (next !== undefined && !next.startsWith("--")) {
        out[key] = next;
        i++;
      } else {
        out[key] = true;
      }
    } else {
      out._.push(args[i]);
    }
  }
  return out;
}

function fail(msg) {
  console.error(`error: ${msg}`);
  process.exit(1);
}

function shortRow(d) {
  const f = d.fields;
  const tags = (f.tags || []).length ? ` [${f.tags.join(",")}]` : "";
  const archive = d.archived ? " (archive)" : "";
  return `${f.id}  ${String(f.status).padEnd(7)} ${String(f.priority || "").padEnd(3)} ${String(f.epic || "-").padEnd(5)} ${f.title}${tags}${archive}`;
}

function numberArg(value, fallback) {
  if (value === undefined || value === true || value === "") return fallback;
  const parsed = Number(value);
  return Number.isFinite(parsed) && parsed > 0 ? Math.floor(parsed) : fallback;
}

function clampText(text, maxChars) {
  if (text.length <= maxChars) return { text: text.trim(), truncated: false };
  const clipped = text.slice(0, Math.max(0, maxChars - 80)).replace(/\s+$/, "");
  return {
    text: `${clipped}\n\n... truncated ${text.length - clipped.length} chars; inspect tasks/STATUS.md or linked task files only if needed.`,
    truncated: true,
  };
}

function writeJson(value) {
  process.stdout.write(`${JSON.stringify(value, null, 2)}\n`);
}

const CURRENT_PREFLIGHT_NEXT_ACTION = "create or refine exactly one `doing` pipeline/orchestration task, then run `node tools/ai.mjs orchestration-check --current --json`";
const AMBIGUOUS_CURRENT_PREFLIGHT_NEXT_ACTION = "set exactly one pipeline/orchestration task to `doing`, then run `node tools/ai.mjs orchestration-check --current --json`";
const ORCHESTRATION_BOOTSTRAP_REQUIRED_ARGS = [
  "title",
  "objective",
  "allowed-files",
  "expected-output",
  "evidence-command",
  "stop-condition",
  "independent-reviewer",
];

function currentSelectorProblem(code, message, ids = [], nextAction = CURRENT_PREFLIGHT_NEXT_ACTION) {
  return {
    code,
    selector: "current",
    taskIds: ids,
    message,
    nextAction,
  };
}

function isSelectorProblem(error) {
  return error && typeof error === "object" && error.selector === "current" && typeof error.message === "string";
}

function argText(args, key) {
  const value = args[key];
  return typeof value === "string" ? value.trim() : "";
}

function splitTags(value) {
  return value ? String(value).split(",").map((s) => s.trim()).filter(Boolean) : [];
}

function uniqueTags(values) {
  return [...new Set(values.filter(Boolean))];
}

function missingBootstrapArgs(args) {
  return ORCHESTRATION_BOOTSTRAP_REQUIRED_ARGS
    .filter((key) => !argText(args, key))
    .map((key) => `--${key}`);
}

function bootstrapMissingProblem(missingArgs) {
  return {
    code: "missing_required_argument",
    missingArgs,
    message: `orchestration-bootstrap missing required argument(s): ${missingArgs.join(", ")}`,
  };
}

function bootstrapCurrentProblem(ids) {
  return {
    code: "current_task_exists",
    taskIds: ids,
    message: `current doing pipeline/orchestration task already exists: ${ids.join(", ")}; finish or move it before bootstrapping another`,
  };
}

function bootstrapMachineEvidenceProblem(command) {
  return {
    code: "invalid_evidence_command",
    evidenceCommand: command,
    message: "--evidence-command must be closeout-ready machine evidence: status --agent-rollup --require-agent-rollup-ok with a source plus --agent-rollup-evidence --json-output, or orchestration-trace with source plus --json-output",
  };
}

function bootstrapAllowedFilesProblem(value) {
  return {
    code: "invalid_allowed_files",
    allowedFiles: value,
    message: "--allowed-files must be bounded repo-local file paths or final-segment file patterns, separated by comma or semicolon",
  };
}

function orchestrationBootstrapUsage() {
  return `usage: node tools/taskboard/cli.mjs orchestration-bootstrap --title "..." --objective "..." --allowed-files "..." --expected-output "..." --evidence-command "..." --stop-condition "..." --independent-reviewer "..." [--tool-use-guard "..."] [--tags a,b] [--json]

Creates one current \`doing\` pipeline/orchestration task with a complete packet.

Required:
  --title                 Short task title.
  --objective             Bounded work objective.
  --allowed-files         Repo-local files or bounded patterns, separated by comma or semicolon.
  --expected-output       Concrete output the task must produce.
  --evidence-command      Machine orchestration evidence command.
  --stop-condition        Validation and closeout condition.
  --independent-reviewer  Reviewer/verifier plan.

After creation:
  node tools/ai.mjs orchestration-check --current --json`;
}

function orchestrationBootstrapBody(args) {
  return `## What

${argText(args, "objective")}

## Done when

- [ ] ${argText(args, "stop-condition")}

## Open questions

## Log

- orchestration: used
  objective: ${argText(args, "objective")}
  allowed files: ${argText(args, "allowed-files")}
  tool-use guard: ${argText(args, "tool-use-guard") || DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}
  expected output: ${argText(args, "expected-output")}
  evidence command: ${argText(args, "evidence-command")}
  stop condition: ${argText(args, "stop-condition")}
  independent reviewer: ${argText(args, "independent-reviewer")}
`;
}

function readTaskFileArg(value) {
  const requested = value || fail("usage: orchestration-check <task-id>|--id <task-id>|--file <task.md>|--current");
  const file = isAbsolute(requested) ? resolve(requested) : resolve(root, requested);
  const rel = relative(root, file);
  if (rel.startsWith("..") || isAbsolute(rel)) {
    fail("--file must be inside the repository root");
  }
  if (!existsSync(file)) {
    fail(`no such file: ${requested}`);
  }
  const parsed = parseDoc(readFileSync(file, "utf8"));
  return {
    ...parsed,
    kind: "task",
    file,
    fields: {
      ...parsed.fields,
      display_id: parsed.fields.id || rel,
    },
  };
}

function readSubagentPacketArg(args) {
  const inputCount = [args.file, args.text, args.stdin].filter(Boolean).length;
  if (inputCount > 1) fail("use only one subagent-packet-check input: --file, --text, or --stdin");
  if (typeof args.text === "string") return args.text;
  if (args.stdin) return readFileSync(0, "utf8");
  if (args.file) {
    const requested = args.file;
    const file = isAbsolute(requested) ? resolve(requested) : resolve(root, requested);
    const rel = relative(root, file);
    if (rel.startsWith("..") || isAbsolute(rel)) {
      fail("--file must be inside the repository root");
    }
    if (!existsSync(file)) {
      fail(`no such file: ${requested}`);
    }
    return readFileSync(file, "utf8");
  }
  fail("usage: subagent-packet-check --file packet.txt|--text \"...\"|--stdin [--json]");
}

function workflowOutputPath(taskId, args) {
  const requested = typeof args.output === "string" && args.output.trim()
    ? args.output.trim()
    : `tasks/workflows/${taskId}.json`;
  if (!requested.toLowerCase().endsWith(".json")) fail("--output must be a .json path");
  const file = isAbsolute(requested) ? resolve(requested) : resolve(root, requested);
  const rel = relative(root, file);
  if (rel.startsWith("..") || isAbsolute(rel)) fail("--output must be inside the repository root");
  const normalized = rel.replaceAll("\\", "/");
  if (!/^tasks\/workflows\/T\d+[^/]*\.json$/i.test(normalized)) fail("--output must be under tasks/workflows/T*.json");
  return { file, rel };
}

function readTaskForOrchestrationCheck(args) {
  const id = args.id || args._[0];
  const selectors = [Boolean(args.file), Boolean(id), Boolean(args.current)].filter(Boolean).length;
  if (selectors > 1) {
    fail("use only one selector: <task-id>, --id, --file, or --current");
  }
  if (args.current) {
    const ids = currentDoingOrchestrationTaskIds(root);
    if (ids.length === 0) {
      throw currentSelectorProblem(
        "current_task_missing",
        "no current doing pipeline/orchestration task; create or set exactly one task to doing first",
      );
    }
    if (ids.length > 1) {
      throw currentSelectorProblem(
        "current_task_ambiguous",
        `multiple current doing pipeline/orchestration tasks: ${ids.join(", ")}; select one explicitly`,
        ids,
        AMBIGUOUS_CURRENT_PREFLIGHT_NEXT_ACTION,
      );
    }
    const doc = findDoc(root, ids[0]);
    if (!doc || doc.kind !== "task") {
      throw currentSelectorProblem("current_task_unresolved", `current task ${ids[0]} could not be resolved`, ids);
    }
    return doc;
  }
  if (args.file) return readTaskFileArg(args.file);
  if (!id) {
    fail("usage: orchestration-check <task-id>|--id <task-id>|--file <task.md>|--current");
  }
  const doc = findDoc(root, id);
  if (!doc || doc.kind !== "task") {
    fail(`no task with id ${id}`);
  }
  return doc;
}

function sectionText(markdown, title) {
  const pattern = new RegExp(`(?:^|\\r?\\n)## ${escapeRegExp(title)}[ \\t]*\\r?\\n([\\s\\S]*?)(?=\\r?\\n##\\s+|$)`, "i");
  const match = markdown.match(pattern);
  return match ? match[1].trim() : "";
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function statusCounts(tasks) {
  const counts = new Map();
  for (const task of tasks) {
    const key = task.fields.status || "unknown";
    counts.set(key, (counts.get(key) || 0) + 1);
  }
  return TASK_STATUSES.map((status) => `${status}:${counts.get(status) || 0}`).join(" ");
}

function priorityRank(priority) {
  return { P0: 0, P1: 1, P2: 2, P3: 3 }[priority] ?? 9;
}

function taskRank(task) {
  const statusRank = { doing: 0, todo: 1, backlog: 2, review: 3, idea: 4, done: 5, dropped: 6 }[task.fields.status] ?? 9;
  return statusRank * 10 + priorityRank(task.fields.priority);
}

const PIPELINE_CONTEXT_TAGS = new Set([
  "pipeline",
  "ai-pipeline",
  "orchestration",
  "taskboard",
  "profiling",
  "subagent",
  "subagents",
  "context",
  "context-budget",
  "skills",
  "skills-eval",
  "skills-sync",
  "validation",
  "tooling",
]);

function isPipelineContextTask(task) {
  return (task.fields.tags || []).some((tag) => PIPELINE_CONTEXT_TAGS.has(String(tag).toLowerCase()));
}

function hasOnlyPipelineCurrentWork(tasks) {
  return tasks.length > 0 && tasks.every(isPipelineContextTask);
}

function idNumber(task) {
  const match = String(task.fields.id || "").match(/\d+/);
  return match ? Number(match[0]) : 0;
}

function currentWorkTasks(root) {
  const tasks = listTasks(root).filter((task) => ["backlog", "todo", "doing"].includes(task.fields.status));
  tasks.sort((a, b) => taskRank(a) - taskRank(b) || idNumber(b) - idNumber(a) || String(a.fields.id).localeCompare(String(b.fields.id)));
  return tasks;
}

function reviewTasks(root) {
  const tasks = listTasks(root).filter((task) => task.fields.status === "review");
  tasks.sort((a, b) => priorityRank(a.fields.priority) - priorityRank(b.fields.priority) || idNumber(b) - idNumber(a) || String(a.fields.id).localeCompare(String(b.fields.id)));
  return tasks;
}

function appendCurrentWork(lines, tasks, limit, overflowCommand) {
  if (tasks.length === 0) return;
  lines.push("");
  lines.push("## Current Work");
  lines.push("");
  for (const task of tasks.slice(0, limit)) {
    lines.push(`- ${shortRow(task)}`);
  }
  if (tasks.length > limit) {
    lines.push(`- ... ${tasks.length - limit} more; run \`${overflowCommand}\` only when needed.`);
  }
}

function renderSummary(root, options) {
  const statusFile = join(root, "tasks", "STATUS.md");
  const status = existsSync(statusFile) ? readFileSync(statusFile, "utf8") : "";
  const tasksLimit = numberArg(options["tasks-limit"], 5);
  const openTasks = currentWorkTasks(root);
  const reviewCount = reviewTasks(root).length;
  const lines = [];
  lines.push("# Taskboard Summary");
  lines.push("");
  lines.push(`active_task_counts: ${statusCounts(listTasks(root))}`);
  lines.push(`open_actionable_tasks: ${openTasks.length}`);
  lines.push(`review_tasks: ${reviewCount}`);
  appendCurrentWork(lines, openTasks, tasksLimit, "node tools/taskboard/cli.mjs context");
  const pipelineCurrentWork = hasOnlyPipelineCurrentWork(openTasks);
  const currentGoal = sectionText(status, "Current Goal");
  const blockers = sectionText(status, "Blocking Work") || sectionText(status, "Blockers");
  const nextPriorities = sectionText(status, "Next Priorities");
  if (pipelineCurrentWork) {
    lines.push("");
    lines.push("## Status Context");
    lines.push("");
    lines.push("Live game status sections are omitted while current actionable work is pipeline/tooling-scoped.");
  } else {
    for (const [title, body, budget] of [
      ["Current Goal", currentGoal, 500],
      ["Blocking Work", blockers, 500],
      ["Next Priorities", nextPriorities, 700],
    ]) {
      if (!body) continue;
      lines.push("");
      lines.push(`## ${title}`);
      lines.push("");
      lines.push(clampText(body, budget).text || "(empty)");
    }
  }
  lines.push("");
  lines.push("## Top Open Tasks");
  lines.push("");
  for (const task of openTasks.slice(0, tasksLimit)) {
    lines.push(`- ${shortRow(task)}`);
  }
  if (openTasks.length > tasksLimit) {
    lines.push(`- ... ${openTasks.length - tasksLimit} more; run \`node tools/taskboard/cli.mjs context\` or \`list\` only when needed.`);
  }
  if (openTasks.length === 0) {
    lines.push("- none");
    if (reviewCount > 0) {
      lines.push(`- ${reviewCount} task(s) are in review; run \`node tools/taskboard/cli.mjs list --review\` only when reviewing or closing old work.`);
    }
  }
  return `${lines.join("\n")}\n`;
}

function renderContext(root, options) {
  const statusFile = join(root, "tasks", "STATUS.md");
  const status = existsSync(statusFile) ? readFileSync(statusFile, "utf8") : "";
  const statusMaxChars = numberArg(options["status-max-chars"], LIVE_STATUS_MAX_CHARS);
  const tasksLimit = numberArg(options["tasks-limit"], 25);
  const sections = [
    "Current Goal",
    "Blocking Work",
    "Non-blocking Debt",
    "Next Priorities",
    "Current Gate",
    "Required Validation",
    "Last Known Good Evidence",
  ];
  const tasks = currentWorkTasks(root);
  const reviewCount = reviewTasks(root).length;

  const lines = [];
  lines.push("# Current Context Digest");
  lines.push("");
  lines.push(`status_file: ${relative(root, statusFile)}`);
  lines.push(`status_chars: ${status.length}`);
  if (status.length > statusMaxChars) {
    lines.push(`status_warning: large; digest is capped at ${statusMaxChars} chars`);
  }
  lines.push(`active_task_counts: ${statusCounts(listTasks(root))}`);
  appendCurrentWork(lines, tasks, tasksLimit, "node tools/taskboard/cli.mjs list");
  lines.push("");

  const pipelineCurrentWork = hasOnlyPipelineCurrentWork(tasks);
  let remaining = statusMaxChars;
  if (pipelineCurrentWork) {
    lines.push("## Status Context");
    lines.push("");
    lines.push("Live game status sections are omitted while current actionable work is pipeline/tooling-scoped.");
    lines.push("");
  } else {
    for (const section of sections) {
      const body = sectionText(status, section);
      if (!body) continue;
      const sectionBudget = Math.max(600, Math.min(remaining, 1400));
      const clipped = clampText(body, sectionBudget);
      lines.push(`## ${section}`);
      lines.push("");
      lines.push(clipped.text || "(empty)");
      lines.push("");
      remaining -= Math.min(body.length, sectionBudget);
      if (remaining <= 0) break;
    }
  }

  lines.push("## Actionable Tasks");
  lines.push("");
  for (const task of tasks.slice(0, tasksLimit)) {
    lines.push(`- ${shortRow(task)}`);
  }
  if (tasks.length > tasksLimit) {
    lines.push(`- ... ${tasks.length - tasksLimit} more; run \`node tools/taskboard/cli.mjs list\` or show a specific task only if needed.`);
  }
  if (tasks.length === 0) {
    lines.push("- none");
  }
  if (reviewCount > 0) {
    lines.push(`- ${reviewCount} review task(s) hidden from current context; use \`list --review\` for review cleanup.`);
  }
  lines.push("");
  lines.push("Next context step: inspect only the linked task files or evidence paths needed for the current decision.");
  return `${lines.join("\n")}\n`;
}

const args = parseArgs(rest);

switch (cmd) {
  case "list": {
    const hidden = new Set(args.all ? [] : ["idea", "review", "done", "dropped"]);
    if (args.ideas) hidden.delete("idea");
    if (args.review) hidden.delete("review");
    if (args.status) hidden.clear();
    const tasks = listTasks(root, { includeArchive: args.archive === true }).filter((t) => {
      if (hidden.has(t.fields.status)) return false;
      if (args.status && t.fields.status !== args.status) return false;
      if (args.epic && t.fields.epic !== args.epic) return false;
      if (args.tag && !(t.fields.tags || []).includes(args.tag)) return false;
      return true;
    });
    const order = new Map(TASK_STATUSES.map((s, i) => [s, i]));
    tasks.sort((a, b) =>
      (order.get(a.fields.status) ?? 99) - (order.get(b.fields.status) ?? 99) ||
      String(a.fields.priority).localeCompare(String(b.fields.priority)));
    for (const e of listEpics(root)) {
      if (!args.status && !args.tag && (!args.epic || args.epic === e.fields.id)) {
        if (!args.all && !args.archive && ["done", "dropped"].includes(e.fields.status)) continue;
        console.log(`# ${e.fields.id} ${e.fields.title} (${e.fields.status})`);
      }
    }
    for (const t of tasks) {
      console.log(shortRow(t));
    }
    if (!tasks.length) {
      console.log("(no tasks match)");
    }
    break;
  }
  case "summary": {
    process.stdout.write(renderSummary(root, args));
    break;
  }
  case "context": {
    process.stdout.write(renderContext(root, args));
    break;
  }
  case "orchestration-template": {
    console.log(orchestrationPacketTemplate());
    break;
  }
  case "subagent-packet-template":
  case "subagent-template": {
    console.log(subagentPacketTemplate());
    break;
  }
  case "subagent-packet-check":
  case "subagent-check": {
    const problem = subagentPacketProblem(readSubagentPacketArg(args));
    if (args.json) {
      writeJson({
        ok: !problem,
        problem,
      });
      process.exit(problem ? 1 : 0);
    }
    if (problem) {
      console.log(`problem: ${problem.message}`);
      console.log("hint: start from `node tools/taskboard/cli.mjs subagent-packet-template`");
      process.exit(1);
    }
    console.log("ok: subagent packet passed");
    break;
  }
  case "orchestration-workflow-template": {
    const taskId = typeof args["task-id"] === "string" ? args["task-id"].trim() : "T0000";
    const payload = orchestrationWorkflowTemplate(taskId || "T0000");
    if (args.json) {
      writeJson(payload);
    } else {
      console.log(JSON.stringify(payload, null, 2));
    }
    break;
  }
  case "orchestration-workflow-init": {
    let doc;
    try {
      doc = readTaskForOrchestrationCheck(args);
    } catch (error) {
      if (isSelectorProblem(error)) {
        if (args.json) {
          writeJson({
            ok: false,
            file: null,
            problem: error,
          });
          process.exit(1);
        }
        fail(error.message);
      }
      throw error;
    }
    const manifest = orchestrationWorkflowInitPayload(doc, {
      status: typeof args.status === "string" ? args.status.trim() : "",
      packetStatus: typeof args["packet-status"] === "string" ? args["packet-status"].trim() : "",
    });
    const output = workflowOutputPath(doc.fields?.id || manifest.task_id, args);
    const exists = existsSync(output.file);
    if (args.write && exists && !args.force) {
      const problem = {
        code: "workflow_manifest_exists",
        path: output.rel,
        message: `workflow manifest already exists: ${output.rel}; pass --force to overwrite`,
      };
      if (args.json) {
        writeJson({ ok: false, path: output.rel, manifest, problem });
        process.exit(1);
      }
      fail(problem.message);
    }
    if (args.write) {
      mkdirSync(dirname(output.file), { recursive: true });
      writeFileSync(output.file, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
    }
    const payload = {
      ok: true,
      wrote: Boolean(args.write),
      path: output.rel,
      log_line: `- workflow manifest: ${output.rel.replaceAll("\\", "/")}`,
      manifest,
    };
    if (args.json) {
      writeJson(payload);
    } else {
      console.log(args.write ? `wrote ${output.rel}` : `would write ${output.rel}`);
      console.log(payload.log_line);
      console.log(JSON.stringify(manifest, null, 2));
    }
    break;
  }
  case "orchestration-workflow-check":
  case "workflow-check": {
    let doc;
    try {
      doc = readTaskForOrchestrationCheck(args);
    } catch (error) {
      if (isSelectorProblem(error)) {
        if (args.json) {
          writeJson({
            ok: false,
            file: null,
            problem: error,
          });
          process.exit(1);
        }
        fail(error.message);
      }
      throw error;
    }
    const problem = orchestrationWorkflowManifestProblem(doc, root);
    if (args.json) {
      writeJson({
        ok: !problem,
        file: relative(root, doc.file),
        problem: problem ? {
          code: "orchestration_workflow_manifest_invalid",
          taskId: doc.fields?.id || "",
          message: `${doc.fields?.id || "task"}: workflow manifest failed (${problem})`,
          missingFields: [problem],
        } : null,
      });
      process.exit(problem ? 1 : 0);
    }
    if (problem) {
      console.log(`problem: ${doc.fields?.id || "task"} workflow manifest failed (${problem})`);
      console.log("hint: record `- workflow manifest: tasks/workflows/<task-id>.json` and validate the artifact before closeout");
      process.exit(1);
    }
    console.log(`ok: workflow manifest passed for ${relative(root, doc.file)}`);
    break;
  }
  case "orchestration-bootstrap": {
    if (args.help === true || args.h === true) {
      console.log(orchestrationBootstrapUsage());
      break;
    }
    const missingArgs = missingBootstrapArgs(args);
    if (missingArgs.length) {
      const problem = bootstrapMissingProblem(missingArgs);
      if (args.json) {
        writeJson({ ok: false, problem });
        process.exit(1);
      }
      fail(problem.message);
    }
    if (!isBoundedOrchestrationAllowedFiles(argText(args, "allowed-files"))) {
      const problem = bootstrapAllowedFilesProblem(argText(args, "allowed-files"));
      if (args.json) {
        writeJson({ ok: false, problem });
        process.exit(1);
      }
      fail(problem.message);
    }
    if (!isCloseoutReadyMachineEvidenceCommand(argText(args, "evidence-command"))) {
      const problem = bootstrapMachineEvidenceProblem(argText(args, "evidence-command"));
      if (args.json) {
        writeJson({ ok: false, problem });
        process.exit(1);
      }
      fail(problem.message);
    }
    const currentIds = currentDoingOrchestrationTaskIds(root);
    if (currentIds.length) {
      const problem = bootstrapCurrentProblem(currentIds);
      if (args.json) {
        writeJson({ ok: false, problem });
        process.exit(1);
      }
      fail(problem.message);
    }
    const tags = uniqueTags([
      "pipeline",
      "orchestration",
      "subagents",
      ...splitTags(args.tags),
    ]);
    const doc = createTask(root, {
      title: argText(args, "title"),
      status: "doing",
      epic: argText(args, "epic"),
      priority: argText(args, "priority"),
      tags,
      body: orchestrationBootstrapBody(args),
    });
    const payload = {
      ok: true,
      doc: {
        id: doc.fields.id,
        status: doc.fields.status,
        tags: doc.fields.tags || [],
        file: relative(root, doc.file),
      },
      nextAction: "node tools/ai.mjs orchestration-check --current --json",
    };
    if (args.json) {
      writeJson(payload);
    } else {
      console.log(`created ${doc.fields.id}: ${relative(root, doc.file)}`);
      console.log(`next: ${payload.nextAction}`);
    }
    break;
  }
  case "orchestration-check": {
    let doc;
    try {
      doc = readTaskForOrchestrationCheck(args);
    } catch (error) {
      if (isSelectorProblem(error)) {
        if (args.json) {
          writeJson({
            ok: false,
            file: null,
            problem: error,
          });
          process.exit(1);
        }
        fail(error.message);
      }
      throw error;
    }
    const problem = orchestrationPreflightProblem(doc);
    if (args.json) {
      writeJson({
        ok: !problem,
        file: relative(root, doc.file),
        problem,
      });
      process.exit(problem ? 1 : 0);
    }
    if (problem) {
      console.log(`problem: ${problem.message}`);
      console.log(`hint: use a complete packet from \`node tools/taskboard/cli.mjs orchestration-template\` before launching subagents:`);
      console.log(orchestrationPacketTemplate());
      process.exit(1);
    }
    console.log(`ok: orchestration packet preflight passed for ${relative(root, doc.file)}`);
    break;
  }
  case "show": {
    const id = args._[0] || fail("usage: show <id>");
    const doc = findDoc(root, id) || fail(`no doc with id ${id}`);
    console.log(`file: ${relative(root, doc.file)}`);
    for (const [k, v] of Object.entries(doc.fields)) {
      console.log(`${k}: ${Array.isArray(v) ? v.join(", ") : v}`);
    }
    console.log("\n" + doc.body);
    break;
  }
  case "new": {
    const kind = args._[0];
    if (kind !== "task" && kind !== "epic") fail("usage: new task|epic --title \"...\"");
    if (!args.title) fail("--title is required");
    const input = {
      title: args.title,
      status: args.status,
      epic: args.epic,
      priority: args.priority,
      tags: args.tags ? String(args.tags).split(",").map((s) => s.trim()).filter(Boolean) : [],
    };
    const doc = kind === "task" ? createTask(root, input) : createEpic(root, input);
    console.log(`created ${doc.fields.id}: ${relative(root, doc.file)}`);
    break;
  }
  case "set": {
    const id = args._[0] || fail("usage: set <id> --field value ...");
    const fields = {};
    for (const key of ["status", "epic", "priority", "title"]) {
      if (args[key] !== undefined) fields[key] = args[key];
    }
    if (args.tags !== undefined) {
      fields.tags = String(args.tags).split(",").map((s) => s.trim()).filter(Boolean);
    }
    const patch = { fields };
    if (typeof args.log === "string" && args.log) {
      const doc = findDoc(root, id) || fail(`no doc with id ${id}`);
      const stamp = new Date().toISOString().slice(0, 10);
      patch.body = `${doc.body.replace(/\s+$/, "")}\n- ${stamp}: ${args.log}\n`;
    }
    if (!Object.keys(fields).length && !patch.body) fail("nothing to set");
    try {
      const doc = updateDoc(root, id, patch);
      if (args.json) {
        writeJson({ ok: true, doc: { id: doc.fields.id, status: doc.fields.status, file: relative(root, doc.file) } });
      } else {
        console.log(`updated ${id}: ${shortRow(doc)}`);
      }
    } catch (err) {
      if (args.json) {
        writeJson({ ok: false, problem: err.problem || { code: "taskboard_error", message: err.message } });
        process.exit(1);
      }
      fail(err.message);
    }
    break;
  }
  case "validate": {
    const detailedProblems = validateStoreDetailed(root);
    if (args.json) {
      writeJson({ ok: detailedProblems.length === 0, problems: detailedProblems });
      process.exit(detailedProblems.length ? 1 : 0);
    }
    const problems = validateStore(root);
    if (!problems.length) {
      console.log("ok: no problems found");
    } else {
      for (const p of problems) {
        console.log(`problem: ${p}`);
        const hint = remediationHint(p);
        if (hint) {
          console.log(`hint: ${hint}`);
        }
      }
      process.exit(1);
    }
    break;
  }
  default:
    console.log("usage: cli.mjs <list|context|show|new|set|orchestration-template|subagent-packet-template|subagent-template|subagent-packet-check|subagent-check|orchestration-workflow-template|orchestration-workflow-init|orchestration-workflow-check|workflow-check|orchestration-bootstrap|orchestration-check|validate> ...  (see header comment)");
    process.exit(cmd ? 1 : 0);
}

function remediationHint(problem) {
  if (problem.includes("missing id")) {
    return "create or repair the item with `node tools/taskboard/cli.mjs new`, or add a unique id in frontmatter";
  }
  if (problem.includes("duplicate id")) {
    return "keep one canonical file for that id; rename or drop the duplicate instead of editing both";
  }
  if (problem.includes("missing title")) {
    return "add a short `title:` in frontmatter so list output is readable";
  }
  if (problem.includes("invalid status")) {
    return "use task statuses idea/backlog/todo/doing/review/done/dropped or epic statuses idea/active/done/dropped";
  }
  if (problem.includes("references missing epic")) {
    return "create the epic first, fix the task's `epic:` value, or clear it if the task is unassigned";
  }
  if (problem.includes("actionable task needs")) {
    return "fill `## What` and at least one checkable `## Done when`, or move raw work back to `status: idea`";
  }
  if (problem.includes("active epic needs")) {
    return "fill `## Goal`, `## In scope`, and `## Out of scope`, or move the epic back to `status: idea`";
  }
  if (problem.includes("exceeds live status budget")) {
    return "replace inline history with pointers to `tasks/archive/` or `gamedesign/projects/<game-id>/`; keep `STATUS.md` as a current index";
  }
  if (problem.includes("substantial pipeline/orchestration task needs orchestration evidence")) {
    return `add a complete packet from \`node tools/taskboard/cli.mjs orchestration-template\`:\n${orchestrationPacketTemplate()}\nor record \`orchestration: not needed - small scope: one-file/docs-only/no code ...\``;
  }
  return "";
}
