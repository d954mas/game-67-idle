---
id: T0040
title: Surface current pipeline work in taskboard context
status: review
epic: ""
priority: P1
tags: [pipeline, taskboard, orchestration, context]
created: 2026-06-21
updated: 2026-06-21
---

## What

`tasks/STATUS.md` can legitimately describe the active game status while the
current thread is doing reusable pipeline/orchestration work. Make taskboard
orientation surface actionable task-store work before the live status sections
so agents do not drift back to game implementation when a pipeline task is
actually current.

## Done when

- [x] `node tools/taskboard/cli.mjs summary` shows current actionable work
  before the live `Current Goal` section.
- [x] `node tools/taskboard/cli.mjs context` shows current actionable work
  before expanded status sections.
- [x] Existing review-queue hiding behavior remains unchanged.
- [x] Focused taskboard tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: surface current task-store work before stale broader live status in taskboard orientation
  allowed files: tools/taskboard/cli.mjs, tools/taskboard/test.mjs, tasks/active/T0040-surface-current-pipeline-work-in-taskboard-context.md
  expected output: summary/context orient first around active pipeline tasks while still preserving live game status sections
  evidence command: node --test tools/taskboard/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove actionable work appears before `Current Goal` and full review validation remains green
  independent reviewer: Boole audits whether Current Work section is enough or a STATUS validator is required
- reviewer: PASS Boole confirmed `Current Work` before `Current Goal` is the
  right fix and a validator is not needed because this is rendered output order,
  not task-store data integrity.
- evidence: PASS `node --test tools/taskboard/test.mjs` (55 tests)
- evidence: PASS `node tools/taskboard/cli.mjs summary` showed T0040 under
  `Current Work` before Dragon Grove `Current Goal`.
- evidence: PASS `node tools/taskboard/cli.mjs context --tasks-limit 3` showed
  T0040 under `Current Work` before expanded status sections.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (27 subagent sessions)
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `git diff --check`
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
