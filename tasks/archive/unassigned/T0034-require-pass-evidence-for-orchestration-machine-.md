---
id: T0034
title: Require PASS evidence for orchestration machine checks
status: done
epic: ""
priority: P1
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

T0031+ substantial pipeline/orchestration tasks must prove that the declared
machine orchestration evidence was actually run. A packet-level
`evidence command` is not enough; the task log also needs a later
`evidence: PASS ...` entry for an approved machine check such as
`node tools/ai.mjs orchestration-trace ...` or
`node tools/ai.mjs status --agent-rollup ...`.

## Done when

- [x] Taskboard rejects a T0031+ orchestration packet with an approved machine
  evidence command but no later machine `evidence: PASS` line.
- [x] Taskboard rejects non-machine PASS evidence and machine PASS evidence
  recorded before the orchestration packet.
- [x] Taskboard accepts later PASS evidence for `orchestration-trace`,
  `status --agent-rollup`, and wrapped command lines.
- [x] The subagent protocol documents the required later PASS evidence.
- [x] Current active tasks validate with the stricter rule.

## Open questions

- None.

## Log

- orchestration: used
  objective: require PASS evidence for machine orchestration checks in taskboard
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0034-require-pass-evidence-for-orchestration-machine-.md
  expected output: taskboard rejects T0031+ machine evidence commands without a later evidence: PASS machine check and accepts trace/status PASS evidence
  evidence command: node --test tools/taskboard/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: validation stays static and checks recorded PASS evidence without executing arbitrary task-log commands
  independent reviewer: Hume reviewed PASS evidence guard scope and false-positive risks
- reviewer: PASS Hume found the guard direction reasonable, flagged the
  missing protocol update, confirmed T0031-T0033 already have compatible PASS
  evidence, and requested the PASS-before-packet regression.
- evidence: PASS `node --test tools/taskboard/test.mjs` (48 tests)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (20 subagent sessions)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `git diff --check`
- evidence: PASS current strict `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (26 subagent sessions); refreshed after T0039 strict evidence contract.
