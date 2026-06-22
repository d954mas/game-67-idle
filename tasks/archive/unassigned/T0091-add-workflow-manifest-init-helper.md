---
id: T0091
title: Add workflow manifest init helper
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Add an operator-facing workflow manifest init helper so current orchestration tasks can create a repo-local starter manifest from the task packet and evidence command instead of hand-assembling tasks/workflows/T*.json from memory.

## Done when

- [x] Focused taskboard/ai facade tests, taskboard validate, orchestration-check --current, workflow-check T0091, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Add an operator-facing workflow manifest init helper so current orchestration tasks can create a repo-local starter manifest from the task packet and evidence command instead of hand-assembling tasks/workflows/T*.json from memory.
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/cli.mjs; tools/taskboard/test.mjs; tools/ai.mjs; tools/ai.test.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md; tasks/evidence/T*.json; tasks/workflows/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Taskboard and ai facade expose an orchestration-workflow-init command for --current or task selectors; tests cover dry-run, write, existing-file refusal, and generated manifest linkage; protocol documents init as the starter path before closeout edits.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0091-status-rollup.json --json
  stop condition: Focused taskboard/ai facade tests, taskboard validate, orchestration-check --current, workflow-check T0091, validate --review, and strict compact status evidence pass.
  independent reviewer: A read-only subagent verifies workflow manifest init reduces manual closeout assembly without bypassing manifest validation or overwriting existing artifacts.
- workflow manifest: tasks/workflows/T0091.json
- implementation: added `orchestration-workflow-init` to taskboard and `tools/ai.mjs`; it derives a starter manifest from a task packet, refuses existing files unless `--force` is passed, supports `--current` and task selectors, and can emit closeout-shaped `--status review --packet-status integrated` manifests.
- evidence: PASS `node --test --test-name-pattern "workflow init|orchestration-workflow-init|workflow-check while doing" tools/taskboard/test.mjs` (4/4 tests).
- evidence: PASS `node --test --test-name-pattern "orchestration-workflow-init" tools/ai.test.mjs` (1/1 test).
- evidence: PASS `node --test tools/taskboard/test.mjs` (172/172 tests from read-only verifier).
- evidence: PASS `node --test tools/ai.test.mjs` (33/33 tests from read-only verifier).
- evidence: PASS `node tools\taskboard\cli.mjs orchestration-workflow-check T0091 --json` after starter manifest/log marker was present.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0091-status-rollup.json --json`; compact artifact has `strict_ok: true`, `subagent_session_count: 113`, `unresolved_failed_records: 0`, and `agent_tool_usage_clean_tail_agents: 4`.
- reviewer: PASS
  checked: read-only subagent confirmed init/dry-run/write/refuse-existing behavior, ai facade forwarding, docs, and fit with existing manifest validation; lead resolved the expected early workflow-check failures by making the starter manifest pass before strict evidence.
  risks: accepted that closeout-shaped manifests still need the lead to update/rewrite status and packet status before review; helper reduces manual assembly but does not replace final workflow-check validation.
  action: ready for T0091 review after closeout-shaped workflow-check, taskboard validate, and reusable review validation pass.
