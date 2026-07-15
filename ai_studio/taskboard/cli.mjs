#!/usr/bin/env node
// Taskboard CLI for humans and agents.
//
//   node ai_studio/taskboard/cli.mjs list [--json] [--status s] [--project P001] [--epic E001] [--tag t] [--ideas] [--all] [--archive]
//   node ai_studio/taskboard/cli.mjs show T0001 [--archive] [--json]
//   node ai_studio/taskboard/cli.mjs archive seal --name 2026-07-closeout [--json]
//   node ai_studio/taskboard/cli.mjs new project --title "..." [--kind ai-studio|game|template|tooling|research|other] [--target ai_studio]
//   node ai_studio/taskboard/cli.mjs new epic --title "..." [--project P001] [--status active]
//   node ai_studio/taskboard/cli.mjs new task --title "..." [--project P001] [--epic E001] [--priority P1] [--status backlog] [--tags a,b]
//   node ai_studio/taskboard/cli.mjs set T0001 --status doing [--project P001] [--epic E001] [--priority P1] [--title "..."] [--log "evidence line"] [--json]
//   node ai_studio/taskboard/cli.mjs context [--json] [--tasks-limit 5]
//   node ai_studio/taskboard/cli.mjs validate [--json]
//   node ai_studio/taskboard/cli.mjs help
//
// Agents: prefer `new` over hand-writing files so IDs never collide.

import {
  agentEpicRow, agentProjectRow, agentTaskRow,
  findRoot, listTasks, listEpics, listProjects, findDoc, createTask, createEpic, createProject,
  sealTaskArchive, updateDoc, canonicalQualityAssignments, CURRENT_TASK_STATUSES,
  parseQualityAssignments, rankTaskEntries, READY_QUEUE_LIMIT, TASK_STATUSES,
} from "./store.mjs";
import {
  agentContextPayloadForStores,
  findTaskboardDoc,
  mutationStore,
  storeOptions,
  taskboardStoresForQuery,
  taskboardStoreSummary,
  validateTaskboardStoresDetailed,
} from "./stores.mjs";
import { relative } from "node:path";
import { isMain } from "../core_harness/tool_lib/cli.mjs";

