---
id: T0079
title: Harden orchestration lifecycle and classification
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, taskboard]
created: 2026-06-21
updated: 2026-06-22
---

## What

Make new substantial pipeline/orchestration work harder to bypass by requiring preflight fields at review/done closeout for T0079+ tasks and broadening substantial-work classification toward the documented game/visual/asset/review/runtime domains without touching game implementation.

## Done when

- [x] Focused taskboard tests cover lifecycle bypass, broad classification, compatibility, and small-scope exceptions; taskboard validate, orchestration-check --current, validate --review, and strict agent rollup pass.

## Open questions

## Log

- orchestration: used
  objective: Make new substantial pipeline/orchestration work harder to bypass by requiring preflight fields at review/done closeout for T0079+ tasks and broadening substantial-work classification toward the documented game/visual/asset/review/runtime domains without touching game implementation.
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/test.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: Taskboard tests and validators reject T0079+ substantial tasks that skip preflight fields at review/done, classify representative broad substantial work domains for orchestration guards, preserve legacy compatibility, and keep small-scope exceptions working.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose
  stop condition: Focused taskboard tests cover lifecycle bypass, broad classification, compatibility, and small-scope exceptions; taskboard validate, orchestration-check --current, validate --review, and strict agent rollup pass.
  independent reviewer: two subagents inspect lifecycle/classification scope and compatibility before closeout; parent integrates findings
- subagent review: Volta warned against standalone broad words such as review/runtime/asset/game/visual because body text would false-positive; implemented T0079+ paired domain+scope cues and negative small-gameplay coverage.
- subagent review: Boyle found write-path bypasses in createTask review/done and body-only updateDoc for existing review/done tasks; implemented T0079+ hard checks for create, direct transition, and body-only closeout edits.
- implementation: T0079+ review/done closeout now requires the full preflight field set, including tool-use guard, while pre-T0079 compatibility remains covered.
- implementation: T0079+ broad substantial-work classification now covers documented game/visual/asset/review/runtime/product/DevAPI workflow domains only through conservative paired cues or explicit standalone workflow phrases.
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json` (ok true)
- evidence: PASS `node --test tools/taskboard/test.mjs` (119/119)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json` (ok true)
- evidence: PASS `node tools/doc_reference_check.mjs` (121 markdown files)
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose` (88/88 telemetry agents; strict command exit 0)
- evidence: PASS `git diff --check` (only global git ignore permission warning)
