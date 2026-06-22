// Taskboard core tests. Run: node --test tools/taskboard/test.mjs
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
  LIVE_STATUS_MAX_CHARS, orchestrationPacketTemplate, orchestrationPreflightProblem,
  subagentPacketTemplate, subagentPacketProblem,
  subagentPacketPreset, subagentPacketPresetNames, renderSubagentPacketPreset,
  DEFAULT_ORCHESTRATION_TOOL_USE_GUARD,
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

function writeTaskDoc(root, fields, body) {
  const dir = join(root, "tasks", "active");
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, `${fields.id}-${slugify(fields.title || "task")}.md`),
    serializeDoc({
      status: "todo",
      priority: "P1",
      tags: [],
      created: "2026-06-21",
      updated: "2026-06-21",
      ...fields,
    }, body),
  );
}

function validSubagentPacket() {
  return `objective: Verify the reusable packet shape.
allowed files: tools/taskboard/lib.mjs; tools/taskboard/test.mjs
forbidden files: AGENTS.md; src/clean_seed_main.c
tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}
expected output: PASS or CONCERNS with exact evidence.
evidence command or artifact: node --test --test-name-pattern "subagent packet" tools/taskboard/test.mjs
stop condition: focused packet checks pass or a blocker is found.
handoff:
  findings: exact findings and verdict
  files: files inspected
  commands/evidence: commands run and results
  risks: residual risks
  owner action: lead next action
  not-done: explicit gaps`;
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
  assert.match(result.stdout, /## Current Work/);
  assert.ok(result.stdout.indexOf("## Current Work") < result.stdout.indexOf("## Current Goal"));
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

test("cli context omits live game sections for pipeline current work", (t) => {
  const root = tempRoot(t);
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(
    join(root, "tasks", "STATUS.md"),
    `# Project Status

## Current Goal

GAME_GOAL_SHOULD_NOT_APPEAR

## Next Priorities

1. GAME_PRIORITY_SHOULD_NOT_APPEAR
`,
  );
  createTask(root, {
    title: "Pipeline current task",
    status: "doing",
    priority: "P1",
    tags: ["pipeline", "orchestration"],
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "context"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /## Current Work/);
  assert.match(result.stdout, /T0001 .* Pipeline current task/);
  assert.match(result.stdout, /## Status Context/);
  assert.match(result.stdout, /pipeline\/tooling-scoped/);
  assert.doesNotMatch(result.stdout, /GAME_GOAL_SHOULD_NOT_APPEAR/);
  assert.doesNotMatch(result.stdout, /GAME_PRIORITY_SHOULD_NOT_APPEAR/);
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
  assert.match(result.stdout, /## Current Work/);
  assert.ok(result.stdout.indexOf("## Current Work") < result.stdout.indexOf("## Current Goal"));
  assert.match(result.stdout, /Improve the game quickly/);
  assert.match(result.stdout, /T0001 .* Doing task/);
  assert.doesNotMatch(result.stdout, /T0002 .* Review task/);
});

test("cli summary omits live game sections for pipeline current work", (t) => {
  const root = tempRoot(t);
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(
    join(root, "tasks", "STATUS.md"),
    `# Project Status

## Current Goal

GAME_GOAL_SHOULD_NOT_APPEAR

## Blocking Work

GAME_BLOCKER_SHOULD_NOT_APPEAR

## Next Priorities

1. GAME_PRIORITY_SHOULD_NOT_APPEAR
`,
  );
  createTask(root, {
    title: "Pipeline current task",
    status: "doing",
    priority: "P1",
    tags: ["pipeline", "taskboard"],
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "summary"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /## Current Work/);
  assert.match(result.stdout, /T0001 .* Pipeline current task/);
  assert.match(result.stdout, /## Status Context/);
  assert.match(result.stdout, /pipeline\/tooling-scoped/);
  assert.doesNotMatch(result.stdout, /GAME_GOAL_SHOULD_NOT_APPEAR/);
  assert.doesNotMatch(result.stdout, /GAME_BLOCKER_SHOULD_NOT_APPEAR/);
  assert.doesNotMatch(result.stdout, /GAME_PRIORITY_SHOULD_NOT_APPEAR/);
});

test("cli summary and context keep game sections when only task body mentions pipeline", (t) => {
  const root = tempRoot(t);
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(
    join(root, "tasks", "STATUS.md"),
    `# Project Status

## Current Goal

GAME_GOAL_SHOULD_APPEAR

## Next Priorities

1. GAME_PRIORITY_SHOULD_APPEAR
`,
  );
  createTask(root, {
    title: "Gameplay validation task",
    status: "doing",
    priority: "P1",
    tags: ["prototype", "dragon-grove"],
    body: `## What

Run node tools/ai.mjs validate --review after the gameplay smoke.

## Done when

- [ ] game check passes

## Open questions

## Log
`,
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const summary = spawnSync(process.execPath, [cli, "summary"], { cwd: root, encoding: "utf8" });
  assert.equal(summary.status, 0, summary.stderr);
  assert.match(summary.stdout, /## Current Goal/);
  assert.match(summary.stdout, /GAME_GOAL_SHOULD_APPEAR/);
  assert.match(summary.stdout, /GAME_PRIORITY_SHOULD_APPEAR/);
  assert.doesNotMatch(summary.stdout, /pipeline\/tooling-scoped/);

  const context = spawnSync(process.execPath, [cli, "context"], { cwd: root, encoding: "utf8" });
  assert.equal(context.status, 0, context.stderr);
  assert.match(context.stdout, /## Current Goal/);
  assert.match(context.stdout, /GAME_GOAL_SHOULD_APPEAR/);
  assert.doesNotMatch(context.stdout, /pipeline\/tooling-scoped/);
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

test("validateStore does not force-gate a plain gameplay task (keyword-only classifier)", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0079",
    title: "Gameplay runtime playtest pass",
    status: "doing",
    tags: ["gameplay"],
  }, `## What

Broad gameplay/runtime playtest work; coupled single-agent slice.

## Done when

- [ ] the playtest pass can move to review

## Open questions

## Log

- 2026-06-21: Started the coupled gameplay slice; no delegation packet needed.
`);

  // Plain game/visual/asset slices are coupled single-agent work — not gated.
  assert.deepEqual(validateStoreDetailed(root), []);
});

test("validateStore preserves pre-T0079 broad work compatibility", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0078",
    title: "Gameplay runtime playtest pass",
    status: "doing",
    tags: ["gameplay"],
  }, `## What

Exercise a gameplay runtime check.

## Done when

- [ ] the check can remain in progress

## Open questions

## Log

- 2026-06-21: Legacy broad gameplay/runtime work started before broad classification.
`);

  assert.deepEqual(validateStoreDetailed(root), []);
});

test("validateStore accepts T0079 broad work with small-scope exception", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0079",
    title: "Visual review wording tweak",
    status: "review",
    tags: ["visual", "review"],
  }, taskBodyWithLog("- orchestration: not needed - small scope: docs-only review wording tweak"));

  assert.deepEqual(validateStoreDetailed(root), []);
});

test("validateStore does not classify T0079 small gameplay task without broad scope cue", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0079",
    title: "Add player skills menu",
    status: "doing",
    tags: ["gameplay"],
  }, `## What

Add a gameplay menu for hero skill choices.

## Done when

- [ ] the menu can move to review

## Open questions

## Log

- 2026-06-21: Implemented gameplay menu validation.
`);

  assert.deepEqual(validateStoreDetailed(root), []);
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

test("validateStore rejects unbounded allowed files for new orchestration tasks", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0076",
    title: "Pipeline allowed files guard",
    status: "review",
    tags: ["pipeline", "orchestration"],
  }, taskBodyWithLog(`- orchestration: used
  objective: verify bounded allowed files
  allowed files: tools/**
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: focused guard tests
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent
  stop condition: tests pass
  independent reviewer: reviewed guard scope
- evidence: PASS \`node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent\``));

  const problems = validateStoreDetailed(root);
  assert.equal(problems.length, 1);
  assert.deepEqual(problems[0].missingFields, ["allowed files bounds"]);
});

test("validateStore keeps pre-T0076 allowed files compatibility", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0032",
    title: "Pipeline legacy orchestration guard",
    status: "review",
    tags: ["pipeline", "orchestration"],
  }, taskBodyWithLog(`- orchestration: used
  objective: verify legacy task compatibility
  allowed files: tools/**
  expected output: focused guard tests
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent
  stop condition: tests pass
  independent reviewer: reviewed guard scope
- evidence: PASS \`node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent\``));

  assert.deepEqual(validateStoreDetailed(root), []);
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

test("cli validate --json reports parseable orchestration start preflight fields", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0078",
    title: "Pipeline start without preflight",
    status: "doing",
    tags: ["pipeline", "orchestration"],
  }, taskBodyWithLog("- 2026-06-21: Ready to start without packet."));
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "validate", "--json"], { cwd: root, encoding: "utf8" });
  assert.notEqual(result.status, 0);
  assert.doesNotMatch(result.stdout, /^problem:/m);
  assert.doesNotMatch(result.stdout, /^hint:/m);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.equal(parsed.problems[0].code, "orchestration_start_preflight_missing");
  assert.equal(parsed.problems[0].taskId, "T0078");
  assert.equal(parsed.problems[0].status, "doing");
  assert.deepEqual(parsed.problems[0].missingFields, ["orchestration: used packet"]);
  assert.match(parsed.problems[0].nextAction, /orchestration-template/);
  assert.match(parsed.problems[0].nextAction, /orchestration-check T0078 --json/);
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

