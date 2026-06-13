// Taskboard core tests. Run: node --test tools/taskboard/test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, rmSync, writeFileSync, readFileSync, mkdirSync, utimesSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";
import vm from "node:vm";
import {
  parseDoc, serializeDoc, slugify, createTask, createEpic, listTasks,
  listEpics, updateDoc, findDoc, validateStore,
} from "./lib.mjs";

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
  assert.equal(slugify("Идея без латиницы"), "item");
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
  writeFileSync(join(root, "tasks", "STATUS.md"), "# Project Status\n");
  assert.equal(listTasks(root).length, 1);
  assert.deepEqual(validateStore(root), []);
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
  const cli = join(import.meta.dirname, "cli.mjs");
  const base = { cwd: root, encoding: "utf8" };
  const normal = spawnSync(process.execPath, [cli, "list"], base);
  assert.equal(normal.status, 0, normal.stderr);
  assert.match(normal.stdout, /Actionable/);
  assert.doesNotMatch(normal.stdout, /Raw idea/);
  const ideas = spawnSync(process.execPath, [cli, "list", "--ideas"], base);
  assert.equal(ideas.status, 0, ideas.stderr);
  assert.match(ideas.stdout, /Raw idea/);
});

test("cli list hides review by default and shows it explicitly", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Active work", status: "todo" });
  createTask(root, { title: "Old review", status: "review" });
  const cli = join(import.meta.dirname, "cli.mjs");
  const base = { cwd: root, encoding: "utf8" };
  const normal = spawnSync(process.execPath, [cli, "list"], base);
  assert.equal(normal.status, 0, normal.stderr);
  assert.match(normal.stdout, /Active work/);
  assert.doesNotMatch(normal.stdout, /Old review/);
  const review = spawnSync(process.execPath, [cli, "list", "--review"], base);
  assert.equal(review.status, 0, review.stderr);
  assert.match(review.stdout, /Old review/);
  const statusReview = spawnSync(process.execPath, [cli, "list", "--status", "review"], base);
  assert.equal(statusReview.status, 0, statusReview.stderr);
  assert.match(statusReview.stdout, /Old review/);
});

test("cli context caps status and prioritizes recent actionable tasks", (t) => {
  const root = tempRoot(t);
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(
    join(root, "tasks", "STATUS.md"),
    `# Project Status

## Current Goal

Ship the current project.

## Current Gate

Keep the context digest short.

## Required Validation

Run the narrow checks.

## Last Known Good Evidence

Large evidence list starts here.
${"evidence\n".repeat(300)}

## Blocking Work

- none

## Next Priorities

1. Continue with the next task.
`.replace(/\n/g, "\r\n"),
  );
  createTask(root, { title: "Old review", status: "review", priority: "P1" });
  createTask(root, { title: "Current todo", status: "todo", priority: "P1" });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(
    process.execPath,
    [cli, "context", "--status-max-chars", "1200", "--tasks-limit", "1"],
    { cwd: root, encoding: "utf8" },
  );
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /# Current Context Digest/);
  assert.match(result.stdout, /status_warning: large/);
  assert.match(result.stdout, /## Current Gate/);
  assert.match(result.stdout, /Keep the context digest short/);
  assert.match(result.stdout, /T0002 .* Current todo/);
  assert.doesNotMatch(result.stdout, /T0001 .* Old review/);
  assert.match(result.stdout, /1 review task\(s\) hidden/);
});

test("cli summary is short and avoids full task list noise", (t) => {
  const root = tempRoot(t);
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(
    join(root, "tasks", "STATUS.md"),
    `# Project Status

## Current Goal

Improve the game quickly.

## Blocking Work

- none

## Next Priorities

1. Build the next playable slice.
`.replace(/\n/g, "\r\n"),
  );
  createTask(root, { title: "Doing task", status: "doing", priority: "P1" });
  createTask(root, { title: "Review task", status: "review", priority: "P1" });
  createTask(root, { title: "Idea task", status: "idea", priority: "P1" });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(
    process.execPath,
    [cli, "summary", "--tasks-limit", "1"],
    { cwd: root, encoding: "utf8" },
  );
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /# Taskboard Summary/);
  assert.match(result.stdout, /active_task_counts: idea:1 backlog:0 todo:0 doing:1 review:1/);
  assert.match(result.stdout, /open_actionable_tasks: 1/);
  assert.match(result.stdout, /review_tasks: 1/);
  assert.match(result.stdout, /Improve the game quickly/);
  assert.match(result.stdout, /T0001 .* Doing task/);
  assert.doesNotMatch(result.stdout, /T0002 .* Review task/);
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

test("updateDoc with stale rev throws conflict", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Conflict me" });
  const doc = findDoc(root, "T0001");
  // Simulate an external edit by bumping the file mtime.
  const future = new Date(Date.now() + 5000);
  utimesSync(doc.file, future, future);
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "todo" }, rev: doc.rev }),
    (err) => err.conflict === true,
  );
  // Without rev (CLI-style last-write-wins) it still works.
  const updated = updateDoc(root, "T0001", { fields: { status: "todo" } });
  assert.equal(updated.fields.status, "todo");
});

test("validateStore reports missing epic refs and duplicate ids", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Orphan", epic: "E999" });
  const dupe = readFileSync(findDoc(root, "T0001").file, "utf8");
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(join(root, "tasks", "T9999-dupe.md"), dupe);
  const problems = validateStore(root);
  assert.ok(problems.some((p) => p.includes("missing epic")), problems.join("; "));
  assert.ok(problems.some((p) => p.includes("duplicate id")), problems.join("; "));
});

test("validateStore rejects placeholder actionable tasks but allows raw ideas", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Raw idea", status: "idea" });
  createTask(root, { title: "Empty backlog", status: "backlog" });
  const problems = validateStore(root);
  assert.ok(problems.some((p) => p.includes("T0002: actionable task needs")), problems.join("; "));

  updateDoc(root, "T0002", {
    body: `## What

Make the backlog item executable.

## Done when

- [ ] acceptance is checkable

## Open questions

## Log
`,
  });
  assert.deepEqual(validateStore(root), []);
});

test("validateStore rejects placeholder active epics but allows raw epic ideas", (t) => {
  const root = tempRoot(t);
  createEpic(root, { title: "Raw epic", status: "idea" });
  createEpic(root, { title: "Active epic", status: "active" });
  const problems = validateStore(root);
  assert.ok(problems.some((p) => p.includes("E002: active epic needs")), problems.join("; "));

  updateDoc(root, "E002", {
    body: `## Goal

Make the epic executable.

## In scope

- One focused workstream.

## Out of scope

- Unrelated work.

## Log
`,
  });
  assert.deepEqual(validateStore(root), []);
});

test("cli validate prints remediation hints for common failures", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Empty backlog", status: "backlog" });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "validate"], { cwd: root, encoding: "utf8" });
  assert.notEqual(result.status, 0);
  assert.match(result.stdout, /problem: T0001: actionable task needs/);
  assert.match(result.stdout, /hint: fill `## What` and at least one checkable `## Done when`/);
});

test("markdown preview renders task syntax and escapes html", () => {
  const sandbox = {};
  sandbox.globalThis = sandbox;
  const source = readFileSync(join(import.meta.dirname, "public", "markdown_preview.js"), "utf8");
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
