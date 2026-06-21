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
  listEpics, updateDoc, findDoc, validateStore, validateStoreDetailed,
  LIVE_STATUS_MAX_CHARS, orchestrationPacketTemplate,
} from "./lib.mjs";

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "taskboard-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function taskBodyWithLog(log) {
  return `## What

Make taskboard pipeline work checkable.

## Done when

- [ ] evidence is mechanically validated

## Open questions

## Log

${log}
`;
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

test("cli context default status cap follows live status budget", (t) => {
  const root = tempRoot(t);
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(
    join(root, "tasks", "STATUS.md"),
    `# Project Status

## Current Goal

${"goal line\n".repeat(400)}
THIS_SHOULD_BE_BEYOND_DEFAULT_CONTEXT
`,
  );
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "context"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, new RegExp(`status_warning: large; digest is capped at ${LIVE_STATUS_MAX_CHARS} chars`));
  assert.match(result.stdout, /truncated/);
  assert.doesNotMatch(result.stdout, /THIS_SHOULD_BE_BEYOND_DEFAULT_CONTEXT/);
});

test("cli context lists active tasks without embedding large task bodies", (t) => {
  const root = tempRoot(t);
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(
    join(root, "tasks", "STATUS.md"),
    `# Project Status

## Current Goal

Keep context compact.
`,
  );
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
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "context"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /T0001 .* Large active task/);
  assert.doesNotMatch(result.stdout, /LARGE_TASK_BODY_SHOULD_NOT_APPEAR/);
  assert.match(result.stdout, /inspect only the linked task files/);
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

test("updateDoc rejects substantial pipeline transition without orchestration evidence", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Pipeline guard",
    status: "doing",
    body: taskBodyWithLog("- 2026-06-21: Implemented the validator."),
  });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    /substantial pipeline\/orchestration task needs orchestration evidence/,
  );
});

test("updateDoc accepts substantial pipeline transition with orchestration packet and reviewer", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Pipeline guard",
    status: "doing",
    body: taskBodyWithLog(`- orchestration: used
  objective: verify the transition guard behavior
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs
  expected output: focused failing/passing taskboard tests
  evidence command: node --test tools/taskboard/test.mjs
  stop condition: requested guard scenarios pass
  independent reviewer: reviewed transition and validateStore cases`),
  });
  const updated = updateDoc(root, "T0001", { fields: { status: "review" } });
  assert.equal(updated.fields.status, "review");
});

test("updateDoc rejects orchestration packet labels without meaningful values", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Pipeline guard",
    status: "doing",
    body: taskBodyWithLog(`- orchestration: used
  objective:
  allowed files: TBD
  expected output: none
  evidence command: ...
  stop condition: unknown
  independent reviewer:`),
  });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "review" } }),
    /substantial pipeline\/orchestration task needs orchestration evidence/,
  );
});

test("updateDoc reports missing orchestration packet fields", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Pipeline guard",
    status: "doing",
    body: taskBodyWithLog(`- orchestration: used
  objective: improve orchestration diagnostics
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs
  expected output: focused failure details
  evidence command: node --test tools/taskboard/test.mjs
  stop condition: taskboard tests pass
  independent reviewers: reviewed by a plural label`),
  });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "review" } }),
    /missing\/invalid: independent reviewer/,
  );
});

test("updateDoc reports missing orchestration packet when no block exists", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Subagent pipeline guard",
    status: "doing",
    body: taskBodyWithLog("- 2026-06-21: Closed without packet."),
  });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "review" } }),
    /missing\/invalid: orchestration: used packet/,
  );
});

test("updateDoc rejects orchestration packet assembled from separate log entries", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Pipeline guard",
    status: "doing",
    body: taskBodyWithLog(`- 2026-06-21: objective: old planning note
  allowed files: tools/taskboard/lib.mjs
  expected output: old output note
  evidence command: old command
  stop condition: old stop
  independent reviewer: old reviewer
- 2026-06-21: orchestration: used`),
  });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "review" } }),
    /substantial pipeline\/orchestration task needs orchestration evidence/,
  );
});

