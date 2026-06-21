---
id: T0084
title: Clarify classified evidence probe status output
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Make status output distinguish classified agent evidence-probe failures from real unresolved agent failures so strict-green orchestration rollups do not still show misleading unresolved sample labels.

## Done when

- [x] Focused ai_profile tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Make status output distinguish classified agent evidence-probe failures from real unresolved agent failures so strict-green orchestration rollups do not still show misleading unresolved sample labels.
  allowed files: tools/ai_profile/status.mjs; tools/ai_profile/test.mjs; tasks/active/T*.md; tasks/evidence/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Classified evidence-probe samples render with a non-unresolved label while strict unresolved failure semantics remain unchanged; focused tests and strict compact status evidence pass.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0084-status-rollup.json --json
  stop condition: Focused ai_profile tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.
  independent reviewer: A subagent verifies the wording change does not hide real unresolved failures or weaken strict status semantics.
- implementation: Classified agent tool-usage and evidence-probe samples now carry the same inferred reason used by the rollup buckets; evidence-probe samples render as `evidence-probe` instead of `unresolved`.
- implementation: Hot operator paths now expose `orchestration-check --current --json` and `orchestration-evidence --current --run --json`.
- reviewer: Avicenna found the profile-sourced reason propagation gap before fix; Socrates read-only verification requested after fix and hot-doc update.
- evidence: PASS node --test tools/ai_profile/test.mjs
- evidence: PASS node --test tools/ai.test.mjs tools/ai_profile/test.mjs
- evidence: PASS node tools/taskboard/cli.mjs validate --json
- evidence: PASS node tools/ai.mjs orchestration-check --current --json
- evidence: PASS node tools/context_budget.mjs --review
- evidence: PASS node tools/doc_reference_check.mjs
- evidence: PASS node tools/ai.mjs validate --review
- evidence: PASS node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0084-status-rollup.json --json
- evidence: PASS tasks/evidence/T0084-status-rollup.json
