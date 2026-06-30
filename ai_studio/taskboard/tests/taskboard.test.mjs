// Taskboard core tests. Run: node --test ai_studio/taskboard/tests/taskboard.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, rmSync, writeFileSync, readFileSync, mkdirSync, utimesSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";
import vm from "node:vm";
import {
  parseDoc, serializeDoc, slugify, createTask, createEpic, listTasks,
  listEpics, updateDoc, findDoc, validateStore, validateStoreDetailed,
} from "../lib.mjs";
import { boardPayload } from "../api.mjs";

const taskboardDir = dirname(import.meta.dirname);
const cliPath = join(taskboardDir, "cli.mjs");

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "taskboard-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

test("frontmatter roundtrip preserves fields and body", () => {
  const fields = {
    id: "T0001",
    title: 'Tricky: title with #hash and "quotes"',
    status: "backlog",
    tags: ["a-b", "c d"],
    created: "2026-06-11",
  };
  const body = "## What\n\nLine one.\n\n- [ ] box\n";
  const parsed = parseDoc(serializeDoc(fields, body));
  assert.deepEqual(parsed.fields, fields);
  assert.equal(parsed.body.trim(), body.trim());
});

test("parseDoc tolerates files without frontmatter", () => {
  const parsed = parseDoc("just text\n");
  assert.deepEqual(parsed.fields, {});
  assert.equal(parsed.body, "just text\n");
});

test("slugify handles non-ascii and empty titles", () => {
  assert.equal(slugify("Camp Rest Action!"), "camp-rest-action");
  assert.equal(slugify("\u0418\u0434\u0435\u044f \u0431\u0435\u0437 \u043b\u0430\u0442\u0438\u043d\u0438\u0446\u044b"), "item");
});

test("createTask allocates sequential ids and createEpic separate sequence", (t) => {
  const root = tempRoot(t);
  const t1 = createTask(root, { title: "First" });
  const t2 = createTask(root, { title: "Second" });
  const e1 = createEpic(root, { title: "Epic" });
  assert.equal(t1.fields.id, "T0001");
  assert.equal(t2.fields.id, "T0002");
  assert.equal(e1.fields.id, "E001");
  assert.equal(listTasks(root).length, 2);
  assert.equal(listEpics(root).length, 1);
});

test("task store ignores operational docs", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "First" });
  writeFileSync(join(root, "tasks", "README.md"), "# Task Store\n");
  assert.equal(listTasks(root).length, 1);
  assert.deepEqual(validateStore(root), []);
});

test("Taskboard API board payload exposes public task and epic state", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Visible task", status: "todo" });
  createEpic(root, { title: "Visible epic", status: "active" });

  const payload = boardPayload(root);
  assert.equal(payload.root, root);
  assert.deepEqual(payload.tasks.map((doc) => doc.fields.id), ["T0001"]);
  assert.deepEqual(payload.epics.map((doc) => doc.fields.id), ["E001"]);
  assert.ok(payload.taskStatuses.includes("doing"));
  assert.ok(payload.epicStatuses.includes("active"));
  assert.ok(payload.priorities.includes("P1"));
  assert.equal(payload.tasks[0].file, undefined);
});

test("done tasks move to archive and stay addressable by id", (t) => {
  const root = tempRoot(t);
  createEpic(root, { title: "Epic" });
  createTask(root, { title: "Archive me", epic: "E001", status: "todo" });
  const updated = updateDoc(root, "T0001", { fields: { status: "done" } });
  assert.match(updated.file, /tasks[\\/]+archive[\\/]+E001[\\/]+T0001-/);
  assert.equal(existsSync(updated.file), true);
  assert.equal(listTasks(root).length, 0);
  assert.equal(listTasks(root, { includeArchive: true }).length, 1);
  assert.equal(findDoc(root, "T0001").fields.status, "done");
});

