---
id: T0036
title: Preserve status session selection through ai facade
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

The public `node tools/ai.mjs status --session <id>` facade advertised profile
session selection, but stripped `--session` before invoking
`tools/ai_profile/status.mjs`. As a result, session-scoped profiling could
silently fall back to the active profile, hiding older or multi-day agent work.
Preserve the `--session` selector while keeping Codex transcript import as a
separate analysis-time recovery step.

## Done when

- [x] The public facade forwards `--session <id>` to profile status.
- [x] Existing Codex transcript import via `status --profile <p> --session <file>`
  keeps working.
- [x] Agent-rollup hint commands preserve session/profile selection where
  applicable.
- [x] Focused facade/profile tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: keep profiling session selection intact through the public AI facade
  allowed files: tools/ai.mjs, tools/ai.test.mjs, tools/ai_profile/status.mjs, tasks/active/T0036-preserve-status-session-selection-through-ai-fac.md
  expected output: `node tools/ai.mjs status --session <id>` reads matching per-session profile logs instead of silently falling back to the active profile
  evidence command: node --test tools/ai.test.mjs; node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: profile session selection and Codex transcript import remain distinct and tested at the facade
  independent reviewer: Dalton audits next orchestration gap and facade/session semantics
- reviewer: PASS Dalton confirmed T0036 is the right current fix, review-queue
  visibility is not the highest-value gap, and the next later improvement should
  bind declared machine evidence to matching PASS evidence.
- evidence: PASS `node --test tools/ai.test.mjs` (17 tests)
- evidence: PASS `node --test tools/ai_profile/test.mjs` (29 tests)
- evidence: PASS `node tools/ai.mjs status --session 019ee5cc --no-import-codex-session` (2 session logs)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (22 subagent sessions)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS current strict `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (26 subagent sessions); refreshed after T0039 strict evidence contract.
