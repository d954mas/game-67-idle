---
id: T0092
title: Add stdin subagent packet check
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Add stdin support for subagent packet validation so operators can pipe or here-string multiline spawn_agent packets through the checker without fragile command-line quoting.

## Done when

- [x] Focused taskboard/ai facade tests, taskboard validate, orchestration-check --current, workflow-check T0092, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Add stdin support for subagent packet validation so operators can pipe or here-string multiline spawn_agent packets through the checker without fragile command-line quoting.
  allowed files: tools/taskboard/cli.mjs; tools/taskboard/test.mjs; tools/ai.mjs; tools/ai.test.mjs; tools/ai_profile/status.mjs; tools/ai_profile/test.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md; tasks/evidence/T*.json; tasks/workflows/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Taskboard and ai facade accept subagent-packet-check/subagent-check --stdin; focused tests cover valid stdin packets, stdin/text/file exclusivity, and docs show piping/here-string usage.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0092-status-rollup.json --json
  stop condition: Focused taskboard/ai facade tests, taskboard validate, orchestration-check --current, workflow-check T0092, validate --review, and strict compact status evidence pass.
  independent reviewer: A read-only subagent verifies stdin support improves packet-check ergonomics without weakening file/text validation or introducing ambiguous input handling.
- workflow manifest: tasks/workflows/T0092.json
- implementation: added `--stdin` input for `subagent-packet-check` / `subagent-check`, kept `--file` / `--text` mutually exclusive with stdin, updated the AI facade usage, and documented a PowerShell here-string packet check.
- implementation: classified failed structured validator probes from subagent transcripts as evidence probes instead of unresolved agent failures, so read-only reviewers can verify fail-closed behavior without poisoning strict rollup closeout.
- evidence: PASS `node --test --test-name-pattern "subagent packet|subagent-packet|subagent-template|subagent-check" tools/taskboard/test.mjs` (12/12 tests).
- evidence: PASS `node --test --test-name-pattern "subagent packet|subagent-packet|subagent-template|subagent-check" tools/taskboard/test.mjs` (13/13 tests after alias stdin coverage).
- evidence: PASS `node --test --test-name-pattern "subagent-packet|subagent-template|subagent-check" tools/ai.test.mjs` (8/8 tests).
- evidence: PASS `node --test --test-name-pattern "structured validator probes|strict status agent rollup ignores failed diagnostic strict-status probes|strict status agent rollup classifies profile diagnostic failures outside unresolved" tools/ai_profile/test.mjs` (3/3 tests).
- evidence: PASS PowerShell here-string piped to `node tools/ai.mjs subagent-packet-check --stdin --json`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0092-status-rollup.json --json`; compact artifact has `strict_ok: true`, `subagent_session_count: 117`, `unresolved_failed_records: 0`, and `agent_tool_usage_clean_tail_agents: 8`.
- reviewer: PASS
  checked: read-only reviewer raised missing alias stdin tests, facade stdin/file exclusivity, and closeout evidence; lead added the coverage, fixed structured validator probe classification, and reran focused tests plus strict status evidence.
  risks: accepted that structured validator probe failures count as evidence probes only for known taskboard/AI facade validator commands with structured problem output; real failed tests still remain unresolved.
  action: ready for T0092 review after workflow manifest check, taskboard validate, and reusable review validation pass.