test("cli list hides ideas by default and shows them explicitly", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Raw idea", status: "idea" });
  createTask(root, { title: "Actionable", status: "backlog" });
  const cli = cliPath;
  const base = { cwd: root, encoding: "utf8" };
  const normal = spawnSync(process.execPath, [cli, "list"], base);
  assert.equal(normal.status, 0, normal.stderr);
  assert.match(normal.stdout, /Actionable/);
  assert.doesNotMatch(normal.stdout, /Raw idea/);
  const ideas = spawnSync(process.execPath, [cli, "list", "--ideas"], base);
  assert.equal(ideas.status, 0, ideas.stderr);
  assert.match(ideas.stdout, /Raw idea/);
});

test("cli list shows review by default and keeps ideas hidden", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Active work", status: "todo" });
  createTask(root, { title: "Needs review", status: "review" });
  createTask(root, { title: "Raw idea", status: "idea" });
  const cli = cliPath;
  const base = { cwd: root, encoding: "utf8" };
  const normal = spawnSync(process.execPath, [cli, "list"], base);
  assert.equal(normal.status, 0, normal.stderr);
  assert.match(normal.stdout, /Active work/);
  assert.match(normal.stdout, /Needs review/);
  assert.doesNotMatch(normal.stdout, /Raw idea/);
  const ideas = spawnSync(process.execPath, [cli, "list", "--ideas"], base);
  assert.equal(ideas.status, 0, ideas.stderr);
  assert.match(ideas.stdout, /Raw idea/);
});

test("cli context is task-derived and includes review without task bodies", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Large active task",
    status: "doing",
    priority: "P0",
    body: `## What

${"LARGE_TASK_BODY_SHOULD_NOT_APPEAR\n".repeat(300)}

## Done when

- [ ] compact context stays row-only

## Open questions

## Log
`,
  });
  createTask(root, { title: "Review task", status: "review", priority: "P1" });
  createTask(root, { title: "Raw idea", status: "idea", priority: "P1" });
  const cli = cliPath;
  const result = spawnSync(process.execPath, [cli, "context"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /# Current Context Digest/);
  assert.match(result.stdout, /active_task_counts: idea:1 backlog:0 todo:0 doing:1 review:1/);
  assert.match(result.stdout, /T0001 .* Large active task/);
  assert.match(result.stdout, /T0002 .* Review task/);
  assert.doesNotMatch(result.stdout, /T0003 .* Raw idea/);
  assert.doesNotMatch(result.stdout, /LARGE_TASK_BODY_SHOULD_NOT_APPEAR/);
  assert.match(result.stdout, /inspect only the linked task files/);
});

test("cli summary is task-derived and shows review as current work", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Doing task", status: "doing", priority: "P1" });
  createTask(root, { title: "Review task", status: "review", priority: "P1" });
  createTask(root, { title: "Idea task", status: "idea", priority: "P1" });
  const cli = cliPath;
  const result = spawnSync(process.execPath, [cli, "summary", "--tasks-limit", "5"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /# Taskboard Summary/);
  assert.match(result.stdout, /active_task_counts: idea:1 backlog:0 todo:0 doing:1 review:1/);
  assert.match(result.stdout, /open_work_items: 2/);
  assert.match(result.stdout, /review_tasks: 1/);
  assert.match(result.stdout, /T0001 .* Doing task/);
  assert.match(result.stdout, /T0002 .* Review task/);
  assert.doesNotMatch(result.stdout, /T0003 .* Idea task/);
  assert.doesNotMatch(result.stdout, /## Current Goal/);
});

test("cli summary json is a compact agent API payload", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Implement thing",
    status: "doing",
    priority: "P0",
    body: `## What

BODY_SHOULD_NOT_APPEAR

## Done when

- [ ] done

## Log
`,
  });
  createTask(root, { title: "Needs review", status: "review", priority: "P1" });
  createTask(root, { title: "Raw idea", status: "idea", priority: "P2" });

  const result = spawnSync(process.execPath, [cliPath, "summary", "--json"], { cwd: root, encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr);
  const payload = JSON.parse(result.stdout);
  assert.equal(payload.schema, "ai_studio.taskboard.agent_context.v1");
  assert.equal(payload.counts.tasks.doing, 1);
  assert.equal(payload.counts.tasks.review, 1);
  assert.equal(payload.counts.tasks.idea, 1);
  assert.deepEqual(payload.currentWork.map((task) => task.id), ["T0001", "T0002"]);
  assert.equal(payload.currentWork[0].file.includes("tasks/active/T0001-"), true);
  assert.equal("body" in payload.currentWork[0], false);
  assert.doesNotMatch(result.stdout, /BODY_SHOULD_NOT_APPEAR/);
});

