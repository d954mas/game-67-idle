---
id: T0082
title: Harden subagent tool-use packet guard
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Move repeated subagent tool-use prevention hints from profiler evidence into reusable orchestration packet guidance and validation so new delegated work avoids missing-path reads, unsupported PowerShell commands, and missing evidence-source probes.

## Done when

- [x] Focused taskboard/profile/facade tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence all pass.

## Open questions

## Log

- orchestration: used
  objective: Move repeated subagent tool-use prevention hints from profiler evidence into reusable orchestration packet guidance and validation so new delegated work avoids missing-path reads, unsupported PowerShell commands, and missing evidence-source probes.
  allowed files: docs/ai-pipeline/subagent-protocol.md; tools/taskboard/**; tools/ai_profile/**; tools/ai.test.mjs; tasks/active/T*.md; tasks/evidence/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Subagent orchestration packets and/or validators carry concrete reusable guard text for exact path discovery, safe line windows, and safe orchestration evidence collection; focused tests and strict compact status evidence pass.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0082-status-rollup.json --json
  stop condition: Focused taskboard/profile/facade tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence all pass.
  independent reviewer: At least one subagent reviews the packet guard wording and fail-closed coverage before closeout.

- implementation: moved repeated profiler prevention hints into the taskboard orchestration packet contract for T0082+ by requiring detailed `tool-use guard` buckets: path discovery/read safety, safe `Select-Object -Skip/-First` line windows, and evidence source plus `--json-output`.
- review: PASS subagent verifier recommended taskboard validation over profiler-only advice, semantic bucket checks instead of exact prose, and T0082+ cutoff to avoid overfitting legacy packets.
- evidence: PASS `node --test tools/taskboard/test.mjs` (136/136)
- evidence: PASS `node --test tools/ai_profile/test.mjs` (56/56)
- evidence: PASS `node --test tools/ai.test.mjs` (27/27)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`
- evidence: PASS `node tools/ai.mjs orchestration-evidence --current --run --json` wrote `tasks/evidence/T0082-status-rollup.json` with strict status evidence.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0082-status-rollup.json --json`; compact artifact `tasks/evidence/T0082-status-rollup.json` has `strict_ok: true`.
