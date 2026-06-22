---
id: T0073
title: Carry tool-use guard into orchestration guidance
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, profiling]
created: 2026-06-21
updated: 2026-06-22
---

## What

Make status-driven orchestration guidance carry the compact tool-use guard into the bootstrap command so subagent packets reduce known PowerShell/path/evidence mistakes without manual rewriting.

## Done when

- [x] Status guidance includes a valid bootstrap tool-use guard, focused tests pass, validate --review passes, and task log records subagent plus machine evidence.

## Open questions

## Log

- orchestration: used
  objective: Make status-driven orchestration guidance carry the compact tool-use guard into the bootstrap command so subagent packets reduce known PowerShell/path/evidence mistakes without manual rewriting.
  allowed files: tools/ai_profile/status.mjs; tools/ai_profile/test.mjs; tools/ai.test.mjs; tasks/active/T*.md
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: One focused status guidance change with targeted tests; no gameplay/runtime edits.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose
  stop condition: Status guidance includes a valid bootstrap tool-use guard, focused tests pass, validate --review passes, and task log records subagent plus machine evidence.
  independent reviewer: two subagents inspect status guidance/test coverage before implementation; parent reconciles findings
- 2026-06-21: independent review: Nash confirmed the zero-current bootstrap guidance should include `--tool-use-guard` and recommended stdout coverage; Popper confirmed no facade test was needed and added the existing-current negative assertion edge case.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (48/48)
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json` (ok true)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose` (70/70 agents, unresolved failures 0, clean tail 30)
