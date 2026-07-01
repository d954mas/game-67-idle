#!/usr/bin/env node
// Taskboard CLI for humans and agents.
//
//   node ai_studio/taskboard/cli.mjs list [--json] [--status s] [--project P001] [--epic E001] [--tag t] [--ideas] [--all] [--archive]
//   node ai_studio/taskboard/cli.mjs summary [--json] [--tasks-limit 5]
//   node ai_studio/taskboard/cli.mjs show T0001 [--json]
//   node ai_studio/taskboard/cli.mjs new project --title "..." [--kind ai-studio|game|template|tooling|research|other] [--target ai_studio]
//   node ai_studio/taskboard/cli.mjs new epic --title "..." [--project P001] [--status active]
//   node ai_studio/taskboard/cli.mjs new task --title "..." [--project P001] [--epic E001] [--priority P1] [--status backlog] [--tags a,b]
//   node ai_studio/taskboard/cli.mjs set T0001 --status doing [--project P001] [--epic E001] [--priority P1] [--title "..."] [--log "evidence line"] [--json]
//   node ai_studio/taskboard/cli.mjs context [--json] [--tasks-limit 25]
//   node ai_studio/taskboard/cli.mjs validate [--json]
//   node ai_studio/taskboard/cli.mjs help
//
// Agents: prefer `new` over hand-writing files so IDs never collide.

import {
  agentContextPayload, agentEpicRow, agentProjectRow, agentTaskRow,
  findRoot, listTasks, listEpics, listProjects, findDoc, createTask, createEpic, createProject,
  updateDoc, validateStoreDetailed,
} from "./lib.mjs";
import { ACTIVE_TASK_STATUSES, idNumber, priorityRank, TASK_STATUSES, taskRank } from "./store.mjs";
import { relative } from "node:path";
import { fail } from "../core_harness/tool_lib/cli.mjs";

const root = findRoot();
const [cmd, ...rest] = process.argv.slice(2);

const USAGE = `usage: cli.mjs <list|summary|context|show|new|set|validate|help> ...

Commands:
  list [--json] [--status s] [--project P001] [--epic E001] [--tag t] [--ideas] [--all] [--archive]
  summary [--json] [--tasks-limit 5]
  context [--json] [--tasks-limit 25]
  show <P###|E###|T####> [--json]
  new project --title "..." [--kind ai-studio|game|template|tooling|research|other] [--target path] [--tags a,b]
  new epic --title "..." [--project P001] [--status active] [--tags a,b]
  new task --title "..." [--project P001] [--epic E001] [--priority P1] [--status backlog] [--tags a,b]
  set <id> [--status s] [--project P001] [--epic E001] [--priority P1] [--title "..."] [--log "..."] [--json]
  validate [--json]
`;

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
  const project = d.kind === "project" ? (f.kind || "-") : (f.project || "-");
  const epic = d.kind === "task" ? (f.epic || "-") : "-";
  return `${f.id}  ${String(f.status).padEnd(7)} ${String(f.priority || "").padEnd(3)} ${String(project).padEnd(8)} ${String(epic).padEnd(5)} ${f.title}${tags}${archive}`;
}

function numberArg(value, fallback) {
  if (value === undefined || value === true || value === "") return fallback;
  const parsed = Number(value);
  return Number.isFinite(parsed) && parsed > 0 ? Math.floor(parsed) : fallback;
}

function writeJson(value) {
  process.stdout.write(`${JSON.stringify(value, null, 2)}\n`);
}

function printUsage() {
  process.stdout.write(USAGE);
}

function statusCounts(tasks) {
  const counts = new Map();
  for (const task of tasks) {
    const key = task.fields.status || "unknown";
    counts.set(key, (counts.get(key) || 0) + 1);
  }
  return TASK_STATUSES.map((status) => `${status}:${counts.get(status) || 0}`).join(" ");
}

