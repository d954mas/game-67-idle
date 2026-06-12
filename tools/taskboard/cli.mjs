#!/usr/bin/env node
// Taskboard CLI for humans and agents.
//
//   node tools/taskboard/cli.mjs list [--status s] [--epic E001] [--tag t] [--ideas] [--all] [--archive]
//   node tools/taskboard/cli.mjs show T0001
//   node tools/taskboard/cli.mjs new task --title "..." [--epic E001] [--priority P1] [--status backlog] [--tags a,b]
//   node tools/taskboard/cli.mjs new epic --title "..." [--status active]
//   node tools/taskboard/cli.mjs set T0001 --status doing [--epic E001] [--priority P1] [--title "..."] [--log "evidence line"]
//   node tools/taskboard/cli.mjs validate
//
// Agents: prefer `new` over hand-writing files so IDs never collide.

import {
  findRoot, listTasks, listEpics, findDoc, createTask, createEpic,
  updateDoc, validateStore, TASK_STATUSES,
} from "./lib.mjs";
import { relative } from "node:path";

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

const args = parseArgs(rest);

switch (cmd) {
  case "list": {
    const hidden = new Set(args.all ? [] : ["idea", "done", "dropped"]);
    if (args.ideas) hidden.delete("idea");
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
      console.log(`updated ${id}: ${shortRow(doc)}`);
    } catch (err) {
      fail(err.message);
    }
    break;
  }
  case "validate": {
    const problems = validateStore(root);
    if (!problems.length) {
      console.log("ok: no problems found");
    } else {
      for (const p of problems) console.log(`problem: ${p}`);
      process.exit(1);
    }
    break;
  }
  default:
    console.log("usage: cli.mjs <list|show|new|set|validate> ...  (see header comment)");
    process.exit(cmd ? 1 : 0);
}
