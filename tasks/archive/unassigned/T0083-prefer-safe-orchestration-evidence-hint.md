---
id: T0083
title: Prefer safe orchestration evidence hint
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Align profiler prevention hints for missing orchestration evidence source with the safe orchestration-evidence wrapper so future agents use the task-scoped dry-run/run path before falling back to raw orchestration-trace commands.

## Done when

- [x] Focused ai_profile/taskboard/facade tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Align profiler prevention hints for missing orchestration evidence source with the safe orchestration-evidence wrapper so future agents use the task-scoped dry-run/run path before falling back to raw orchestration-trace commands.
  allowed files: tools/ai_profile/status.mjs; tools/ai_profile/test.mjs; tools/taskboard/**; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md; tasks/evidence/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Profiler missing-evidence-source hints prefer node tools/ai.mjs orchestration-evidence --current --run --json and retain raw trace command only as lower-level fallback; focused tests and strict compact status evidence pass.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0083-status-rollup.json --json
  stop condition: Focused ai_profile/taskboard/facade tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.
  independent reviewer: A subagent verifies the hint wording does not regress raw trace fallback or taskboard guard alignment.

- implementation: profiler prevention hints for missing orchestration evidence source now prefer `node tools/ai.mjs orchestration-evidence --current --run --json` and retain raw `orchestration-trace` with source/json-output as fallback.
- review: PASS subagent verifier found no blocker; added requested JSON-payload assertion and `isMachineEvidenceCommand` regression for the raw fallback command.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (56/56)
- evidence: PASS `node --test tools/ai.test.mjs` (27/27)
- evidence: PASS `node --test tools/taskboard/test.mjs` (136/136)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`
- evidence: PASS `node tools/ai.mjs orchestration-evidence --current --run --json` wrote `tasks/evidence/T0083-status-rollup.json` with `strict_ok: true`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0083-status-rollup.json --json`; compact artifact `tasks/evidence/T0083-status-rollup.json` has `strict_ok: true`.
