---
id: T0096
title: Harden subagent tool-use packet guard
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Make subagent packet templates and validation steer agents away from repeated tool-use failures: missing path reads, unsafe PowerShell line windows, and missing orchestration evidence source.

## Done when

- [x] Focused taskboard guard tests, taskboard validate, workflow-check T0096, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Make subagent packet templates and validation steer agents away from repeated tool-use failures: missing path reads, unsafe PowerShell line windows, and missing orchestration evidence source.
  allowed files: tools/taskboard/lib.mjs;tools/taskboard/test.mjs;docs/ai-pipeline/subagent-protocol.md;tasks/active/T*.md;tasks/evidence/T*.json;tasks/workflows/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Subagent packet template and checks encode concrete tool-use guard wording; tests prove weak/old guard wording is rejected and default templates pass; T0096 closeout has strict compact status evidence and workflow manifest.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0096-status-rollup.json --json
  stop condition: Focused taskboard guard tests, taskboard validate, workflow-check T0096, validate --review, and strict compact status evidence pass.
  independent reviewer: Read-only reviewer verifies templates reject vague guard wording and pass with concrete profiler-derived guard wording.
- evidence: PASS `node --test --test-name-pattern "tool-use guard|subagent packet|missing machine evidence source|workflow-check" tools/taskboard/test.mjs`
- evidence: PASS `node --test tools/taskboard/test.mjs`
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/doc_reference_check.mjs`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --agent-rollup-evidence --json-output tasks\evidence\T0096-status-rollup.json --no-import-codex-session --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd`
- evidence: PASS `node tools/ai.mjs validate --review`
- workflow manifest: tasks/workflows/T0096.json
- reviewer: PASS
  checked: concrete guard wording, vague guard rejection, machine evidence source checks, default packet template, workflow-check T0096, taskboard validate, and validate --review.
  risks: none remaining.
  action: ready for review.
