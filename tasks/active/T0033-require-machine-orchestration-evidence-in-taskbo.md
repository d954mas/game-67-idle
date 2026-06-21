---
id: T0033
title: Require machine orchestration evidence in taskboard
status: review
epic: ""
priority: P1
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Strengthen the taskboard orchestration guard so newer substantial
pipeline/orchestration tasks require a machine-readable orchestration evidence
command, not only a complete prose packet.

## Done when

- [x] Trace-era substantial orchestration tasks without machine evidence fail
      `validateStoreDetailed`.
- [x] `node tools/ai.mjs orchestration-trace ...` evidence is accepted.
- [x] `node tools/ai.mjs status --agent-rollup ...` evidence is accepted.
- [x] Existing pre-trace review tasks remain valid without retroactive rewrites.
- [x] Protocol docs explain the machine evidence requirement.
- [x] Validation evidence is recorded in this log.

## Open questions

- Should a later task execute the declared evidence command, or keep validation
  static and require the lead to record PASS evidence?

## Log

- orchestration: used
  objective: require machine-readable orchestration evidence for new substantial pipeline/orchestration tasks
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0033-require-machine-orchestration-evidence-in-taskbo.md
  expected output: taskboard guard rejects trace-era packets without orchestration-trace/status agent-rollup evidence and accepts both machine evidence commands
  evidence command: node --test tools/taskboard/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: validation is static and machine-evidence-aware without executing arbitrary task log commands
  independent reviewer: Kant reviewed guard compatibility risks and false positives

- evidence: PASS `node --test tools/taskboard/test.mjs` (45 tests). Covered
  T0031+ missing machine evidence rejection, T0030 compatibility, machine
  command outside `evidence command` rejection, `orchestration-trace`
  acceptance, `status --agent-rollup` source requirement, and wrapped command
  acceptance.
- evidence: PASS live `node tools/ai.mjs status --agent-rollup
  --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session`; output showed
  `Agent Rollup` with 19 subagent sessions and no unresolved profile failures.
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`,
  `node tools/taskboard/cli.mjs validate`, `node tools/ai.mjs validate
  --review`, and `git diff --check`.
- reviewer: Kant recommended using the existing `evidence command` field, the
  T0031 compatibility boundary, parsing that field specifically instead of the
  whole packet, requiring `--parent-thread-id` or `--trace-session` for
  `status --agent-rollup`, and keeping validation static rather than executing
  task-log commands. Implemented with regression tests.
- evidence: PASS current strict `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (26 subagent sessions); refreshed after T0039 strict evidence contract.