test("cli set --json reports structured start preflight transition failure", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0078",
    title: "Pipeline start without preflight",
    status: "todo",
    tags: ["pipeline", "orchestration"],
  }, taskBodyWithLog("- 2026-06-21: Ready to start without packet."));
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "set", "T0078", "--status", "doing", "--json"], { cwd: root, encoding: "utf8" });
  assert.notEqual(result.status, 0);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.equal(parsed.problem.code, "orchestration_start_preflight_missing");
  assert.equal(parsed.problem.taskId, "T0078");
  assert.equal(parsed.problem.status, "doing");
  assert.deepEqual(parsed.problem.missingFields, ["orchestration: used packet"]);
  assert.match(parsed.problem.nextAction, /orchestration-check T0078 --json/);
});

test("cli orchestration-check passes complete preflight packet without PASS evidence", (t) => {
  const root = tempRoot(t);
  const command = "node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent";
  const task = createTask(root, {
    title: "Subagent packet preflight",
    status: "doing",
    body: taskBodyWithLog(`- orchestration: used
  objective: verify subagent packet before launch
  allowed files: tools/taskboard/lib.mjs
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: ${command}
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--file", task.file], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr || result.stdout);
  assert.match(result.stdout, /ok: orchestration packet preflight passed/);
});

test("cli orchestration-check accepts positional task id", (t) => {
  const root = tempRoot(t);
  const command = "node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent";
  const task = createTask(root, {
    title: "Subagent packet preflight",
    status: "doing",
    body: taskBodyWithLog(`- orchestration: used
  objective: verify subagent packet before launch
  allowed files: tools/taskboard/cli.mjs
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: ${command}
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", task.fields.id], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr || result.stdout);
  assert.match(result.stdout, new RegExp(`ok: orchestration packet preflight passed for .*${task.fields.id}`));
});

test("cli orchestration-check --id emits resolved file in json", (t) => {
  const root = tempRoot(t);
  const command = "node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent";
  const task = createTask(root, {
    title: "Subagent packet preflight",
    status: "doing",
    body: taskBodyWithLog(`- orchestration: used
  objective: verify subagent packet before launch
  allowed files: tools/taskboard/cli.mjs
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: ${command}
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--id", task.fields.id, "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr || result.stdout);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, true);
  assert.equal(parsed.file, `tasks\\active\\${task.file.split(/[\\/]/).pop()}`);
  assert.equal(parsed.problem, null);
});

test("cli orchestration-check rejects missing or invalid id selectors", (t) => {
  const root = tempRoot(t);
  const cli = join(import.meta.dirname, "cli.mjs");
  const missing = spawnSync(process.execPath, [cli, "orchestration-check"], { cwd: root, encoding: "utf8" });
  assert.notEqual(missing.status, 0);
  assert.match(missing.stderr, /usage: orchestration-check <task-id>\|--id <task-id>\|--file <task\.md>/);

  const unknown = spawnSync(process.execPath, [cli, "orchestration-check", "T9999"], { cwd: root, encoding: "utf8" });
  assert.notEqual(unknown.status, 0);
  assert.match(unknown.stderr, /no task with id T9999/);

  const epic = createEpic(root, { title: "Not a task", status: "active" });
  const epicResult = spawnSync(process.execPath, [cli, "orchestration-check", epic.fields.id], { cwd: root, encoding: "utf8" });
  assert.notEqual(epicResult.status, 0);
  assert.match(epicResult.stderr, new RegExp(`no task with id ${epic.fields.id}`));
});

test("cli orchestration-check rejects conflicting selectors", (t) => {
  const root = tempRoot(t);
  const command = "node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent";
  const task = createTask(root, {
    title: "Subagent packet preflight",
    status: "doing",
    body: taskBodyWithLog(`- orchestration: used
  objective: verify subagent packet before launch
  allowed files: tools/taskboard/cli.mjs
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: ${command}
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", task.fields.id, "--file", task.file], { cwd: root, encoding: "utf8" });
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /use only one selector/);
});

test("cli orchestration-check rejects missing tool-use guard while validate stays compatible", (t) => {
  const root = tempRoot(t);
  const command = "node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent";
  const task = createTask(root, {
    title: "Subagent packet preflight",
    status: "review",
    body: taskBodyWithLog(`- orchestration: used
  objective: verify subagent packet before launch
  allowed files: tools/taskboard/lib.mjs
  expected output: packet preflight passes
  evidence command: ${command}
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope
- evidence: PASS \`${command}\``),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--file", task.file], { cwd: root, encoding: "utf8" });
  assert.notEqual(result.status, 0);
  assert.match(result.stdout, /missing\/invalid: tool-use guard/);
  assert.deepEqual(validateStore(root), []);
});

test("cli orchestration-check rejects unbounded allowed files", (t) => {
  const root = tempRoot(t);
  const command = "node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent";
  const invalidValues = [
    "everything",
    "tools/**",
    "tools/*",
    "tools/**/test.mjs",
    "../tools/taskboard/lib.mjs",
    "C:\\projects\\game-67-idle\\tools\\taskboard\\lib.mjs",
    "https://example.test/file.md",
  ];
  const cli = join(import.meta.dirname, "cli.mjs");
  for (const [index, allowedFiles] of invalidValues.entries()) {
    const task = createTask(root, {
      title: `Subagent packet preflight ${index}`,
      status: "doing",
      body: taskBodyWithLog(`- orchestration: used
  objective: verify subagent packet before launch
  allowed files: ${allowedFiles}
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: ${command}
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
    });
    const result = spawnSync(process.execPath, [cli, "orchestration-check", "--file", task.file], { cwd: root, encoding: "utf8" });
    assert.notEqual(result.status, 0, allowedFiles);
    assert.match(result.stdout, /missing\/invalid: allowed files bounds/, allowedFiles);
  }
});

test("cli orchestration-check accepts bounded allowed file patterns", (t) => {
  const root = tempRoot(t);
  const command = "node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent";
  const task = createTask(root, {
    title: "Subagent packet preflight",
    status: "doing",
    body: taskBodyWithLog(`- orchestration: used
  objective: verify subagent packet before launch
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/test.mjs; tasks/active/T*.md; tools/ai*.mjs; tools/taskboard/**; docs/ai-pipeline/**
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: ${command}
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--file", task.file], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr || result.stdout);
  assert.match(result.stdout, /ok: orchestration packet preflight passed/);
});

test("cli orchestration-check rejects placeholder tool-use guard", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Subagent packet preflight",
    status: "doing",
    body: taskBodyWithLog(`- orchestration: used
  objective: verify subagent packet before launch
  allowed files: tools/taskboard/lib.mjs
  tool-use guard: TBD
  expected output: packet preflight passes
  evidence command: node tools/ai.mjs orchestration-trace --parent-thread-id parent --json-output tmp/trace.json --json
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--file", task.file], { cwd: root, encoding: "utf8" });
  assert.notEqual(result.status, 0);
  assert.match(result.stdout, /missing\/invalid: tool-use guard/);
});

test("orchestration preflight next action falls back without task id", () => {
  const problem = orchestrationPreflightProblem({
    fields: { status: "doing" },
    body: taskBodyWithLog(`- orchestration: used
  objective: verify subagent packet before launch
  allowed files: tools/taskboard/lib.mjs
  expected output: packet preflight passes
  evidence command: node --test tools/taskboard/test.mjs
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
  });

  assert.equal(problem.code, "orchestration_preflight_missing");
  assert.equal(problem.taskId, "");
  assert.match(problem.nextAction, /orchestration-check <task-id> --json/);
  assert.doesNotMatch(problem.nextAction, /orchestration-check\s+--json/);
});

test("cli orchestration-check --file does not use file fallback as task id in next action", (t) => {
  const root = tempRoot(t);
  const activeDir = join(root, "tasks", "active");
  mkdirSync(activeDir, { recursive: true });
  const file = join(activeDir, "missing-id.md");
  writeFileSync(file, serializeDoc({
    title: "Missing id preflight",
    status: "doing",
    priority: "P2",
    tags: ["pipeline", "orchestration"],
    created: "2026-06-21",
    updated: "2026-06-21",
  }, taskBodyWithLog(`- orchestration: used
  objective: verify file fallback behavior
  allowed files: tools/taskboard/lib.mjs
  expected output: packet preflight passes
  evidence command: node --test tools/taskboard/test.mjs
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`)), "utf8");

  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--file", file, "--json"], { cwd: root, encoding: "utf8" });
  const parsed = JSON.parse(result.stdout);

  assert.notEqual(result.status, 0);
  assert.equal(parsed.problem.code, "orchestration_preflight_missing");
  assert.equal(parsed.problem.taskId, "");
  assert.match(parsed.problem.nextAction, /orchestration-check <task-id> --json/);
  assert.doesNotMatch(parsed.problem.nextAction, /missing-id\.md/);
});

test("cli orchestration-check --current resolves one doing orchestration task", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Current subagent packet preflight",
    status: "doing",
    tags: ["pipeline", "orchestration"],
    body: taskBodyWithLog(`- orchestration: used
  objective: verify current packet before launch
  allowed files: tools/taskboard/lib.mjs
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--current", "--json"], { cwd: root, encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr || result.stdout);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, true);
  assert.equal(parsed.file, `tasks\\active\\${task.fields.id}-current-subagent-packet-preflight.md`);
  assert.equal(parsed.problem, null);
});

test("cli orchestration-check --current rejects no current task", (t) => {
  const root = tempRoot(t);
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--current", "--json"], { cwd: root, encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.equal(result.stderr, "");
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.equal(parsed.file, null);
  assert.equal(parsed.problem.code, "current_task_missing");
  assert.equal(parsed.problem.selector, "current");
  assert.deepEqual(parsed.problem.taskIds, []);
  assert.match(parsed.problem.message, /no current doing pipeline\/orchestration task/);
  assert.equal(typeof parsed.problem.nextAction, "string");
  assert.match(parsed.problem.nextAction, /exactly one `doing` pipeline\/orchestration task/);
  assert.match(parsed.problem.nextAction, /orchestration-check --current --json/);
});

test("cli orchestration-check --current rejects multiple current tasks", (t) => {
  const root = tempRoot(t);
  for (const title of ["Current packet A", "Current packet B"]) {
    createTask(root, {
      title,
      status: "doing",
      tags: ["pipeline", "orchestration"],
      body: taskBodyWithLog(`- orchestration: used
  objective: verify current packet before launch
  allowed files: tools/taskboard/lib.mjs
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
    });
  }
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--current", "--json"], { cwd: root, encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.equal(result.stderr, "");
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.equal(parsed.file, null);
  assert.equal(parsed.problem.code, "current_task_ambiguous");
  assert.equal(parsed.problem.selector, "current");
  assert.deepEqual(parsed.problem.taskIds, ["T0001", "T0002"]);
  assert.match(parsed.problem.message, /multiple current doing pipeline\/orchestration tasks: T0001, T0002/);
  assert.equal(typeof parsed.problem.nextAction, "string");
  assert.match(parsed.problem.nextAction, /set exactly one pipeline\/orchestration task to `doing`/);
  assert.match(parsed.problem.nextAction, /orchestration-check --current --json/);
});

test("cli orchestration-check --current keeps non-json selector failures on stderr", (t) => {
  const root = tempRoot(t);
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--current"], { cwd: root, encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.equal(result.stdout, "");
  assert.match(result.stderr, /error: no current doing pipeline\/orchestration task/);
});

test("cli orchestration-check --current keeps non-json ambiguous selector failures on stderr", (t) => {
  const root = tempRoot(t);
  for (const title of ["Current packet A", "Current packet B"]) {
    createTask(root, {
      title,
      status: "doing",
      tags: ["pipeline", "orchestration"],
      body: taskBodyWithLog(`- orchestration: used
  objective: verify current packet before launch
  allowed files: tools/taskboard/lib.mjs
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
    });
  }
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", "--current"], { cwd: root, encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.equal(result.stdout, "");
  assert.match(result.stderr, /error: multiple current doing pipeline\/orchestration tasks: T0001, T0002/);
});

test("cli orchestration-check --current rejects conflicting selectors", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Current packet",
    status: "doing",
    tags: ["pipeline", "orchestration"],
    body: taskBodyWithLog(`- orchestration: used
  objective: verify current packet before launch
  allowed files: tools/taskboard/lib.mjs
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: packet preflight passes
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent
  stop condition: preflight reports ok
  independent reviewer: reviewed packet scope`),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-check", task.fields.id, "--current"], { cwd: root, encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /use only one selector/);
});

test("cli orchestration-template prints accepted packet shape", () => {
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-template"], { encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /^- orchestration: used/m);
  assert.match(result.stdout, /objective: <non-empty>/);
  assert.match(result.stdout, /allowed files: <non-empty>/);
  assert.ok(result.stdout.includes(`tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}`));
  assert.match(result.stdout, /expected output: <non-empty>/);
  assert.match(result.stdout, /evidence command: <non-empty>/);
  assert.match(result.stdout, /stop condition: <non-empty>/);
  assert.match(result.stdout, /independent reviewer: <non-empty>/);
});

test("subagent packet template contains tool-use guard and handoff fields", () => {
  const template = subagentPacketTemplate();

  assert.match(template, /objective:/);
  assert.match(template, /allowed files:/);
  assert.match(template, /forbidden files:/);
  assert.ok(template.includes(`tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}`));
  assert.match(template, /evidence command or artifact:/);
  assert.match(template, /handoff:/);
  for (const label of ["findings", "files", "commands/evidence", "risks", "owner action", "not-done"]) {
    assert.match(template, new RegExp(`${label}:`));
  }
});

test("subagent packet check accepts complete packet", () => {
  assert.equal(subagentPacketProblem(validSubagentPacket()), null);
});

test("subagent packet check rejects unbounded allowed files", () => {
  const packet = validSubagentPacket().replace("tools/taskboard/lib.mjs; tools/taskboard/test.mjs", "tools/**");
  const problem = subagentPacketProblem(packet);

  assert.equal(problem.code, "subagent_packet_invalid");
  assert.ok(problem.missingFields.includes("bounded allowed files"));
});

test("subagent packet check rejects missing handoff subfields", () => {
  const packet = validSubagentPacket().replace("  not-done: explicit gaps", "");
  const problem = subagentPacketProblem(packet);

  assert.equal(problem.code, "subagent_packet_invalid");
  assert.ok(problem.missingFields.includes("handoff not-done"));
});

test("cli subagent-packet-template prints reusable packet", () => {
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "subagent-packet-template"], { encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /objective:/);
  assert.ok(result.stdout.includes(`tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}`));
  assert.match(result.stdout, /evidence command or artifact:/);
  assert.match(result.stdout, /handoff:/);
});

test("every subagent packet preset emits lint-valid packets", () => {
  const names = subagentPacketPresetNames();
  assert.ok(names.includes("codebase-map"));
  assert.ok(names.includes("asset-research"));
  for (const name of names) {
    const { packets } = subagentPacketPreset(name);
    assert.ok(packets.length >= 1, `${name} emits no packets`);
    for (const packet of packets) {
      assert.equal(subagentPacketProblem(packet.text), null, `${name}/${packet.label} is not a valid packet`);
    }
  }
});

test("subagent packet preset fans out one packet per target", () => {
  const { packets, mode } = subagentPacketPreset("codebase-map", ["src/a/**", "tools/b/**"]);
  assert.equal(mode, "parallel");
  assert.equal(packets.length, 2);
  assert.match(packets[0].text, /allowed files: src\/a\/\*\*/);
  assert.match(packets[1].text, /allowed files: tools\/b\/\*\*/);
});

test("subagent packet preset rejects an unknown name and lists presets", () => {
  assert.throws(
    () => subagentPacketPreset("nope"),
    (err) => err.code === "unknown_preset" && Array.isArray(err.presets) && err.presets.includes("review"),
  );
});

test("cli subagent-packet-template --preset emits a parallel fan-out", () => {
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(
    process.execPath,
    [cli, "subagent-packet-template", "--preset", "codebase-map", "--targets", "src/a/**,tools/b/**"],
    { encoding: "utf8" },
  );
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /PARALLEL FAN-OUT/);
  assert.match(result.stdout, /# packet 1\/2/);
  assert.match(result.stdout, /# packet 2\/2/);
  assert.match(result.stdout, /allowed files: src\/a\/\*\*/);
});

test("cli subagent-packet-template --preset with no name lists presets", () => {
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "subagent-packet-template", "--preset"], { encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /presets: /);
  assert.match(result.stdout, /asset-intake/);
});

test("cli subagent-packet-template --preset rejects an unknown name", () => {
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "subagent-packet-template", "--preset", "nope"], { encoding: "utf8" });
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /unknown preset: nope/);
});

test("cli subagent-packet-check reports structured failures", (t) => {
  const root = tempRoot(t);
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "subagent-packet-check", "--text", "objective: only this", "--json"], { cwd: root, encoding: "utf8" });

  assert.notEqual(result.status, 0);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.equal(parsed.problem.code, "subagent_packet_invalid");
  assert.ok(parsed.problem.missingFields.includes("tool-use guard"));
  assert.ok(parsed.problem.missingFields.includes("handoff"));
});

test("cli subagent-packet-check accepts file input", (t) => {
  const root = tempRoot(t);
  const packet = join(root, "packet.txt");
  writeFileSync(packet, validSubagentPacket());
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "subagent-packet-check", "--file", packet, "--json"], { cwd: root, encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr || result.stdout);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, true);
  assert.equal(parsed.problem, null);
});

test("cli subagent-packet-check accepts stdin input", (t) => {
  const root = tempRoot(t);
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "subagent-packet-check", "--stdin", "--json"], {
    cwd: root,
    encoding: "utf8",
    input: validSubagentPacket(),
  });

  assert.equal(result.status, 0, result.stderr || result.stdout);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, true);
  assert.equal(parsed.problem, null);
});

