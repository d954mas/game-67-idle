---
id: T0044
title: Normalize subagent transcript failure classification
status: done
epic: ""
priority: P1
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Subagent transcript telemetry and Codex session recovery currently treat every
nonzero shell exit as a failed command. The hook hot path already treats search
command exit 1 as normal no-match. Mirror that rule in transcript-derived
telemetry so failure counts are not inflated by `rg` or `Select-String` queries
that simply found no matches.

## Done when

- [x] Transcript fallback treats search command exit 1 as PASS.
- [x] Transcript fallback still treats real search errors, such as exit 2, as
  FAIL.
- [x] Codex session recovery does not import search no-match as a recovered
  failure.
- [x] Current live agent status shows reduced unresolved agent failures after
  normalization.
- [x] Independent reviewer confirms semantics and risk.
- [x] Focused tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: align subagent transcript failure classification with hook search no-match semantics
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/hook_record.mjs, tools/ai_profile/test.mjs, tasks/active/T0044-normalize-subagent-transcript-failure-classifica.md
  expected output: agent transcript telemetry no longer counts `rg`/`Select-String` exit 1 as unresolved failures while preserving real search errors
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove search no-match behavior and live status shows reduced agent failure noise
  independent reviewer: Goodall audits transcript classification parity with hook semantics
- evidence: PASS `node --test tools/ai_profile/test.mjs` (33 tests)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (31 subagent sessions; unresolved agent failures reduced from 26 to 11)
- reviewer: PASS Goodall confirmed hook parity: search exit 1 is normal
  no-match, exit 2 and non-search nonzero remain failures; noted recovery path
  also needed normalization.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (34 tests)
- evidence: PASS `node --test tools/ai_profile/test.mjs tools/ai.test.mjs` (51 tests)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `git diff --check`
