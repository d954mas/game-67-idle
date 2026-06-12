// Taskboard core tests. Run: node --test tools/taskboard/test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, rmSync, writeFileSync, readFileSync, mkdirSync, utimesSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";
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
