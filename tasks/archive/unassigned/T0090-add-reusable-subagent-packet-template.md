---
id: T0090
title: Add reusable subagent packet template
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Add a reusable operator-facing subagent packet template/check path so manual delegated prompts consistently include the known tool-use guard, bounded scope, evidence, handoff fields, and closeout expectations instead of relying on memory.

## Done when

- [x] Focused taskboard/ai facade tests, taskboard validate, orchestration-check --current, workflow-check T0090, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Add a reusable operator-facing subagent packet template/check path so manual delegated prompts consistently include the known tool-use guard, bounded scope, evidence, handoff fields, and closeout expectations instead of relying on memory.
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/cli.mjs; tools/taskboard/test.mjs; tools/ai.mjs; tools/ai.test.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md; tasks/evidence/T*.json; tasks/workflows/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Taskboard and ai facade expose a subagent-packet template/check command; tests cover template contents and invalid/missing packet fields; protocol points operators to the reusable packet before spawn_agent; T0090 closeout records strict compact evidence and workflow manifest.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0090-status-rollup.json --json
  stop condition: Focused taskboard/ai facade tests, taskboard validate, orchestration-check --current, workflow-check T0090, validate --review, and strict compact status evidence pass.
  independent reviewer: A read-only subagent verifies the packet template reduces known tool-use failure modes without duplicating or weakening existing orchestration task packet guards.
- implementation: added reusable prompt-level `subagent-packet-template` / `subagent-packet-check` commands plus short aliases `subagent-template` / `subagent-check` in taskboard and `tools/ai.mjs`; the check validates bounded allowed files, detailed tool-use guard text, evidence/stop fields, and handoff subfields.
- workflow manifest: tasks/workflows/T0090.json
- evidence: PASS `node --test --test-name-pattern "subagent packet|subagent-packet|subagent-template|subagent-check" tools/taskboard/test.mjs` (10/10 tests).
- evidence: PASS `node --test --test-name-pattern "subagent-packet|subagent-template|subagent-check" tools/ai.test.mjs` (4/4 tests).
- evidence: PASS `node --test tools/taskboard/test.mjs` (168/168 tests).
- evidence: PASS `node --test tools/ai.test.mjs` (32/32 tests).
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`.
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0090-status-rollup.json --json`; compact artifact has `strict_ok: true`, `subagent_session_count: 112`, `agent_tool_usage_clean_tail_agents: 3`, and `agent_evidence_probe_clean_tail_agents: 9`.
- reviewer: PASS
  checked: read-only subagent confirmed the command should be separate from `orchestration-template`; lead fixed the packet parser bug, added aliases, focused tests, ai facade forwarding, and protocol docs.
  risks: accepted that `subagent-packet-check` is a friction-reduction helper, not a mandatory spawn gate; operators can still bypass it in raw prompts, so protocol points to it before manual `spawn_agent`.
  action: ready for T0090 review after workflow manifest check and reusable review validation pass.