test("cli subagent-packet-check rejects ambiguous stdin input", (t) => {
  const root = tempRoot(t);
  const packet = join(root, "packet.txt");
  writeFileSync(packet, validSubagentPacket());
  const cli = join(import.meta.dirname, "cli.mjs");

  for (const args of [
    ["subagent-packet-check", "--stdin", "--text", validSubagentPacket(), "--json"],
    ["subagent-packet-check", "--stdin", "--file", packet, "--json"],
  ]) {
    const result = spawnSync(process.execPath, [cli, ...args], {
      cwd: root,
      encoding: "utf8",
      input: validSubagentPacket(),
    });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /use only one subagent-packet-check input/);
  }
});

function bootstrapArgs(overrides = {}) {
  const values = {
    title: "Bootstrap orchestration task",
    objective: "verify bootstrap command",
    "allowed-files": "tools/taskboard/cli.mjs",
    "expected-output": "preflight passes",
    "evidence-command": "node --test tools/taskboard/test.mjs",
    "stop-condition": "current preflight passes",
    "independent-reviewer": "reviewed bootstrap contract",
    ...overrides,
  };
  return Object.entries(values).flatMap(([key, value]) => value === undefined ? [] : [`--${key}`, value]);
}

test("cli orchestration-bootstrap --help prints usage without requiring task args", (t) => {
  const root = tempRoot(t);
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [cli, "orchestration-bootstrap", "--help"], { cwd: root, encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr || result.stdout);
  assert.equal(result.stderr, "");
  assert.match(result.stdout, /usage: node tools\/taskboard\/cli\.mjs orchestration-bootstrap/);
  assert.match(result.stdout, /--allowed-files/);
  assert.match(result.stdout, /--evidence-command/);
  assert.match(result.stdout, /node tools\/ai\.mjs orchestration-check --current --json/);
});

