---
id: T0087
title: Show evidence probe clean tail
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Make agent profile rollup distinguish historical evidence-probe failures from current clean behavior by reporting an evidence-probe clean tail, matching existing tool-usage clean-tail ergonomics.

## Done when

- [x] Focused ai_profile tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Make agent profile rollup distinguish historical evidence-probe failures from current clean behavior by reporting an evidence-probe clean tail, matching existing tool-usage clean-tail ergonomics.
  allowed files: tools/ai_profile/status.mjs; tools/ai_profile/test.mjs; tasks/active/T*.md; tasks/evidence/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Status JSON and text include agent_evidence_probe_clean_tail_agents when historical evidence-probe failures exist but recent agents are clean; strict semantics stay unchanged; focused tests and strict evidence pass.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0087-status-rollup.json --json
  stop condition: Focused ai_profile tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.
  independent reviewer: A subagent verifies evidence-probe clean-tail output is accurate and does not weaken strict unresolved/evidence-probe classification.
- implementation: added `agent_evidence_probe_clean_tail_agents` to full status rollup and compact status evidence; text output now reports `agent evidence-probe clean tail: N agent(s)` when historical evidence-probe failures exist and the recent agent tail is clean.
- reviewer: PASS read-only subagent review confirmed strict semantics stayed unchanged and identified compact evidence coverage; compact evidence was updated to include evidence-probe failed records and clean tail.
- evidence: PASS `node --test --test-name-pattern "evidence-probe|compact evidence|clean tail" tools/ai_profile/test.mjs` (5/5 tests).
- evidence: PASS `node --test tools/ai_profile/test.mjs` (59/59 tests).
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`.
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0087-status-rollup.json --json`; compact artifact has `strict_ok: true`, `agent_evidence_probe_failed_records: 10`, and `agent_evidence_probe_clean_tail_agents: 3`.
- evidence: PASS `node tools/ai.mjs validate --review`.
