---
id: T0072
title: Improve orchestration packet guidance
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Find and implement the next small improvement that reduces subagent tool-use mistakes or makes orchestration evidence easier to produce correctly.

## Done when

- [x] The next orchestration friction is fixed, covered by targeted tests or skill/doc validation, validate --review passes, and task log records subagent plus machine evidence.

## Open questions

## Log

- orchestration: used
  objective: Find and implement the next small improvement that reduces subagent tool-use mistakes or makes orchestration evidence easier to produce correctly.
  allowed files: tools/ai*.mjs; tools/ai_profile/**; tools/taskboard/**; docs/ai-pipeline/**; .codex/skills/ai-pipeline-maintenance/**; tasks/active/T*.md
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: One focused pipeline/tooling or guidance change with targeted tests; no gameplay/runtime edits.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose
  stop condition: The next orchestration friction is fixed, covered by targeted tests or skill/doc validation, validate --review passes, and task log records subagent plus machine evidence.
  independent reviewer: two subagents inspect distinct orchestration surfaces before implementation; parent reconciles findings
- 2026-06-21: independent review: Averroes found that strict `status --agent-rollup --require-agent-rollup-ok` could fail structurally while `Next Action` still reported no profiling action needed; Poincare found a related packet-guard wording improvement deferred behind the stricter evidence UX bug.
- evidence: PASS `node --test tools/ai.test.mjs` (26/26)
- evidence: PASS `node --test tools/ai_profile/test.mjs` (48/48)
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json` (ok true)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose` (68/68 agents, unresolved failures 0, clean tail 28)
