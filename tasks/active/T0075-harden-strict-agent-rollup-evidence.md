---
id: T0075
title: Harden strict agent rollup evidence
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, profiling]
created: 2026-06-21
updated: 2026-06-21
---

## What

Make strict orchestration status evidence fail on unresolved subagent failures or missing usable telemetry so agent rollup cannot pass on empty or failed subagent sessions.

## Done when

- [x] Strict rollup fails on unresolved subagent failures or missing telemetry in tests, current strict evidence passes, focused tests pass, validate --review passes, and task log records subagent plus machine evidence.

## Open questions

## Log

- orchestration: used
  objective: Make strict orchestration status evidence fail on unresolved subagent failures or missing usable telemetry so agent rollup cannot pass on empty or failed subagent sessions.
  allowed files: tools/ai_profile/status.mjs; tools/ai_profile/test.mjs; tools/ai.mjs; tools/ai.test.mjs; tools/taskboard/lib.mjs; tools/taskboard/test.mjs; tasks/active/T*.md
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: Focused status/test change that hardens strict agent rollup evidence without gameplay/runtime edits.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose
  stop condition: Strict rollup fails on unresolved subagent failures or missing telemetry in tests, current strict evidence passes, focused tests pass, validate --review passes, and task log records subagent plus machine evidence.
  independent reviewer: two subagents inspect ai_profile strict semantics and facade/taskboard evidence before closeout; parent integrates findings
- subagent: Tesla reviewed ai_profile strict semantics; recommended strict_ok as trace/count plus no unresolved failures, no missing telemetry, no parse errors, while keeping classified tool-use failures diagnostic.
- subagent: Sartre reviewed facade/taskboard evidence; found min-agents was not part of taskboard evidence matching and facade help/tests hid strict evidence knobs.
- implementation: strict status evidence now exits on agent_rollup.strict_ok, tracks diagnostic failed strict-status probes separately to avoid self-poisoning, and taskboard evidence signatures compare min-agents strength.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (50/50)
- evidence: PASS `node --test tools/taskboard/test.mjs` (85/85)
- evidence: PASS `node --test tools/ai.test.mjs` (26/26)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `git diff --check`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose`
