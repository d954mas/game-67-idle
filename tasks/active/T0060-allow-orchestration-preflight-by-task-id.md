---
id: T0060
title: Allow orchestration preflight by task id
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

`orchestration-check` now validates packet text before subagent launch, but it
requires a copied task file path. Since the agent rollup repeatedly caught
path-related command failures, let the preflight command resolve tasks by ID:
`node tools/taskboard/cli.mjs orchestration-check T0060` or
`--id T0060`, while keeping `--file` supported.

## Done when

- [x] `orchestration-check <task-id>` resolves the task through the task store.
- [x] `orchestration-check --id <task-id> --json` returns structured output
      with the resolved task file.
- [x] Existing `--file` behavior remains supported.
- [x] Focused taskboard tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: reduce path-copy friction for orchestration packet preflight
  allowed files: tools/taskboard/cli.mjs, tools/taskboard/test.mjs,
  tasks/active/T0060-allow-orchestration-preflight-by-task-id.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: `orchestration-check` accepts task IDs and still accepts
  explicit task file paths
  evidence command: node --test tools/taskboard/test.mjs; node tools/taskboard/cli.mjs orchestration-check T0060 --json; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: ID-based preflight passes and taskboard validation remains
  compatible
  independent reviewer: Russell audits CLI compatibility and expected tests

- reviewer Russell: PASS for ID-based preflight as path-friction reduction;
  requested tests for positional id, `--id --json`, missing selector, unknown
  task, epic id rejection, and conflicting selector handling.
- evidence: PASS `node --test tools/taskboard/test.mjs` (69/69).
- evidence: PASS `node tools/taskboard/cli.mjs orchestration-check T0060
  --json` returned `ok: true` and resolved
  `tasks\active\T0060-allow-orchestration-preflight-by-task-id.md`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose` (47
  subagent sessions; telemetry agents 47/47; clean tail 7).
- evidence: PASS `node tools/taskboard/cli.mjs validate`.
- evidence: PASS `node tools/ai.mjs validate --review`.
