---
id: T0078
title: Require orchestration preflight before work starts
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, taskboard]
created: 2026-06-21
updated: 2026-06-22
---

## What

Make new substantial pipeline/orchestration tasks mechanically require a complete orchestration preflight packet before they can remain or move into doing, so delegated work cannot start from an unbounded or unverified packet.

## Done when

- [x] New doing-state preflight validation rejects missing or malformed packets in tests, accepts valid bootstrap packets, taskboard validate and validate --review pass, and task log records subagent plus machine evidence.

## Open questions

## Log

- orchestration: used
  objective: Make new substantial pipeline/orchestration tasks mechanically require a complete orchestration preflight packet before they can remain or move into doing, so delegated work cannot start from an unbounded or unverified packet.
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/test.mjs; tools/taskboard/cli.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: Focused taskboard validator/tests that reject T0078+ doing orchestration tasks without a valid preflight packet while preserving legacy task compatibility and not touching gameplay/runtime.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose
  stop condition: New doing-state preflight validation rejects missing or malformed packets in tests, accepts valid bootstrap packets, taskboard validate and validate --review pass, and task log records subagent plus machine evidence.
  independent reviewer: two subagents inspect taskboard start-gate surfaces and compatibility before closeout; parent integrates findings
- subagent: Noether inspected taskboard lifecycle surfaces; found `createTask`, CLI `new`, `orchestration-bootstrap`, `updateDoc`/CLI `set`, and manual `doing` edits all need a shared lib-level start guard, while preflight must not require PASS evidence.
- subagent: Meitner recommended regression coverage for T0078+ doing validation, transition failure, CLI JSON problem shape, bootstrap T0078 acceptance, review closeout remaining stricter, and pre-T0078 compatibility.
- implementation: T0078+ substantial pipeline/orchestration tasks now require a complete preflight packet before entering or remaining in `doing`; `createTask`, `updateDoc`, and `validateStoreDetailed` share the guard, while review/done closeout still requires later machine PASS evidence.
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json` (ok true)
- evidence: PASS `node --test tools/taskboard/test.mjs` (109/109)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json` (ok true)
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose` (84/84 agents, unresolved failures 0, clean tail 5)
