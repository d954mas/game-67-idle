---
id: T0057
title: Remove session-specific command from live status
status: done
epic: ""
priority: P2
tags: [pipeline, taskboard, orchestration, context, profiling]
created: 2026-06-21
updated: 2026-06-22
---

## What

`tasks/STATUS.md` currently includes a strict agent-rollup command with this
thread's exact `--parent-thread-id` and `--session-root`. That is valid task
evidence, but it is not good live default guidance because it goes stale across
sessions. Replace the session-specific command in live status with generic
instruction to run strict agent rollup using the current parent/session values,
while keeping exact commands in task logs.

## Done when

- [x] `tasks/STATUS.md` no longer contains this thread's hardcoded
      `--parent-thread-id` or session date path.
- [x] Required validation still tells agents to run strict agent rollup for
      orchestration closeout with current session values.
- [x] Exact machine evidence for this task remains in the task log.
- [x] Taskboard validation, context budget review, and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: remove session-specific strict agent-rollup command from live
  status while preserving exact command evidence in the task log
  allowed files: tasks/STATUS.md,
  tasks/active/T0057-remove-session-specific-command-from-live-status.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: live Required Validation stays portable and task evidence
  keeps exact parent-thread/session-root command
  evidence command: node tools/taskboard/cli.mjs summary; node
  tools/taskboard/cli.mjs context --tasks-limit 3; node tools/context_budget.mjs
  --review; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok
  --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose; node
  tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: status is portable, task evidence remains exact, validation
  stays green
  independent reviewer: Aquinas audits wording and validator risk
- reviewer: PASS Aquinas confirmed exact `--parent-thread-id` and
  `--session-root` values belong in task evidence, not live `STATUS.md`, and
  noted taskboard validator risk is low because it parses task logs, not the
  status Required Validation block.
- evidence: PASS `rg -n "019ee5cc|2026\\06\\21|parent-thread-id 019ee|session-root C:\\Users" tasks/STATUS.md tasks/active/T0057-remove-session-specific-command-from-live-status.md`;
  output showed the exact parent/session values only in this task log, not in
  `tasks/STATUS.md`.
- evidence: PASS `node tools/taskboard/cli.mjs summary`; output showed T0057 as
  current work and no live session-specific validation command.
- evidence: PASS `node tools/context_budget.mjs --review`; output showed
  context budgets pass.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`; output showed
  `subagent sessions: 44` and strict agent rollup passed.
- evidence: PASS `node tools/taskboard/cli.mjs validate`; output showed no
  problems found.
- evidence: PASS `node tools/ai.mjs validate --review`; output showed reusable
  pipeline quick+review validation passed.
- evidence: PASS after moving T0057 to review,
  `node tools/taskboard/cli.mjs summary` showed `open_actionable_tasks: 0` and
  kept the pipeline/orchestration current goal.
- evidence: PASS after moving T0057 to review, `rg -n
  "019ee5cc|2026\\06\\21|parent-thread-id 019ee|session-root C:\\Users"
  tasks/STATUS.md` returned no matches.
