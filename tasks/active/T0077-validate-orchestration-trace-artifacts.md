---
id: T0077
title: Validate orchestration trace artifacts
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, taskboard]
created: 2026-06-21
updated: 2026-06-21
---

## What

Make review/done taskboard validation reject orchestration-trace PASS evidence when the declared json-output artifact is missing, invalid, failing, or does not prove the declared trace evidence.

## Done when

- [x] New trace artifact validation rejects missing invalid and failing artifacts in tests, accepts valid trace artifacts, taskboard validate and validate --review pass, and task log records subagent plus machine evidence.

## Open questions

## Log

- orchestration: used
  objective: Make review/done taskboard validation reject orchestration-trace PASS evidence when the declared json-output artifact is missing, invalid, failing, or does not prove the declared trace evidence.
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/test.mjs; tools/ai_profile/orchestration_trace.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: Focused taskboard validator/tests that require valid trace JSON artifacts for new orchestration trace evidence without executing task-log commands or touching gameplay/runtime.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose
  stop condition: New trace artifact validation rejects missing invalid and failing artifacts in tests, accepts valid trace artifacts, taskboard validate and validate --review pass, and task log records subagent plus machine evidence.
  independent reviewer: two subagents inspect trace artifact shape and taskboard lifecycle compatibility before closeout; parent integrates findings
- subagent: Banach inspected `tools/ai_profile/orchestration_trace.mjs` and trace tests; recommended validating `ok:true`, empty problem/failure arrays, matching session or parent thread, transcript calls for session mode, and subagent count/session records for parent mode.
- subagent: Hegel inspected taskboard lifecycle; recommended adding T0077+ artifact validation in shared `orchestrationEvidenceProblem`/`hasMatchingMachineEvidencePassAfterOrchestration`, preserving old task compatibility and never executing task-log commands.
- implementation: T0077+ `orchestration-trace` evidence now requires the declared repo-local `.json` artifact to exist, parse, pass, match declared source, and satisfy declared min-agents; pre-T0077 trace evidence and strict status rollup compatibility remain intact.
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json` (ok true)
- evidence: PASS `node --test tools/taskboard/test.mjs` (99/99)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose` (82/82 agents, unresolved failures 0, clean tail 3)