test("cli list and show json expose stable agent rows", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Visible task",
    status: "todo",
    priority: "P1",
    tags: ["agent-api"],
    body: `## What

Detailed body.

## Done when

- [ ] done

## Log
`,
  });
  createTask(root, { title: "Hidden idea", status: "idea" });

  const list = spawnSync(process.execPath, [cliPath, "list", "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(list.status, 0, list.stderr);
  const listPayload = JSON.parse(list.stdout);
  assert.equal(listPayload.schema, "ai_studio.taskboard.list.v1");
  assert.deepEqual(listPayload.tasks.map((task) => task.id), ["T0001"]);
  assert.equal("body" in listPayload.tasks[0], false);

  const show = spawnSync(process.execPath, [cliPath, "show", "T0001", "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(show.status, 0, show.stderr);
  const showPayload = JSON.parse(show.stdout);
  assert.equal(showPayload.schema, "ai_studio.taskboard.doc.v1");
  assert.equal(showPayload.doc.id, "T0001");
  assert.match(showPayload.doc.body, /Detailed body/);
});

test("taskboard cli rejects unrelated core commands", () => {
  const result = spawnSync(process.execPath, [cliPath, "workflow-run"], { encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.match(result.stdout, /usage: cli\.mjs <list\|context\|show\|new\|set\|validate>/);
  assert.doesNotMatch(result.stdout, /workflow-run/);
});

test("updateDoc patches fields, keeps id/created, bumps updated", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Patch me", status: "idea" });
  const doc = updateDoc(root, "T0001", { fields: { status: "doing", id: "HACK", created: "1999-01-01" } });
  assert.equal(doc.fields.status, "doing");
  assert.equal(doc.fields.id, "T0001");
  assert.notEqual(doc.fields.created, "1999-01-01");
});

test("updateDoc rejects invalid status", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Bad status" });
  assert.throws(() => updateDoc(root, "T0001", { fields: { status: "nonsense" } }), /Invalid status/);
});

test("updateDoc checks archive move conflicts before rewriting source", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Conflict move",
    status: "doing",
    epic: "E001",
    body: "## What\n\nKeep source stable.\n\n## Done when\n\n- [ ] source unchanged on move conflict\n",
  });
  const original = readFileSync(task.file, "utf8");
  const archiveDir = join(root, "tasks", "archive", "E001");
  mkdirSync(archiveDir, { recursive: true });
  writeFileSync(join(archiveDir, "T0001-conflict-move.md"), "existing archive task\n");

  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    /target already exists/,
  );
  assert.equal(readFileSync(task.file, "utf8"), original);
});

test("markdown preview renders task syntax and escapes html", () => {
  const sandbox = {};
  sandbox.globalThis = sandbox;
  const source = readFileSync(join(taskboardDir, "public", "markdown_preview.js"), "utf8");
  vm.runInNewContext(source, sandbox);
  const html = sandbox.TaskboardMarkdown.renderMarkdown(`# Title

- [x] **done** \`code\`
- item

\`\`\`
<tag>
\`\`\`

<script>alert(1)</script>`);
  assert.match(html, /<h1>Title<\/h1>/);
  assert.match(html, /<input type="checkbox" disabled checked>/);
  assert.match(html, /<strong>done<\/strong>/);
  assert.match(html, /<code>code<\/code>/);
  assert.match(html, /&lt;tag&gt;/);
  assert.match(html, /&lt;script&gt;alert\(1\)&lt;\/script&gt;/);
});
