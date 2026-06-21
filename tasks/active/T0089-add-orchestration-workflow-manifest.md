---
id: T0089
title: Add orchestration workflow manifest
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Introduce a durable workflow state manifest for substantial orchestration work so the workflow scope, packets, budgets, verification, integration policy, and status can be inspected outside chat/task prose.

## Done when

- [x] Focused taskboard tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Introduce a durable workflow state manifest for substantial orchestration work so the workflow scope, packets, budgets, verification, integration policy, and status can be inspected outside chat/task prose.
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/cli.mjs; tools/taskboard/test.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md; tasks/evidence/T*.json; tasks/workflows/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: A taskboard workflow-template command emits a repo-local JSON manifest shape; T0089+ review/done orchestration tasks require a valid workflow manifest artifact referenced in the task log; focused tests cover missing/invalid/valid manifests and old-task compatibility.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0089-status-rollup.json --json
  stop condition: Focused taskboard tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.
  independent reviewer: A subagent verifies the workflow manifest schema is useful, bounded, backwards-compatible, and not overfitted to T0089 only.
- implementation: added `orchestration-workflow-template` and `orchestration-workflow-check`, plus a T0089+ closeout guard that validates repo-local workflow manifests with task/status matching, bounded packet files, terminal packet statuses, duplicate-id rejection, verification/integration fields, and evidence refs tied to the declared machine artifact.
- workflow manifest: tasks/workflows/T0089.json
- evidence: PASS `node --test --test-name-pattern "workflow manifest|workflow-template|workflow-check|T0089|T0088 compatibility" tools/taskboard/test.mjs` (14/14 tests).
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`.
- evidence: PASS `node tools/ai.mjs orchestration-evidence --current --run --json`; compact artifact `tasks/evidence/T0089-status-rollup.json` has `strict_ok: true`, `subagent_session_count: 111`, and `unresolved_failed_records: 0`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0089-status-rollup.json --json`.
- reviewer: PASS
  checked: read-only subagent reviewed manifest schema/template/docs gaps; lead addressed bounded packet files, evidence-ref linkage, workflow-check, path task matching, docs, and closeout artifact coverage.
  risks: accepted residual historical profile friction remains visible in compact evidence (`agent_tool_usage_failed_records: 17`, clean tail 2 after stronger reviewer packet); future packets must keep the full tool-use guard.
  action: ready for T0089 review after full taskboard validation and reusable pipeline review validation pass.
