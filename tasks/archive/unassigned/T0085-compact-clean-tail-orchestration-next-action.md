---
id: T0085
title: Compact clean-tail orchestration next action
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Make clean-tail status next action concise and operator-safe when no current orchestration task exists, avoiding long placeholder bootstrap commands in profiler output.

## Done when

- [x] Focused ai_profile tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Make clean-tail status next action concise and operator-safe when no current orchestration task exists, avoiding long placeholder bootstrap commands in profiler output.
  allowed files: tools/ai_profile/status.mjs; tools/ai_profile/test.mjs; tasks/active/T*.md; tasks/evidence/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Status next action points to concise orchestration-bootstrap/help and orchestration-check flow without embedding a long placeholder command; tests and strict evidence pass.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0085-status-rollup.json --json
  stop condition: Focused ai_profile tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.
  independent reviewer: A subagent verifies the next-action wording stays actionable while avoiding hot status bloat and preserving current-task preflight behavior.
- implementation: The zero-current-task clean-tail status guidance now names `orchestration-bootstrap` and bounded packet fields without embedding a long placeholder command or fake parent id.
- implementation: Focused next-action tests now assert no-current, one-current, multiple-current, and non-current task behavior without reintroducing `--tool-use-guard` or placeholder-heavy output.
- reviewer: Arendt the 2nd verified the compact next-action change is bounded, preserves current-task preflight behavior, and keeps ambiguity handling intact.
- evidence: PASS node --test --test-name-pattern "clean-tail next action" tools/ai_profile/test.mjs
- evidence: PASS node --test tools/ai_profile/test.mjs
- evidence: PASS node tools/taskboard/cli.mjs validate --json
- evidence: PASS node tools/ai.mjs orchestration-check --current --json
- evidence: PASS node tools/context_budget.mjs --review
- evidence: PASS node tools/doc_reference_check.mjs
- evidence: PASS node tools/ai.mjs validate --review
- evidence: PASS node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0085-status-rollup.json --json
- evidence: PASS tasks/evidence/T0085-status-rollup.json