function currentWorkTasks(tasks) {
  tasks = tasks.filter((task) => ACTIVE_TASK_STATUSES.includes(task.fields.status));
  tasks.sort((a, b) => taskRank(a) - taskRank(b) || idNumber(b) - idNumber(a) || String(a.fields.id).localeCompare(String(b.fields.id)));
  return tasks;
}

function reviewTasks(tasks) {
  tasks = tasks.filter((task) => task.fields.status === "review");
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
  const tasksLimit = numberArg(options["tasks-limit"], 5);
  const allTasks = listTasks(root);
  const openTasks = currentWorkTasks(allTasks);
  const reviewCount = reviewTasks(allTasks).length;
  const lines = [];
  lines.push("# Taskboard Summary");
  lines.push("");
  lines.push(`active_task_counts: ${statusCounts(allTasks)}`);
  lines.push(`open_work_items: ${openTasks.length}`);
  lines.push(`review_tasks: ${reviewCount}`);
  appendCurrentWork(lines, openTasks, tasksLimit, "node ai_studio/taskboard/cli.mjs context --json");
  lines.push("");
  lines.push("## Top Open Tasks");
  lines.push("");
  for (const task of openTasks.slice(0, tasksLimit)) {
    lines.push(`- ${shortRow(task)}`);
  }
  if (openTasks.length > tasksLimit) {
    lines.push(`- ... ${openTasks.length - tasksLimit} more; run \`node ai_studio/taskboard/cli.mjs context --json\` or \`list --json\` only when needed.`);
  }
  if (openTasks.length === 0) {
    lines.push("- none");
  }
  return `${lines.join("\n")}\n`;
}

function renderContext(root, options) {
  const tasksLimit = numberArg(options["tasks-limit"], 25);
  const allTasks = listTasks(root);
  const tasks = currentWorkTasks(allTasks);

  const lines = [];
  lines.push("# Current Context Digest");
  lines.push("");
  lines.push(`active_task_counts: ${statusCounts(allTasks)}`);
  appendCurrentWork(lines, tasks, tasksLimit, "node ai_studio/taskboard/cli.mjs list --json");
  lines.push("");

  lines.push("## Actionable Tasks");
  lines.push("");
  for (const task of tasks.slice(0, tasksLimit)) {
    lines.push(`- ${shortRow(task)}`);
  }
  if (tasks.length > tasksLimit) {
    lines.push(`- ... ${tasks.length - tasksLimit} more; run \`node ai_studio/taskboard/cli.mjs list --json\` or show a specific task only if needed.`);
  }
  if (tasks.length === 0) {
    lines.push("- none");
  }
  lines.push("");
  lines.push("Next context step: inspect only the linked task files or evidence paths needed for the current decision.");
  return `${lines.join("\n")}\n`;
}

const args = parseArgs(rest);

if (!cmd || cmd === "help" || cmd === "--help" || cmd === "-h") {
  printUsage();
  process.exit(0);
}

