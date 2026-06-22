---
id: T0097
title: Clarify subagent evidence-source packet wording
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Move explicit trace/status evidence-source requirements closer to the first subagent packet example so agents do not miss source and compact artifact requirements when copying the packet.

## Done when

- [x] doc_reference_check, context budget review, taskboard validate, workflow-check T0097, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Move explicit trace/status evidence-source requirements closer to the first subagent packet example so agents do not miss source and compact artifact requirements when copying the packet.
  allowed files: docs/ai-pipeline/subagent-protocol.md;tasks/active/T*.md;tasks/evidence/T*.json;tasks/workflows/T*.json
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: Subagent protocol packet section directly tells agents that trace/status evidence commands need explicit source and json output, while docs validation and strict orchestration evidence pass.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0097-status-rollup.json --json
  stop condition: doc_reference_check, context budget review, taskboard validate, workflow-check T0097, validate --review, and strict compact status evidence pass.
  independent reviewer: Read-only reviewer verifies the packet section now makes evidence-source requirements visible without reading the closeout section.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --agent-rollup-evidence --json-output tasks\evidence\T0097-status-rollup.json --no-import-codex-session --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd`
- workflow manifest: tasks/workflows/T0097.json
- evidence: PASS `node tools/doc_reference_check.mjs`
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/taskboard/cli.mjs orchestration-workflow-check T0097 --json`
- evidence: PASS `node tools/ai.mjs validate --review`
- reviewer: PASS
  checked: packet section evidence-source wording, strict compact status artifact, workflow manifest, doc_reference_check, context budget review, taskboard validate, workflow-check T0097, validate --review.
  risks: none remaining for T0097; broader orchestration readiness still needs more real task cycles before claiming the full long-running goal complete.
  action: ready for review.
