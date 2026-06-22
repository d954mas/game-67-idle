---
id: T0095
title: Make AI facade help tool-safe
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, profiling, tooling]
created: 2026-06-21
updated: 2026-06-22
---

## What

Make the AI facade help path agent-safe: node tools/ai.mjs --help/help/-h should print usage to stdout with exit 0 while unknown commands remain non-zero, and status evidence should stop treating normal help lookup as a failed probe.

## Done when

- [x] Focused ai facade/profile tests, taskboard validate, workflow-check T0095, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Make the AI facade help path agent-safe: node tools/ai.mjs --help/help/-h should print usage to stdout with exit 0 while unknown commands remain non-zero, and status evidence should stop treating normal help lookup as a failed probe.
  allowed files: tools/ai.mjs;tools/ai.test.mjs;tools/ai_profile/status.mjs;tools/ai_profile/test.mjs;tasks/active/T*.md;tasks/evidence/T*.json;tasks/workflows/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: AI facade help exits 0, unknown commands still fail, profiler tests cover help behavior, T0095 closeout has strict compact status evidence and workflow manifest.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0095-status-rollup.json --json
  stop condition: Focused ai facade/profile tests, taskboard validate, workflow-check T0095, validate --review, and strict compact status evidence pass.
  independent reviewer: A read-only subagent verifies help exits 0, unknown commands still fail, and profiler classification no longer encourages failed help probes.
- evidence: PASS `node --test --test-name-pattern "profile diagnostic failures|recovers single-file" tools/ai_profile/test.mjs`
- evidence: PASS `node --test --test-name-pattern "help commands|subcommand help|validate rejects" tools/ai.test.mjs`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session`
- evidence: PASS `node --test tools/ai.test.mjs`
- evidence: PASS `node --test tools/ai_profile/test.mjs`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --agent-rollup-evidence --json-output tasks\evidence\T0095-status-rollup.json --no-import-codex-session --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd`
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/ai.mjs validate --review`
- workflow manifest: tasks/workflows/T0095.json
- reviewer CONCERNS: read-only reviewer verified code/evidence PASS and flagged only task/workflow status mismatch before review transition.
- reviewer: PASS
  checked: workflow-check T0095, taskboard validate, help path, focused ai/profile tests, validate --review, and strict status artifact.
  risks: none remaining; prior status mismatch concern is resolved by advancing manifest and task status together.
  action: ready for review.
