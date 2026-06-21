---
id: T0055
title: Suppress game status sections for pipeline current work
status: review
epic: ""
priority: P2
tags: [pipeline, taskboard, orchestration, context]
created: 2026-06-21
updated: 2026-06-21
---

## What

Taskboard summary/context still expand `tasks/STATUS.md` game sections such as
`Current Goal` and `Next Priorities` while the active actionable work is
pipeline/orchestration/tooling. That stale game context can steer an
orchestrator or subagent back into playable implementation. Suppress live game
status sections when all current actionable tasks are pipeline/tooling work, and
show an explicit note that the current work is pipeline-scoped.

## Done when

- [x] `summary` still shows `Current Work` for pipeline tasks but omits expanded
      game `Current Goal`/`Next Priorities` sections.
- [x] `context` still shows actionable pipeline tasks but omits expanded game
      status sections.
- [x] Normal non-pipeline actionable tasks keep the existing summary/context
      behavior.
- [x] Focused taskboard tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: keep taskboard orientation focused on pipeline current work instead
  of expanding stale game status sections
  allowed files: tools/taskboard/cli.mjs, tools/taskboard/test.mjs,
  tasks/active/T0055-suppress-game-status-sections-for-pipeline-curre.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: summary/context suppress live game status sections only when
  current actionable work is entirely pipeline/tooling-scoped
  evidence command: node --test tools/taskboard/test.mjs; node
  tools/taskboard/cli.mjs summary; node tools/taskboard/cli.mjs context
  --tasks-limit 3; node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose; node
  tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove pipeline current work suppresses game
  status sections while normal task behavior remains compatible
  independent reviewer: Locke audits summary/context behavior and compatibility
- reviewer: PASS Locke confirmed summary/context should keep `Current Work`
  visible while suppressing expanded live game sections for pipeline/tooling
  current work; recommended tag-only detection with lower-case normalization and
  a negative test for pipeline words in non-pipeline task bodies.
- evidence: PASS `node --test tools/taskboard/test.mjs`; output showed 61
  tests passed, including pipeline suppression and body-keyword negative cases.
- evidence: PASS `node tools/taskboard/cli.mjs summary`; output showed T0055
  under `Current Work`, `Status Context`, and no expanded Dragon Grove game
  sections.
- evidence: PASS `node tools/taskboard/cli.mjs context --tasks-limit 3`;
  output showed T0055 under `Current Work`/`Actionable Tasks`, `Status Context`,
  and no expanded Dragon Grove game sections.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`; output showed
  `subagent sessions: 42` and strict agent rollup passed.
- evidence: PASS `node tools/taskboard/cli.mjs validate`; output showed no
  problems found.
- evidence: PASS `node tools/ai.mjs validate --review`; output showed reusable
  pipeline quick+review validation passed.