test("updateDoc accepts substantial task with small-scope orchestration exception", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Taskboard validator wording",
    status: "doing",
    body: taskBodyWithLog("- orchestration: not needed - small scope: one-file guard wording tweak"),
  });
  const updated = updateDoc(root, "T0001", { fields: { status: "done" } });
  assert.equal(updated.fields.status, "done");
});

test("updateDoc rejects empty or weak small-scope orchestration exceptions", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Pipeline guard empty exception",
    status: "doing",
    body: taskBodyWithLog("- orchestration: not needed - small scope: "),
  });
  createTask(root, {
    title: "Pipeline guard weak exception",
    status: "doing",
    body: taskBodyWithLog("- orchestration: not needed - small scope: no runtime change"),
  });
  createTask(root, {
    title: "Pipeline guard spoofed exception",
    status: "doing",
    body: taskBodyWithLog("- orchestration: not needed - small scope: not one-file; touched many files"),
  });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "review" } }),
    /substantial pipeline\/orchestration task needs orchestration evidence/,
  );
  assert.throws(
    () => updateDoc(root, "T0002", { fields: { status: "review" } }),
    /substantial pipeline\/orchestration task needs orchestration evidence/,
  );
  assert.throws(
    () => updateDoc(root, "T0003", { fields: { status: "review" } }),
    /substantial pipeline\/orchestration task needs orchestration evidence/,
  );
});

test("updateDoc does not classify gameplay skills tasks as pipeline orchestration", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Add player skills menu",
    status: "doing",
    body: `## What

Add a gameplay menu for hero skill choices.

## Done when

- [ ] the menu can move to review

## Open questions

## Log

- 2026-06-21: Implemented gameplay menu validation.
`,
  });
  const updated = updateDoc(root, "T0001", { fields: { status: "review" } });
  assert.equal(updated.fields.status, "review");
});

test("validateStore reports active done task missing orchestration evidence", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Subagent guard",
    status: "done",
    body: taskBodyWithLog("- 2026-06-21: Closed without review."),
  });
  const problems = validateStore(root);
  assert.ok(
    problems.some((p) => p.includes("T0001: substantial pipeline/orchestration task needs orchestration evidence")),
    problems.join("; "),
  );
});

test("validateStore reports archived done task missing orchestration evidence", (t) => {
  const root = tempRoot(t);
  const archiveDir = join(root, "tasks", "archive", "E001");
  mkdirSync(archiveDir, { recursive: true });
  writeFileSync(
    join(archiveDir, "T0028-pipeline-guard.md"),
    serializeDoc(
      {
        id: "T0028",
        title: "Pipeline guard",
        status: "done",
        epic: "E001",
        priority: "P0",
        tags: ["pipeline"],
        created: "2026-06-21",
        updated: "2026-06-21",
      },
      taskBodyWithLog("- 2026-06-21: Closed without orchestration evidence."),
    ),
  );
  const problems = validateStore(root);
  assert.ok(
    problems.some((p) => p.includes("T0028: substantial pipeline/orchestration task needs orchestration evidence")),
    problems.join("; "),
  );
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

test("validateStore reports active review task missing orchestration evidence", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Taskboard pipeline guard",
    status: "review",
    body: taskBodyWithLog("- 2026-06-21: Ready for review."),
  });
  const problems = validateStore(root);
  assert.ok(
    problems.some((p) => p.includes("T0001: substantial pipeline/orchestration task needs orchestration evidence")),
    problems.join("; "),
  );
});

test("validateStoreDetailed reports structured orchestration problem", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Taskboard pipeline guard",
    status: "review",
    body: taskBodyWithLog(`- orchestration: used
  objective: improve orchestration diagnostics
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs
  expected output: focused failure details
  evidence command: node --test tools/taskboard/test.mjs
  stop condition: taskboard tests pass
  independent reviewers: reviewed by a plural label`),
  });
  const problems = validateStoreDetailed(root);
  assert.equal(problems.length, 1);
  assert.equal(problems[0].code, "orchestration_evidence_missing");
  assert.equal(problems[0].taskId, "T0001");
  assert.deepEqual(problems[0].missingFields, ["independent reviewer"]);
  assert.match(problems[0].template, /independent reviewer: <non-empty>/);
});

