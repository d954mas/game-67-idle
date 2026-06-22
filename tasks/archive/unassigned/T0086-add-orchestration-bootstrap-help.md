---
id: T0086
title: Add orchestration bootstrap help
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Make orchestration-bootstrap discoverable from the operator path by returning concise usage/help instead of a missing-arguments error when called with --help.

## Done when

- [x] Focused ai/taskboard tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Make orchestration-bootstrap discoverable from the operator path by returning concise usage/help instead of a missing-arguments error when called with --help.
  allowed files: tools/taskboard/cli.mjs; tools/taskboard/test.mjs; tools/ai.test.mjs; tasks/active/T*.md; tasks/evidence/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: orchestration-bootstrap --help succeeds with usage text through taskboard and ai facade; missing-argument failures remain unchanged without --help; focused tests and strict evidence pass.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0086-status-rollup.json --json
  stop condition: Focused ai/taskboard tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.
  independent reviewer: A subagent verifies help behavior is concise, exits successfully, and does not weaken missing-argument validation.
- implementation: `orchestration-bootstrap --help` now prints concise taskboard-owned usage before missing-argument validation, and the ai facade forwards that same usage.
- implementation: Tests cover direct taskboard help, facade help pass-through, and unchanged missing-argument JSON failures.
- reviewer: Socrates the 2nd verified direct/facade help exit 0 and missing `--objective` without help still returns `missing_required_argument`.
- evidence: PASS node tools/ai.mjs orchestration-bootstrap --help
- evidence: PASS node tools/taskboard/cli.mjs orchestration-bootstrap --help
- evidence: PASS node --test --test-name-pattern "orchestration-bootstrap" tools/ai.test.mjs tools/taskboard/test.mjs
- evidence: PASS node --test tools/ai.test.mjs
- evidence: PASS node --test tools/taskboard/test.mjs
- evidence: PASS node tools/taskboard/cli.mjs validate --json
- evidence: PASS node tools/ai.mjs orchestration-check --current --json
- evidence: PASS node tools/ai.mjs validate --review
- evidence: PASS node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0086-status-rollup.json --json
- evidence: PASS tasks/evidence/T0086-status-rollup.json
