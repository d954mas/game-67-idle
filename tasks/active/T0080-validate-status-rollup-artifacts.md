---
id: T0080
title: Validate status rollup artifacts
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, taskboard, profiling]
created: 2026-06-21
updated: 2026-06-21
---

## What

Make T0080+ strict status --agent-rollup taskboard evidence artifact-backed by requiring --json-output, validating the declared repo-local status JSON, and rejecting stale, missing, failing, source-mismatched, or count-mismatched status artifacts while preserving older task compatibility.

## Done when

- [x] Focused taskboard tests, taskboard validate, orchestration-check --current, validate --review, and strict status artifact evidence all pass; task log records subagent review and artifact-backed evidence.

## Open questions

## Log

- orchestration: used
  objective: Make T0080+ strict status --agent-rollup taskboard evidence artifact-backed by requiring --json-output, validating the declared repo-local status JSON, and rejecting stale, missing, failing, source-mismatched, or count-mismatched status artifacts while preserving older task compatibility.
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/test.mjs; tools/ai_profile/status.mjs; tools/ai_profile/test.mjs; tools/ai.mjs; tools/ai.test.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md; tasks/evidence/T*.json
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: Taskboard tests reject T0080+ status-agent-rollup evidence without json-output or with missing/failing/source-mismatched artifacts, accept matching strict status artifacts, preserve pre-T0080 compatibility, and document the new contract.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0080-status-rollup.json --json
  stop condition: Focused taskboard tests, taskboard validate, orchestration-check --current, validate --review, and strict status artifact evidence all pass; task log records subagent review and artifact-backed evidence.
  independent reviewer: two subagents inspect status artifact contract, parser compatibility, and artifact-size/privacy risks before closeout; parent integrates findings
- review: Planck found missing top-level validity checks, dual-source matching risk, and stale/raw artifact gaps. Franklin found raw status JSON is too large/private for committed task evidence and `tmp/` is not durable enough.
- change: Added compact `--agent-rollup-evidence` status JSON, T0080+ taskboard artifact validation for compact status rollups, top-level validity/profile-rollup checks, dual-source matching, raw-status rejection, and profile diagnostic failure classification outside unresolved failures.
- evidence: PASS `node --test tools/taskboard/test.mjs` (132/132)
- evidence: PASS `node --test tools/ai_profile/test.mjs` (52/52)
- evidence: PASS `node --test tools/ai.test.mjs` (26/26)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0080-status-rollup.json --json`
