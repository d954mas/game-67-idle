---
id: T0088
title: Require structured verifier evidence
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Make new substantial orchestration closeout require a structured reviewer/verifier result instead of accepting only a non-empty independent reviewer plan.

## Done when

- [x] Focused taskboard tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Make new substantial orchestration closeout require a structured reviewer/verifier result instead of accepting only a non-empty independent reviewer plan.
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/test.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md; tasks/evidence/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: T0088+ review/done orchestration tasks must include a structured reviewer PASS log entry after implementation evidence; docs and tests describe the verifier evidence shape; existing older tasks remain compatible.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0088-status-rollup.json --json
  stop condition: Focused taskboard tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence pass.
  independent reviewer: A subagent verifies the reviewer evidence validator rejects missing/weak verifier results without breaking older tasks or preflight-only doing tasks.
- implementation: added a T0088+ closeout guard that requires `- reviewer: PASS` or `- verifier: PASS` after machine evidence PASS, with structured `checked`, `risks`, and `action` fields; older T0087 and earlier review tasks remain compatible.
- evidence: PASS `node --test --test-name-pattern "T0088|T0087 compatibility|reviewer pass" tools/taskboard/test.mjs` (7/7 tests).
- evidence: PASS `node --test tools/taskboard/test.mjs` (144/144 tests).
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`.
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0088-status-rollup.json --json`; compact artifact has `strict_ok: true`, `subagent_session_count: 107`, and `agent_evidence_probe_clean_tail_agents: 4`.
- reviewer: PASS
  checked: read-only subagent reviewed validator placement, T0088+ cutoff, preflight-only behavior, and compatibility risk for T0030-T0087.
  risks: accepted stricter closeout semantics than presence-only reviewer result; CONCERNS/FAIL may be logged but do not satisfy review/done closeout.
  action: ready for T0088 review after gates and strict compact evidence pass.
- evidence: PASS `node tools/ai.mjs validate --review`.
