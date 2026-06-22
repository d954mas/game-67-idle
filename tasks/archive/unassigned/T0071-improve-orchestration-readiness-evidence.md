---
id: T0071
title: Improve orchestration readiness evidence
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, profiling]
created: 2026-06-21
updated: 2026-06-22
---

## What

Find and implement the next small fix that makes the agent orchestration workflow more reliable or easier to verify after the new bootstrap command.

## Done when

- [x] A follow-up orchestration weakness is fixed, covered by targeted tests, validate --review passes, and the task log records evidence.

## Open questions

## Log

- orchestration: used
  objective: Find and implement the next small fix that makes the agent orchestration workflow more reliable or easier to verify after the new bootstrap command.
  allowed files: tools/ai*.mjs; tools/ai_profile/**; tools/taskboard/**; docs/ai-pipeline/**; tasks/active/T*.md
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: One focused pipeline/tooling change with tests and task evidence; no gameplay/runtime edits.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose
  stop condition: A follow-up orchestration weakness is fixed, covered by targeted tests, validate --review passes, and the task log records evidence.
  independent reviewer: two subagents inspect different orchestration surfaces before implementation; parent reconciles findings
- 2026-06-21: independent review: Kepler and Parfit independently identified the same P1 trap: `orchestration-bootstrap` could create a task that immediately failed `orchestration-check` because its evidence command was not machine-verifiable.
- evidence: PASS `node --test tools/taskboard/test.mjs` (81/81)
- evidence: PASS `node --test tools/ai.test.mjs` (25/25)
- evidence: PASS `node --test tools/ai_profile/test.mjs` (48/48)
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json` (ok true)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose` (66/66 agents, unresolved failures 0, clean tail 26)
