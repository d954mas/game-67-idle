---
id: T0070
title: Add orchestration task bootstrap command
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Every orchestration slice currently starts with several manual steps:

- `new task --title ...`
- edit status/tags/body
- paste the orchestration packet
- fill packet fields
- run current preflight

That ceremony is now repeated and visible in profiler/status guidance. Add a
small bootstrap command that creates a `doing` pipeline/orchestration task with
a complete packet from CLI arguments, so the manager can move from status
guidance to a current preflight task in one command.

## Done when

- [x] A taskboard CLI command can create a `doing` pipeline/orchestration task
      with required packet fields supplied from arguments.
- [x] The AI facade exposes the same command.
- [x] Created tasks pass `orchestration-check --current --json` when valid
      packet arguments are provided.
- [x] Missing required arguments fail clearly without creating a task.
- [x] Focused taskboard/facade validation and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: add a bootstrap command for creating complete current
  orchestration tasks
  allowed files: tools/taskboard/cli.mjs, tools/taskboard/lib.mjs,
  tools/taskboard/test.mjs, tools/ai.mjs, tools/ai.test.mjs,
  tasks/active/T0070-add-orchestration-task-bootstrap-command.md
  tool-use guard: exact paths/discovery before reads; use Select-Object
  -Skip/-First for line windows; trace/status commands include evidence source
  and --json-output where applicable
  expected output: one CLI/facade command creates a current orchestration task
  with a complete packet and focused tests cover success and missing-argument
  failure paths
  evidence command: node --test tools/taskboard/test.mjs; node --test tools/ai.test.mjs; node tools/ai.mjs orchestration-check --current --json; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: bootstrap command creates a preflight-valid current task,
  invalid usage is safe, and review validation passes
  independent reviewer: subagents audit command contract and scope
- reviewer Euler: PASS; command name `orchestration-bootstrap`, required
  packet args, optional default `tool-use-guard`, default `doing` status, and
  default `pipeline/orchestration/subagents` tags match the intended workflow.
- reviewer Euclid: PASS; facade exposure is required, zero-current status
  guidance should point at bootstrap, and status/profile tests should cover the
  guidance change.
- evidence: PASS `node --test tools/taskboard/test.mjs` (80/80).
- evidence: PASS `node --test tools/ai.test.mjs` (24/24).
- evidence: PASS `node --test tools/ai_profile/test.mjs` (48/48).
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`
  returned `{ "ok": true, "file": "tasks\\active\\T0070-add-orchestration-task-bootstrap-command.md", "problem": null }`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`; telemetry
  agents 64/64, unresolved failures 0, agent tool-use clean tail 24.
- evidence: PASS `node tools/taskboard/cli.mjs validate`.
- evidence: PASS `node tools/ai.mjs validate --review`.
- evidence: PASS `git diff --check` (only global git ignore permission warning).
