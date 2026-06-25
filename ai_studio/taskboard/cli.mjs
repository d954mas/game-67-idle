#!/usr/bin/env node
// Taskboard CLI for humans and agents.
//
//   node ai_studio/taskboard/cli.mjs list [--status s] [--epic E001] [--tag t] [--ideas] [--review] [--all] [--archive]
//   node ai_studio/taskboard/cli.mjs summary [--tasks-limit 5]
//   node ai_studio/taskboard/cli.mjs show T0001
//   node ai_studio/taskboard/cli.mjs new task --title "..." [--epic E001] [--priority P1] [--status backlog] [--tags a,b]
//   node ai_studio/taskboard/cli.mjs new epic --title "..." [--status active]
//   node ai_studio/taskboard/cli.mjs set T0001 --status doing [--epic E001] [--priority P1] [--title "..."] [--log "evidence line"] [--json]
//   node ai_studio/taskboard/cli.mjs context [--status-max-chars 2400] [--tasks-limit 25]
//   node ai_studio/taskboard/cli.mjs validate [--json]
//
// Agents: prefer `new` over hand-writing files so IDs never collide.

import {
  findRoot, listTasks, listEpics, findDoc, createTask, createEpic,
  updateDoc, validateStoreDetailed, TASK_STATUSES,
  LIVE_STATUS_MAX_CHARS,
} from "./lib.mjs";
import { orchestrationPacketTemplate } from "../core_harness/orchestration/lib.mjs";
import { existsSync, readFileSync } from "node:fs";
import { join, relative } from "node:path";
import { fail } from "../../tools/lib/cli.mjs";

const root = findRoot();
const [cmd, ...rest] = process.argv.slice(2);
const ORCHESTRATION_CLI = "node ai_studio/core_harness/orchestration/cli.mjs";

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
  appendCurrentWork(lines, openTasks, tasksLimit, "node ai_studio/taskboard/cli.mjs context");
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
    lines.push(`- ... ${openTasks.length - tasksLimit} more; run \`node ai_studio/taskboard/cli.mjs context\` or \`list\` only when needed.`);
  }
  if (openTasks.length === 0) {
    lines.push("- none");
    if (reviewCount > 0) {
      lines.push(`- ${reviewCount} task(s) are in review; run \`node ai_studio/taskboard/cli.mjs list --review\` only when reviewing or closing old work.`);
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
  appendCurrentWork(lines, tasks, tasksLimit, "node ai_studio/taskboard/cli.mjs list");
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
    lines.push(`- ... ${tasks.length - tasksLimit} more; run \`node ai_studio/taskboard/cli.mjs list\` or show a specific task only if needed.`);
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
    // Orchestration packet checks are ADVISORY everywhere on the store path:
    // validate (here) and the store-mutation checkpoints (createTask/updateDoc,
    // i.e. `new`/`set`) all NUDGE rather than block, so the task store stays
    // decoupled from orchestration policy. The goal (don't silently skip
    // delegation) is kept as a loud nudge. Only the EXPLICIT `orchestration-check`
    // command still fails hard. [REFACTOR_PLAN Phase 1 #2 + p.6 advisory flip]
    const ADVISORY_CODES = new Set([
      "orchestration_start_preflight_missing",
      "orchestration_evidence_missing",
    ]);
    const detailedProblems = validateStoreDetailed(root);
    const blocking = detailedProblems.filter((p) => !ADVISORY_CODES.has(p.code));
    const advisory = detailedProblems.filter((p) => ADVISORY_CODES.has(p.code));
    if (args.json) {
      writeJson({ ok: blocking.length === 0, problems: blocking, advisories: advisory });
      process.exit(blocking.length ? 1 : 0);
    }
    for (const p of advisory) {
      console.log(`nudge: ${p.message}`);
      const hint = remediationHint(p.message);
      if (hint) {
        console.log(`hint: ${hint}`);
      }
    }
    if (!blocking.length) {
      console.log(
        advisory.length
          ? "ok: no blocking problems (orchestration nudges above are advisory)"
          : "ok: no problems found",
      );
    } else {
      for (const p of blocking) {
        console.log(`problem: ${p.message}`);
        const hint = remediationHint(p.message);
        if (hint) {
          console.log(`hint: ${hint}`);
        }
      }
      process.exit(1);
    }
    break;
  }
  default:
    console.log("usage: cli.mjs <list|context|show|new|set|validate> ...");
    process.exit(cmd ? 1 : 0);
}

function remediationHint(problem) {
  if (problem.includes("missing id")) {
    return "create or repair the item with `node ai_studio/taskboard/cli.mjs new`, or add a unique id in frontmatter";
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
    return `add a complete packet from \`${ORCHESTRATION_CLI} orchestration-template\`:\n${orchestrationPacketTemplate()}\nor record \`orchestration: not needed - small scope: one-file/docs-only/no code ...\``;
  }
  return "";
}