switch (cmd) {
  case "list": {
    const hidden = new Set(args.all ? [] : ["idea", "done"]);
    if (args.ideas) hidden.delete("idea");
    if (args.review) hidden.delete("review");
    if (args.status) hidden.clear();
    const projects = listProjects(root);
    const epics = listEpics(root);
    const tasks = listTasks(root, { includeArchive: args.archive === true }).filter((t) => {
      if (hidden.has(t.fields.status)) return false;
      if (args.status && t.fields.status !== args.status) return false;
      if (args.project && t.fields.project !== args.project) return false;
      if (args.epic && t.fields.epic !== args.epic) return false;
      if (args.tag && !(t.fields.tags || []).includes(args.tag)) return false;
      return true;
    });
    const order = new Map(TASK_STATUSES.map((s, i) => [s, i]));
    tasks.sort((a, b) =>
      (order.get(a.fields.status) ?? 99) - (order.get(b.fields.status) ?? 99) ||
      String(a.fields.priority).localeCompare(String(b.fields.priority)));
    if (args.json) {
      writeJson({
        schema: "ai_studio.taskboard.list.v1",
        filters: {
          status: args.status || "",
          project: args.project || "",
          epic: args.epic || "",
          tag: args.tag || "",
          includeArchive: args.archive === true,
          includeAllStatuses: args.all === true,
        },
        tasks: tasks.map((task) => agentTaskRow(root, task)),
        projects: projects.map((project) => agentProjectRow(root, project)),
        epics: epics.map((epic) => agentEpicRow(root, epic)),
      });
      break;
    }
    for (const p of projects) {
      if (!args.status && !args.tag && !args.epic && (!args.project || args.project === p.fields.id)) {
        if (!args.all && p.fields.status === "done") continue;
        console.log(`# ${p.fields.id} ${p.fields.title} (${p.fields.status}, ${p.fields.kind || "other"})`);
      }
    }
    for (const e of epics) {
      if (!args.status && !args.tag && (!args.project || args.project === e.fields.project) && (!args.epic || args.epic === e.fields.id)) {
        if (!args.all && !args.archive && e.fields.status === "done") continue;
        console.log(`# ${e.fields.id} ${e.fields.title} (${e.fields.status}, ${e.fields.project || "no-project"})`);
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
    if (args.json) {
      writeJson(agentContextPayload(root, { limit: numberArg(args["tasks-limit"], 5) }));
      break;
    }
    process.stdout.write(renderSummary(root, args));
    break;
  }
  case "context": {
    if (args.json) {
      writeJson(agentContextPayload(root, { limit: numberArg(args["tasks-limit"], 25) }));
      break;
    }
    process.stdout.write(renderContext(root, args));
    break;
  }
  case "show": {
    const id = args._[0] || fail("usage: show <id>");
    const doc = findDoc(root, id) || fail(`no doc with id ${id}`);
    if (args.json) {
      let row;
      if (doc.kind === "task") {
        row = agentTaskRow(root, doc, { includeBody: true });
      } else if (doc.kind === "epic") {
        row = agentEpicRow(root, doc, { includeBody: true });
      } else {
        row = agentProjectRow(root, doc, { includeBody: true });
      }
      writeJson({
        schema: "ai_studio.taskboard.doc.v1",
        doc: row,
      });
      break;
    }
    console.log(`file: ${relative(root, doc.file)}`);
    for (const [k, v] of Object.entries(doc.fields)) {
      console.log(`${k}: ${Array.isArray(v) ? v.join(", ") : v}`);
    }
    console.log("\n" + doc.body);
    break;
  }
  case "new": {
    const kind = args._[0];
    if (!["project", "epic", "task"].includes(kind)) fail("usage: new project|epic|task --title \"...\"");
    if (!args.title) fail("--title is required");
    const input = {
      title: args.title,
      status: args.status,
      project: args.project,
      epic: args.epic,
      priority: args.priority,
      kind: args.kind,
      target: args.target,
      tags: args.tags ? String(args.tags).split(",").map((s) => s.trim()).filter(Boolean) : [],
    };
    const doc = kind === "task" ? createTask(root, input) : (kind === "epic" ? createEpic(root, input) : createProject(root, input));
    console.log(`created ${doc.fields.id}: ${relative(root, doc.file)}`);
    break;
  }
  case "set": {
    const id = args._[0] || fail("usage: set <id> --field value ...");
    const fields = {};
    for (const key of ["status", "project", "epic", "priority", "title", "target", "kind"]) {
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
    if (!detailedProblems.length) {
      console.log("ok: no problems found");
    } else {
      for (const p of detailedProblems) {
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
    printUsage();
    process.exit(1);
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
    return "use task statuses idea/backlog/todo/doing/review/done or project/epic statuses idea/active/done";
  }
  if (problem.includes("references missing project")) {
    return "create the project first, fix the `project:` value, or clear it if the item is intentionally unassigned";
  }
  if (problem.includes("does not match epic")) {
    return "make the task `project:` match its epic, or move the task to an epic in the intended project";
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
  if (problem.includes("active project needs")) {
    return "fill `## Goal`, `## In scope`, and `## Out of scope`, or move the project back to `status: idea`";
  }
  return "";
}