test("cli orchestration-bootstrap creates a current preflight-valid task", (t) => {
  const root = tempRoot(t);
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [
    cli,
    "orchestration-bootstrap",
    ...bootstrapArgs({ tags: "taskboard" }),
    "--json",
  ], { cwd: root, encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr || result.stdout);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, true);
  assert.equal(parsed.doc.status, "doing");
  assert.deepEqual(parsed.doc.tags, ["pipeline", "orchestration", "subagents", "taskboard"]);
  assert.match(parsed.nextAction, /orchestration-check --current --json/);

  const doc = findDoc(root, parsed.doc.id);
  assert.ok(doc);
  assert.match(doc.body, /objective: verify bootstrap command/);
  assert.match(doc.body, /allowed files: tools\/taskboard\/cli\.mjs/);
  assert.ok(doc.body.includes(`tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}`));
  assert.match(doc.body, /evidence command: node --test tools\/taskboard\/test\.mjs/);

  const check = spawnSync(process.execPath, [cli, "orchestration-check", "--current", "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(check.status, 0, check.stderr || check.stdout);
  assert.equal(JSON.parse(check.stdout).problem, null);
});

test("cli orchestration-bootstrap creates T0078 current task that passes start preflight validation", (t) => {
  const root = tempRoot(t);
  for (let i = 0; i < 77; i += 1) {
    createTask(root, { title: `Seed ${i + 1}`, status: "dropped" });
  }
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [
    cli,
    "orchestration-bootstrap",
    ...bootstrapArgs({ tags: "taskboard" }),
    "--json",
  ], { cwd: root, encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr || result.stdout);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, true);
  assert.equal(parsed.doc.id, "T0078");
  assert.equal(parsed.doc.status, "doing");

  const check = spawnSync(process.execPath, [cli, "orchestration-check", "--current", "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(check.status, 0, check.stderr || check.stdout);
  assert.equal(JSON.parse(check.stdout).problem, null);

  const validate = spawnSync(process.execPath, [cli, "validate", "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(validate.status, 0, validate.stderr || validate.stdout);
  assert.equal(JSON.parse(validate.stdout).ok, true);
});

test("cli orchestration-bootstrap rejects missing args without creating tasks", (t) => {
  const root = tempRoot(t);
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [
    cli,
    "orchestration-bootstrap",
    ...bootstrapArgs({ objective: undefined }),
    "--json",
  ], { cwd: root, encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.equal(result.stderr, "");
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.equal(parsed.problem.code, "missing_required_argument");
  assert.deepEqual(parsed.problem.missingArgs, ["--objective"]);
  assert.deepEqual(listTasks(root), []);
});

test("cli orchestration-bootstrap rejects invalid allowed files without creating tasks", (t) => {
  const root = tempRoot(t);
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [
    cli,
    "orchestration-bootstrap",
    ...bootstrapArgs({ "allowed-files": "tools/**" }),
    "--json",
  ], { cwd: root, encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.equal(result.stderr, "");
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.equal(parsed.problem.code, "invalid_allowed_files");
  assert.match(parsed.problem.message, /bounded repo-local file paths/);
  assert.deepEqual(listTasks(root), []);
});

test("cli orchestration-bootstrap rejects existing current task without creating another", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Existing current",
    status: "doing",
    tags: ["pipeline", "orchestration"],
    body: taskBodyWithLog(`- orchestration: used
  objective: verify existing current
  allowed files: tools/taskboard/cli.mjs
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: preflight passes
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id parent
  stop condition: preflight passes
  independent reviewer: reviewed current task`),
  });
  const cli = join(import.meta.dirname, "cli.mjs");
  const result = spawnSync(process.execPath, [
    cli,
    "orchestration-bootstrap",
    ...bootstrapArgs(),
    "--json",
  ], { cwd: root, encoding: "utf8" });

  assert.notEqual(result.status, 0);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.problem.code, "current_task_exists");
  assert.deepEqual(parsed.problem.taskIds, ["T0001"]);
  assert.equal(listTasks(root).length, 1);
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
