---
id: T0093
title: Add orchestration operator runbook
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Add a compact operator happy path for delegated orchestration so a lead can go from context to packet check, spawn/wait/close, integration, manifest, evidence, reviewer PASS, validation, and cleanup without assembling steps from several docs.

## Done when

- [x] Context budget review, doc reference check, taskboard validate, workflow-check T0093, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Add a compact operator happy path for delegated orchestration so a lead can go from context to packet check, spawn/wait/close, integration, manifest, evidence, reviewer PASS, validation, and cleanup without assembling steps from several docs.
  allowed files: AGENTS.md; docs/ai-pipeline/agent-workflow.md; docs/ai-pipeline/subagent-protocol.md; tasks/README.md; tasks/active/T*.md; tasks/evidence/T*.json; tasks/workflows/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Hot docs expose a short orchestration happy path and explicit completed-agent cleanup step; protocol remains the deeper source; docs stay under context budget.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 1 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0093-status-rollup.json --json
  stop condition: Context budget review, doc reference check, taskboard validate, workflow-check T0093, validate --review, and strict compact status evidence pass.
  independent reviewer: A read-only subagent verifies the runbook is short, operator-usable, and does not duplicate the full protocol.
- workflow manifest: tasks/workflows/T0093.json
- implementation: added a compact five-step delegated orchestration operator path to `docs/ai-pipeline/agent-workflow.md`, linked it from `tasks/README.md`, added explicit completed-agent cleanup guidance to the deeper subagent protocol, and compressed hot AGENTS wording without changing policy.
- evidence: PASS `node tools/context_budget.mjs`; normal hot docs 18615/26000, `AGENTS.md` 3259/3400, `docs/ai-pipeline/agent-workflow.md` 2299/2600.
- evidence: PASS `node tools/context_budget.mjs --review`; review hot docs 18615/24000.
- evidence: PASS `node tools/doc_reference_check.mjs`.
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`.
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 1 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0093-status-rollup.json --json`; compact artifact has `strict_ok: true`, `subagent_session_count: 118`, `unresolved_failed_records: 0`, and `agent_tool_usage_clean_tail_agents: 9`.
- evidence: PASS `node tools/ai.mjs validate --review`.
- reviewer: PASS
  checked: read-only reviewer confirmed the runbook is short, placed in the right hot doc, and points to the protocol instead of duplicating it; lead incorporated the wait/verify wording suggestion.
  risks: hot doc remains under budget but is still a managed hot entrypoint; future additions should move detail to the protocol.
  action: ready for T0093 review after workflow manifest check and reusable review validation pass.
