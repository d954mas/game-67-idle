---
id: T0081
title: Add safe orchestration evidence wrapper
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, taskboard, profiling]
created: 2026-06-21
updated: 2026-06-22
---

## What

Add a safe command that discovers the current orchestration task and assembles or runs strict compact status rollup evidence without requiring operators to manually provide parent thread id, session root, agent cwd, evidence path, min agents, or --agent-rollup-evidence flags.

## Done when

- [x] Focused ai facade/profile tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence all pass; wrapper output is exercised in tests and task log records subagent review.

## Open questions

## Log

- orchestration: used
  objective: Add a safe command that discovers the current orchestration task and assembles or runs strict compact status rollup evidence without requiring operators to manually provide parent thread id, session root, agent cwd, evidence path, min agents, or --agent-rollup-evidence flags.
  allowed files: tools/ai.mjs; tools/ai.test.mjs; tools/ai_profile/**; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md; tasks/evidence/T*.json
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: A tested facade command emits and runs a task-scoped compact status evidence command for the current orchestration task, fails with structured remediation when source inference is ambiguous, and docs route closeout through the safe wrapper.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0081-status-rollup.json --json
  stop condition: Focused ai facade/profile tests, taskboard validate, orchestration-check --current, validate --review, and strict compact status evidence all pass; wrapper output is exercised in tests and task log records subagent review.
  independent reviewer: two subagents inspect source inference, operator UX, and failure modes before closeout; parent integrates findings

- implementation: added `node tools/ai.mjs orchestration-evidence [--current|--task|--id|--file] [--run] [--json]` as the safe operator path for strict compact status evidence. Dry-run prints the resolved command and `--run` writes the task-scoped compact artifact.
- review: PASS subagent source/UX reviews integrated; follow-up verifier found P1 missing-task fail-open, fixed by requiring explicit `--task/--id` selectors to resolve an existing task file.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (56/56)
- evidence: PASS `node --test tools/ai.test.mjs` (27/27)
- evidence: PASS `node tools/ai.mjs orchestration-evidence --current --run --json` wrote `tasks/evidence/T0081-status-rollup.json` with `strict_ok: true`, `subagent_session_count: 95`, missing telemetry 0, unresolved failures 0.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0081-status-rollup.json --json`; compact artifact `tasks/evidence/T0081-status-rollup.json` has `strict_ok: true`.