const USAGE = `usage: cli.mjs <list|context|show|archive|new|set|validate|help> ...

Commands:
  list [--json] [--store studio|game:<id>] [--game <id>] [--include-private] [--status s] [--project P001] [--epic E001] [--tag t] [--ideas] [--all] [--archive]
  context [--json] [--store studio|game:<id>] [--game <id>] [--include-private] [--tasks-limit 5]
  show <P###|E###|T####|store:id> [--archive] [--json] [--store studio|game:<id>] [--game <id>] [--include-private]
  archive seal --name <immutable-batch-name> [--json] [--store studio|game:<id>] [--game <id>]
  new project --title "..." [--store studio|game:<id>] [--game <id>] [--kind ai-studio|game|template|tooling|research|other] [--target path] [--tags a,b]
  new epic --title "..." [--store studio|game:<id>] [--game <id>] [--project P001] [--status active] [--tags a,b]
  new task --title "..." [--store studio|game:<id>] [--game <id>] [--project P001] [--epic E001] [--priority P1] [--status backlog] [--tags a,b]
  set <id|store:id> [--store studio|game:<id>] [--game <id>] [--status s] [--project P001] [--epic E001] [--priority P1] [--title "..."] [--log "..."] [--waiver-reason "..."] [--closure-evidence "..."] [--quality "QCLR_001=pass; ..."] [--quality-evidence "..."] [--quality-not-applicable "reason"] [--json]
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

function storeQueryArgs(args) {
  return {
    store: typeof args.store === "string" ? args.store : "",
    game: typeof args.game === "string" ? args.game : "",
    includePrivate: args["include-private"] === true,
    includeArchive: args.archive === true,
  };
}

function shortRow(d, store = null) {
  const f = d.fields;
  const id = store && store.storeId !== "studio" ? `${store.storeId}:${f.id}` : f.id;
  const tags = (f.tags || []).length ? ` [${f.tags.join(",")}]` : "";
  const archive = d.archived ? " (archive)" : "";
  const project = d.kind === "project" ? (f.kind || "-") : (f.project || "-");
  const epic = d.kind === "task" ? (f.epic || "-") : "-";
  return `${id}  ${String(f.status).padEnd(7)} ${String(f.priority || "").padEnd(3)} ${String(project).padEnd(8)} ${String(epic).padEnd(5)} ${f.title}${tags}${archive}`;
}

function numberArg(value, fallback) {
  if (value === undefined || value === true || value === "") return fallback;
  const parsed = Number(value);
  return Number.isFinite(parsed) && parsed > 0 ? Math.floor(parsed) : fallback;
}

function textOption(value) {
  return typeof value === "string" && value.trim() ? value.trim() : "";
}

function suggestedQualityGroups(root, doc, patchFields = {}, options = {}) {
  const fields = { ...doc.fields, ...patchFields };
  const project = fields.project ? findDoc(root, fields.project, options) : null;
  const terms = [fields.target, fields.kind, ...(fields.tags || []), project?.fields.kind, project?.fields.target, ...(project?.fields.tags || [])]
    .join(" ").toLowerCase().split(/[^a-z0-9]+/).filter(Boolean);
  const has = (...values) => values.some((value) => terms.includes(value));
  const groups = [];
  const add = (...values) => values.forEach((value) => { if (!groups.includes(value)) groups.push(value); });
  if (has("art", "visual", "illustration")) add("QART", "QASSET");
  if (has("asset", "assets", "pipeline")) add("QASSET", "QTECH");
  if (has("gdd", "design", "gameplay")) add("QGDD", "QDES");
  if (has("ui", "hud", "player", "clarity")) add("QTECH", "QCLR");
  return groups.length ? groups : ["QTECH"];
}

function structuredQualityChecks(canonicalQuality, evidence) {
  const assignments = parseQualityAssignments(canonicalQuality);
  if (!assignments) return null;
  if (assignments.length === 1) {
    return assignments.map(({ ruleId: id, outcome }) => ({ id, outcome, evidence }));
  }
  const evidenceEntries = String(evidence).split(";").map((entry) => {
    const separator = entry.indexOf("=");
    return separator > 0 ? [entry.slice(0, separator).trim(), entry.slice(separator + 1).trim()] : ["", ""];
  });
  const evidenceById = new Map(evidenceEntries);
  if (evidenceEntries.length !== evidenceById.size || evidenceById.size !== assignments.length || assignments.some(({ ruleId }) => !evidenceById.get(ruleId))) return null;
  return assignments.map(({ ruleId: id, outcome }) => ({ id, outcome, evidence: evidenceById.get(id) }));
}

function statusCounts(tasks) {
  const counts = new Map();
  for (const task of tasks) {
    const key = task.fields.status || "unknown";
    counts.set(key, (counts.get(key) || 0) + 1);
  }
  return TASK_STATUSES.map((status) => `${status}:${counts.get(status) || 0}`).join(" ");
}

function taskEntriesForOptions(root, options) {
  return taskboardStoresForQuery(root, storeQueryArgs(options)).flatMap((store) =>
    listTasks(root, storeOptions(store)).map((task) => ({ task, store }))
  );
}

function rankedTaskEntries(entries, statuses) {
  return rankTaskEntries(entries, statuses);
}

function currentWorkTaskEntries(entries) {
  return rankedTaskEntries(entries, CURRENT_TASK_STATUSES);
}

function readyQueueTaskEntries(entries) {
  return rankedTaskEntries(entries, ["backlog"]);
}

function renderContext(root, options) {
  const tasksLimit = numberArg(options["tasks-limit"], 5);
  const taskEntries = taskEntriesForOptions(root, options);
  const allTasks = taskEntries.map(({ task }) => task);
  const currentEntries = currentWorkTaskEntries(taskEntries);
  const readyEntries = readyQueueTaskEntries(taskEntries);
  const shownCurrent = currentEntries.slice(0, tasksLimit);
  const readyLimit = Math.min(READY_QUEUE_LIMIT, Math.max(0, tasksLimit - shownCurrent.length));

  const lines = [];
  lines.push("# Current Context Digest");
  lines.push("");
  lines.push(`active_task_counts: ${statusCounts(allTasks)}`);
  lines.push("## Actionable Tasks");
  lines.push("");
  for (const { task, store } of shownCurrent) {
    lines.push(`- ${shortRow(task, store)}`);
  }
  if (currentEntries.length > tasksLimit) {
    lines.push(`- ... ${currentEntries.length - tasksLimit} more; show a specific task only if needed.`);
  }
  if (currentEntries.length === 0) {
    lines.push("- none");
  }
  lines.push("");
  lines.push("## Next Backlog Candidates");
  lines.push("");
  for (const { task, store } of readyEntries.slice(0, readyLimit)) lines.push(`- ${shortRow(task, store)}`);
  if (readyEntries.length === 0) lines.push("- none");
  else if (readyLimit === 0) lines.push("- omitted; context row limit reached");
  lines.push("");
  lines.push("Next context step: inspect only the linked task files or evidence paths needed for the current decision.");
  return `${lines.join("\n")}\n`;
}

export function main(argv, {
  root = findRoot(),
  writeStdout = (text) => process.stdout.write(text),
  writeStderr = (text) => process.stderr.write(text),
} = {}) {
  class CliExit {
    constructor(code) {
      this.code = code;
    }
  }
  const [cmd, ...rest] = argv;
  const args = parseArgs(rest);
  const writeJson = (value) => writeStdout(`${JSON.stringify(value, null, 2)}\n`);
  const writeLine = (value = "") => writeStdout(`${value}\n`);
  const printUsage = () => writeStdout(USAGE);
  const exit = (code) => { throw new CliExit(code); };
  const failCommand = (message) => {
    writeStderr(`error: ${message}\n`);
    exit(1);
  };
  const failProblem = (problemArgs, code, message, details = {}) => {
    if (problemArgs.json) {
      writeJson({ ok: false, problem: { code, message, details } });
      exit(1);
    }
    failCommand(message);
  };

  try {
    if (!cmd || cmd === "help" || cmd === "--help" || cmd === "-h") {
      printUsage();
      return 0;
    }

    switch (cmd) {
  case "list": {
    const hidden = new Set(args.all ? [] : ["idea", "done"]);
    if (args.ideas) hidden.delete("idea");
    if (args.archive) hidden.delete("done");
    if (args.review) hidden.delete("review");
    if (args.status) hidden.clear();
    const stores = taskboardStoresForQuery(root, storeQueryArgs(args));
    const storeDocs = stores.map((store) => {
      const opts = storeOptions(store);
      const epics = listEpics(root, opts);
      return {
        store,
        projects: listProjects(root, opts),
        epics,
        epicsById: new Map(epics.map((epic) => [epic.fields.id, epic])),
        tasks: listTasks(root, { ...opts, includeArchive: args.archive === true }),
      };
    });
    const projects = storeDocs.flatMap((entry) => entry.projects.map((project) => ({ ...entry, project })));
    const epics = storeDocs.flatMap((entry) => entry.epics.map((epic) => ({ ...entry, epic })));
    const tasks = storeDocs.flatMap((entry) => entry.tasks.map((task) => ({ ...entry, task }))).filter(({ task: t }) => {
      if (args.archive && t.archived !== true) return false;
      if (hidden.has(t.fields.status)) return false;
      if (args.status && t.fields.status !== args.status) return false;
      if (args.project && t.fields.project !== args.project) return false;
      if (args.epic && t.fields.epic !== args.epic) return false;
      if (args.tag && !(t.fields.tags || []).includes(args.tag)) return false;
      return true;
    });
    const order = new Map(TASK_STATUSES.map((s, i) => [s, i]));
    tasks.sort((a, b) =>
      (order.get(a.task.fields.status) ?? 99) - (order.get(b.task.fields.status) ?? 99) ||
      String(a.task.fields.priority).localeCompare(String(b.task.fields.priority)));
    if (args.json) {
      writeJson({
        schema: "ai_studio.taskboard.list.v1",
        stores: stores.map(taskboardStoreSummary),
        filters: {
          status: args.status || "",
          project: args.project || "",
          epic: args.epic || "",
          tag: args.tag || "",
          includeArchive: args.archive === true,
          includeAllStatuses: args.all === true,
        },
        tasks: tasks.map(({ task, store, epicsById }) => agentTaskRow(root, task, { store, epicsById })),
        projects: projects.map(({ project, store }) => agentProjectRow(root, project, { store })),
        epics: epics.map(({ epic, store }) => agentEpicRow(root, epic, { store })),
      });
      break;
    }
    for (const { project: p, store } of projects) {
      if (!args.status && !args.tag && !args.epic && (!args.project || args.project === p.fields.id)) {
        if (!args.all && p.fields.status === "done") continue;
        const id = store.storeId === "studio" ? p.fields.id : `${store.storeId}:${p.fields.id}`;
        writeLine(`# ${id} ${p.fields.title} (${p.fields.status}, ${p.fields.kind || "other"})`);
      }
    }
    for (const { epic: e, store } of epics) {
      if (!args.status && !args.tag && (!args.project || args.project === e.fields.project) && (!args.epic || args.epic === e.fields.id)) {
        if (!args.all && !args.archive && e.fields.status === "done") continue;
        const id = store.storeId === "studio" ? e.fields.id : `${store.storeId}:${e.fields.id}`;
        writeLine(`# ${id} ${e.fields.title} (${e.fields.status}, ${e.fields.project || "no-project"})`);
      }
    }
    for (const { task: t, store } of tasks) {
      writeLine(shortRow(t, store));
    }
    if (!tasks.length) {
      writeLine("(no tasks match)");
    }
    break;
  }
  case "context": {
    if (args.json) {
      const stores = taskboardStoresForQuery(root, storeQueryArgs(args));
      writeJson(agentContextPayloadForStores(root, stores, { limit: numberArg(args["tasks-limit"], 5) }));
      break;
    }
    writeStdout(renderContext(root, args));
    break;
  }
  case "show": {
    const id = args._[0] || failCommand("usage: show <id>");
    let resolved;
    try {
      resolved = findTaskboardDoc(root, id, storeQueryArgs(args));
    } catch (err) {
      failCommand(err.message);
    }
    const doc = resolved ? resolved.doc : null;
    const store = resolved ? resolved.store : null;
    if (!doc) failCommand(`no doc with id ${id}`);
    if (args.json) {
      let row;
      if (doc.kind === "task") {
        row = agentTaskRow(root, doc, { store, includeBody: true });
      } else if (doc.kind === "epic") {
        row = agentEpicRow(root, doc, { store, includeBody: true });
      } else {
        row = agentProjectRow(root, doc, { store, includeBody: true });
      }
      writeJson({
        schema: "ai_studio.taskboard.doc.v1",
        doc: row,
      });
      break;
    }
    writeLine(`file: ${relative(root, doc.file)}`);
    for (const [k, v] of Object.entries(doc.fields)) {
      writeLine(`${k}: ${Array.isArray(v) ? v.join(", ") : v}`);
    }
    writeLine("\n" + doc.body);
    break;
  }
  case "archive": {
    const action = args._[0];
    if (action !== "seal") failCommand("usage: archive seal --name <immutable-batch-name>");
    const name = textOption(args.name);
    if (!name) failCommand("--name is required");
    let store;
    try {
      store = mutationStore(root, storeQueryArgs(args));
    } catch (err) {
      failCommand(err.message);
    }
    let result;
    try {
      result = sealTaskArchive(root, { ...storeOptions(store), name });
    } catch (err) {
      failCommand(err.message);
    }
    const payload = {
      ok: true,
      storeId: store.storeId,
      file: relative(root, result.file).replaceAll("\\", "/"),
      entries: result.entries,
      bytes: result.bytes,
      sha256: result.sha256,
    };
    if (args.json) writeJson(payload);
    else writeLine(`sealed ${payload.entries} tasks: ${payload.file} (${payload.sha256})`);
    break;
  }
  case "new": {
    const kind = args._[0];
    if (!["project", "epic", "task"].includes(kind)) failCommand("usage: new project|epic|task --title \"...\"");
    if (!args.title) failCommand("--title is required");
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
    let store;
    try {
      store = mutationStore(root, storeQueryArgs(args));
    } catch (err) {
      failCommand(err.message);
    }
    const opts = storeOptions(store);
    let doc;
    try {
      doc = kind === "task" ? createTask(root, input, opts) : (kind === "epic" ? createEpic(root, input, opts) : createProject(root, input, opts));
    } catch (err) {
      if (args.json) {
        writeJson({ ok: false, problem: err.problem || { code: "taskboard_error", message: err.message } });
        exit(1);
      }
      failCommand(err.message);
    }
    if (args.json) {
      const row = kind === "task"
        ? agentTaskRow(root, doc, { store })
        : (kind === "epic" ? agentEpicRow(root, doc, { store }) : agentProjectRow(root, doc, { store }));
      writeJson({ ok: true, doc: row });
    } else {
      writeLine(`created ${doc.fields.id}: ${relative(root, doc.file)}`);
    }
    break;
  }
  case "set": {
    const id = args._[0] || failCommand("usage: set <id> --field value ...");
    const fields = {};
    for (const key of ["status", "project", "epic", "priority", "title", "target", "kind"]) {
      if (args[key] !== undefined) fields[key] = args[key];
    }
    if (args.tags !== undefined) {
      fields.tags = String(args.tags).split(",").map((s) => s.trim()).filter(Boolean);
    }
    const waiverReason = textOption(args["waiver-reason"]);
    const closureEvidence = textOption(args["closure-evidence"]);
    const closureInputPresent = args["waiver-reason"] !== undefined || args["closure-evidence"] !== undefined;
    if (closureInputPresent && (!waiverReason || !closureEvidence)) {
      failProblem(args, "closure_input_invalid", "--waiver-reason and --closure-evidence must be provided together");
    }
    const qualityInputPresent = args.quality !== undefined || args["quality-evidence"] !== undefined;
    const quality = textOption(args.quality);
    const qualityEvidence = textOption(args["quality-evidence"]);
    const qualityNotApplicablePresent = args["quality-not-applicable"] !== undefined;
    const qualityNotApplicable = textOption(args["quality-not-applicable"]);
    if (qualityInputPresent && qualityNotApplicablePresent) {
      failProblem(args, "quality_input_conflict", "--quality-not-applicable cannot be combined with --quality or --quality-evidence");
    }
    if (qualityInputPresent && (!quality || !qualityEvidence)) {
      failProblem(args, "quality_input_invalid", "--quality and --quality-evidence must be provided together");
    }
    const canonicalQuality = quality ? canonicalQualityAssignments(quality) : "";
    if (quality && !canonicalQuality) {
      let suggestedGroups = ["QTECH"];
      try {
        const resolved = findTaskboardDoc(root, id, storeQueryArgs(args));
        if (resolved) suggestedGroups = suggestedQualityGroups(root, resolved.doc, fields, storeOptions(resolved.store));
      } catch {
        // Keep the default suggestion; normal resolution reports the authoritative error later.
      }
      failProblem(
        args,
        "quality_input_invalid",
        "--quality must contain unique semicolon-separated Q...=pass|block|review|unverified assignments; use --quality-not-applicable with a reason instead of skip",
        { suggestedGroups },
      );
    }
    const qualityChecks = canonicalQuality ? structuredQualityChecks(canonicalQuality, qualityEvidence) : null;
    if (canonicalQuality && !qualityChecks) {
      failProblem(args, "quality_input_invalid", "multiple Quality checks require per-check evidence: QTECH_001=proof; QCLR_001=review", {});
    }
    if (qualityNotApplicablePresent && !qualityNotApplicable) {
      failProblem(args, "quality_input_invalid", "--quality-not-applicable requires a non-empty reason");
    }
    if (qualityChecks) fields.quality = { checks: qualityChecks };
    if (qualityNotApplicable) fields.quality = { notApplicable: { reason: qualityNotApplicable } };
    const patch = { fields };
    let resolvedForLog = null;
    const logEntries = [];
    if (textOption(args.log)) logEntries.push(textOption(args.log));
    if (waiverReason) logEntries.push(`Closure: waived; reason: ${waiverReason}; evidence: ${closureEvidence}`);
    if (canonicalQuality) logEntries.push(`Quality: ${canonicalQuality}; evidence: ${qualityEvidence}`);
    if (qualityNotApplicable) logEntries.push(`Quality: not-applicable; reason: ${qualityNotApplicable}`);
    if (logEntries.length) {
      try {
        resolvedForLog = findTaskboardDoc(root, id, storeQueryArgs(args));
      } catch (err) {
        failCommand(err.message);
      }
      const doc = resolvedForLog ? resolvedForLog.doc : null;
      if (!doc) failCommand(`no doc with id ${id}`);
      const stamp = new Date().toISOString().slice(0, 10);
      patch.body = `${doc.body.replace(/\s+$/, "")}\n${logEntries.map((entry) => `- ${stamp}: ${entry}`).join("\n")}\n`;
    }
    if (!Object.keys(fields).length && !patch.body) failCommand("nothing to set");
    let resolvedForUpdate = null;
    try {
      const resolved = resolvedForLog || findTaskboardDoc(root, id, storeQueryArgs(args));
      resolvedForUpdate = resolved;
      if (!resolved) failCommand(`no doc with id ${id}`);
      const doc = updateDoc(root, resolved.id, patch, storeOptions(resolved.store));
      if (args.json) {
        writeJson({ ok: true, doc: { id: doc.fields.id, storeId: resolved.store.storeId, qualifiedId: `${resolved.store.storeId}:${doc.fields.id}`, status: doc.fields.status, file: relative(root, doc.file) } });
      } else {
        writeLine(`updated ${id}: ${shortRow(doc, resolved.store)}`);
      }
    } catch (err) {
      if (err instanceof CliExit) throw err;
      if (err.problem?.code === "task_quality_decision_required" && resolvedForUpdate?.doc) {
        err.problem.details = {
          ...err.problem.details,
          suggestedGroups: suggestedQualityGroups(root, resolvedForUpdate.doc, fields, storeOptions(resolvedForUpdate.store)),
        };
      }
      if (args.json) {
        writeJson({ ok: false, problem: err.problem || { code: "taskboard_error", message: err.message } });
        exit(1);
      }
      failCommand(err.message);
    }
    break;
  }
  case "validate": {
    const stores = taskboardStoresForQuery(root, storeQueryArgs(args));
    const detailedProblems = validateTaskboardStoresDetailed(root, stores);
    if (args.json) {
      writeJson({ ok: detailedProblems.length === 0, problems: detailedProblems });
      return detailedProblems.length ? 1 : 0;
    }
    if (!detailedProblems.length) {
      writeLine("ok: no problems found");
    } else {
      for (const p of detailedProblems) {
        writeLine(`problem: ${p.message}`);
        const hint = remediationHint(p.message);
        if (hint) {
          writeLine(`hint: ${hint}`);
        }
      }
      return 1;
    }
    break;
  }
  default:
    printUsage();
    return 1;
    }
    return 0;
  } catch (error) {
    if (error instanceof CliExit) return error.code;
    throw error;
  }
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

if (isMain(import.meta.url)) {
  const exitCode = main(process.argv.slice(2));
  if (exitCode !== 0) process.exit(exitCode);
}