test("orchestration template can be filled and accepted", (t) => {
  const root = tempRoot(t);
  const packet = orchestrationPacketTemplate()
    .replace("objective: <non-empty>", "objective: verify packet template")
    .replace("allowed files: <non-empty>", "allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs")
    .replace("expected output: <non-empty>", "expected output: accepted review transition")
    .replace("evidence command: <non-empty>", "evidence command: node --test tools/taskboard/test.mjs")
    .replace("stop condition: <non-empty>", "stop condition: focused taskboard tests pass")
    .replace("independent reviewer: <non-empty>", "independent reviewer: reviewed JSON output and template drift");
  createTask(root, {
    title: "Taskboard pipeline guard",
    status: "doing",
    body: taskBodyWithLog(packet),
  });
  const updated = updateDoc(root, "T0001", { fields: { status: "review" } });
  assert.equal(updated.fields.status, "review");
});

test("validateStore rejects oversized live status", (t) => {
  const root = tempRoot(t);
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(join(root, "tasks", "STATUS.md"), `# Project Status\n\n${"closed prototype evidence\n".repeat(400)}`);
  const problems = validateStore(root);
  assert.ok(problems.some((p) => p.includes("exceeds live status budget")), problems.join("; "));
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
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(join(root, "tasks", "STATUS.md"), `# Project Status\n\n${"old evidence\n".repeat(700)}`);
  createTask(root, { title: "Empty backlog", status: "backlog" });
  createTask(root, {
    title: "Pipeline review without orchestration",
    status: "review",
    body: taskBodyWithLog("- 2026-06-21: Ready for review."),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "validate"], { cwd: root, encoding: "utf8" });
  assert.notEqual(result.status, 0);
  assert.match(result.stdout, /problem: T0001: actionable task needs/);
  assert.match(result.stdout, /hint: fill `## What` and at least one checkable `## Done when`/);
  assert.match(result.stdout, /problem: T0002: substantial pipeline\/orchestration task needs orchestration evidence/);
  assert.match(result.stdout, /missing\/invalid: orchestration: used packet/);
  assert.match(result.stdout, /hint: add a complete packet from `node tools\/taskboard\/cli\.mjs orchestration-template`:/);
  assert.match(result.stdout, /objective: <non-empty>/);
  assert.match(result.stdout, /independent reviewer: <non-empty>/);
  assert.match(result.stdout, /problem: tasks\/STATUS\.md exceeds live status budget/);
  assert.match(result.stdout, /hint: replace inline history with pointers/);
});

test("cli validate --json reports parseable orchestration fields", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Pipeline review without orchestration",
    status: "review",
    body: taskBodyWithLog("- 2026-06-21: Ready for review."),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "validate", "--json"], { cwd: root, encoding: "utf8" });
  assert.notEqual(result.status, 0);
  assert.doesNotMatch(result.stdout, /^problem:/m);
  assert.doesNotMatch(result.stdout, /^hint:/m);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.equal(parsed.problems[0].code, "orchestration_evidence_missing");
  assert.equal(parsed.problems[0].taskId, "T0001");
  assert.deepEqual(parsed.problems[0].missingFields, ["orchestration: used packet"]);
  assert.match(parsed.problems[0].template, /objective: <non-empty>/);
});

test("cli set --json reports structured transition failure", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Subagent pipeline guard",
    status: "doing",
    body: taskBodyWithLog("- 2026-06-21: Ready for review without packet."),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "set", "T0001", "--status", "review", "--json"], { cwd: root, encoding: "utf8" });
  assert.notEqual(result.status, 0);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.equal(parsed.problem.code, "orchestration_evidence_missing");
  assert.equal(parsed.problem.taskId, "T0001");
  assert.deepEqual(parsed.problem.missingFields, ["orchestration: used packet"]);
});

test("cli orchestration-template prints accepted packet shape", () => {
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-template"], { encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /^- orchestration: used/m);
  assert.match(result.stdout, /objective: <non-empty>/);
  assert.match(result.stdout, /allowed files: <non-empty>/);
  assert.match(result.stdout, /expected output: <non-empty>/);
  assert.match(result.stdout, /evidence command: <non-empty>/);
  assert.match(result.stdout, /stop condition: <non-empty>/);
  assert.match(result.stdout, /independent reviewer: <non-empty>/);
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
