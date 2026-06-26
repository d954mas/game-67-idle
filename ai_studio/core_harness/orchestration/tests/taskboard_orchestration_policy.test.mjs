// Core Harness orchestration policy over Taskboard state.
// Run:
// node --test ai_studio/core_harness/orchestration/tests/taskboard_orchestration_policy.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";
import {
  createTask,
  serializeDoc,
  slugify,
  updateDoc,
  validateStoreDetailed,
} from "../../../taskboard/lib.mjs";
import {
  DEFAULT_ORCHESTRATION_TOOL_USE_GUARD,
  orchestrationPreflightProblem,
  subagentPacketProblem,
  subagentPacketPreset,
  subagentPacketPresetNames,
} from "../lib.mjs";
import {
  currentDoingOrchestrationTaskIds,
  orchestrationTaskEvidenceProblem,
  taskboardOrchestrationProblems,
} from "../taskboard_policy.mjs";

const orchestrationDir = dirname(import.meta.dirname);
const orchestrationCli = join(orchestrationDir, "cli.mjs");

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "taskboard-orchestration-test-"));
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

function completeOrchestrationLog() {
  return `- orchestration: used
  objective: verify the transition guard behavior
  allowed files: ai_studio/taskboard/lib.mjs, ai_studio/core_harness/orchestration/taskboard_policy.mjs
  tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}
  expected output: focused taskboard and orchestration tests
  evidence command: node --test ai_studio/core_harness/orchestration/tests/taskboard_orchestration_policy.test.mjs
  stop condition: requested guard scenarios pass
  independent reviewer: reviewed transition and validation cases`;
}

function validSubagentPacket() {
  return `objective: Verify the reusable packet shape.
allowed files: ai_studio/taskboard/lib.mjs; ai_studio/core_harness/orchestration/taskboard_policy.mjs
forbidden files: AGENTS.md; src/clean_seed_main.c
tool-use guard: ${DEFAULT_ORCHESTRATION_TOOL_USE_GUARD}
expected output: PASS or CONCERNS with exact evidence.
evidence command or artifact: node --test ai_studio/core_harness/orchestration/tests/taskboard_orchestration_policy.test.mjs
stop condition: focused packet checks pass or a blocker is found.
handoff:
  findings: exact findings and verdict
  files: files inspected
  commands/evidence: commands run and results
  risks: residual risks
  owner action: lead next action
  not-done: explicit gaps`;
}

test("Taskboard validation stays store-only while orchestration policy reports advisories", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Pipeline review without orchestration",
    status: "review",
    body: taskBodyWithLog("- 2026-06-21: Ready for review."),
  });

  assert.deepEqual(validateStoreDetailed(root), []);
  const [problem] = taskboardOrchestrationProblems(root);
  assert.equal(problem.code, "orchestration_evidence_missing");
  assert.equal(problem.taskId, "T0001");
  assert.deepEqual(problem.missingFields, ["orchestration: used packet"]);
});

test("orchestration policy reports start preflight for current broad doing work", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0078",
    title: "Pipeline start without preflight",
    status: "doing",
    tags: ["pipeline", "orchestration"],
  }, taskBodyWithLog("- 2026-06-21: Ready to start without packet."));

  assert.deepEqual(currentDoingOrchestrationTaskIds(root), ["T0078"]);
  const [problem] = taskboardOrchestrationProblems(root);
  assert.equal(problem.code, "orchestration_start_preflight_missing");
  assert.equal(problem.taskId, "T0078");
  assert.match(problem.nextAction, /orchestration-check T0078 --json/);
});

test("small-scope exception suppresses orchestration policy", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Taskboard docs touch",
    status: "review",
    tags: ["pipeline"],
    body: taskBodyWithLog("- orchestration: not needed - small scope: docs-only"),
  });

  assert.deepEqual(taskboardOrchestrationProblems(root), []);
});

test("orchestration policy keeps gameplay tasks out of pipeline checks", (t) => {
  const root = tempRoot(t);
  const gameplayBody = `## What

Run the playable scene.

## Done when

- [ ] scene check passes

## Log

- 2026-06-21: Tested the scene.
`;
  createTask(root, {
    title: "Gameplay validation task",
    status: "review",
    tags: ["prototype"],
    body: gameplayBody,
  });

  assert.equal(orchestrationTaskEvidenceProblem({
    kind: "task",
    fields: { id: "T9999", title: "Gameplay validation task", status: "review", tags: ["prototype"] },
    body: gameplayBody,
  }), null);
  assert.deepEqual(taskboardOrchestrationProblems(root), []);
});

test("complete orchestration packet passes review and start checks", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Pipeline guarded task",
    status: "doing",
    tags: ["pipeline", "orchestration"],
    body: taskBodyWithLog(completeOrchestrationLog()),
  });

  assert.equal(orchestrationPreflightProblem(task), null);
  updateDoc(root, "T0001", { fields: { status: "review" } });
  assert.deepEqual(taskboardOrchestrationProblems(root), []);
});

test("archived done orchestration task remains audited", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Archived pipeline task",
    status: "doing",
    tags: ["pipeline", "orchestration"],
    body: taskBodyWithLog("- 2026-06-21: Finished without packet."),
  });
  const archiveDir = join(root, "tasks", "archive", "unassigned");
  mkdirSync(archiveDir, { recursive: true });
  const archivedFile = join(archiveDir, "T0078-archived-pipeline-task.md");
  writeFileSync(archivedFile, readFileSync(task.file, "utf8").replace("id: T0001", "id: T0078").replace("status: doing", "status: done"));
  rmSync(task.file);

  const [problem] = taskboardOrchestrationProblems(root);
  assert.equal(problem.code, "orchestration_evidence_missing");
  assert.equal(problem.taskId, "T0078");
});

test("subagent packet presets emit lint-valid packets", () => {
  for (const name of subagentPacketPresetNames()) {
    for (const packet of subagentPacketPreset(name).packets) {
      assert.equal(subagentPacketProblem(packet.text), null, `${name}: ${packet.label}`);
    }
  }
});

test("subagent packet check rejects unbounded allowed files", () => {
  const packet = validSubagentPacket().replace(
    "ai_studio/taskboard/lib.mjs; ai_studio/core_harness/orchestration/taskboard_policy.mjs",
    "tools/**",
  );

  const problem = subagentPacketProblem(packet);
  assert.equal(problem.code, "subagent_packet_invalid");
  assert.match(problem.message, /bounded allowed files/);
});

test("orchestration-check --current resolves exactly one current task", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(root, {
    id: "T0078",
    title: "Pipeline current work",
    status: "doing",
    tags: ["pipeline", "orchestration"],
  }, taskBodyWithLog(completeOrchestrationLog()));

  const result = spawnSync(process.execPath, [orchestrationCli, "orchestration-check", "--current", "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, true);
  assert.match(parsed.file, /T0078-pipeline-current-work\.md$/);
});
