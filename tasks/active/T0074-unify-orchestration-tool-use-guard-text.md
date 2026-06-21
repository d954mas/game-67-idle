---
id: T0074
title: Unify orchestration tool-use guard text
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, profiling]
created: 2026-06-21
updated: 2026-06-21
---

## What

Remove or mechanically guard duplicate orchestration tool-use guard text so status guidance and taskboard bootstrap cannot drift apart.

## Done when

- [x] Duplicate guard drift is removed or covered by targeted tests, validate --review passes, and task log records subagent plus machine evidence.

## Open questions

## Log

- orchestration: used
  objective: Remove or mechanically guard duplicate orchestration tool-use guard text so status guidance and taskboard bootstrap cannot drift apart.
  allowed files: tools/taskboard/**; tools/ai_profile/**; tools/ai*.mjs; tasks/active/T*.md
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: One focused tooling fix or validator/test that keeps orchestration guard wording aligned; no gameplay/runtime edits.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose
  stop condition: Duplicate guard drift is removed or covered by targeted tests, validate --review passes, and task log records subagent plus machine evidence.
  independent reviewer: two subagents inspect distinct options before implementation; parent reconciles findings
- 2026-06-21: independent review: Erdos recommended `tools/taskboard/lib.mjs` as the single source of truth and flagged `orchestration-template` as the remaining drift source; Harvey recommended shared-constant assertions for status and taskboard outputs plus the non-current-task edge path.
- evidence: PASS `node --test tools/taskboard/test.mjs` (81/81)
- evidence: PASS `node --test tools/ai_profile/test.mjs` (48/48)
- evidence: PASS `node --test tools/ai.test.mjs` (26/26)
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json` (ok true)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose` (72/72 agents, unresolved failures 0, clean tail 32)
