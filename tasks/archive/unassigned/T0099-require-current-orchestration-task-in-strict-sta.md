---
id: T0099
title: Require current orchestration task in strict status
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-22
updated: 2026-06-22
---

## What

Add an explicit strict status option that fails closed unless exactly one current doing pipeline/orchestration task exists and passes orchestration preflight, so green agent rollup cannot be mistaken for launch readiness.

## Done when

- [ ] Stop after the strict status current-task gate is tested, documented by help/tests, and task closeout evidence is recorded; do not touch gameplay/runtime.

## Open questions

## Log

- orchestration: used
  objective: Add an explicit strict status option that fails closed unless exactly one current doing pipeline/orchestration task exists and passes orchestration preflight, so green agent rollup cannot be mistaken for launch readiness.
  allowed files: tools/ai_profile/status.mjs;tools/ai_profile/test.mjs;tools/ai.mjs;tools/ai.test.mjs;tasks/active/T*.md;tasks/evidence/T*.json;tasks/workflows/T*.json
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: status supports a current-task readiness gate; focused tests cover missing/current/invalid preflight paths; strict evidence and workflow manifest pass
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --require-current-orchestration-task --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0099-status-rollup.json --json
  stop condition: Stop after the strict status current-task gate is tested, documented by help/tests, and task closeout evidence is recorded; do not touch gameplay/runtime.
  independent reviewer: A read-only subagent verifies the new status gate fails without current task, passes with valid current preflight, and preserves existing status behavior unless the new flag is set.
- implementation: Added opt-in `--require-current-orchestration-task` / `--require-current-orchestration-preflight` status readiness gate, compact current-task evidence, taskboard artifact validation for declared current-task status evidence, facade help, and regression tests. Gameplay/runtime files were not touched.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --require-current-orchestration-task --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0099-status-rollup.json --json`; compact artifact has `agent_rollup.strict_ok: true`, `current_orchestration_task.ok: true`, and repo-local current task file.
- evidence: PASS `node --test tools/ai.test.mjs tools/ai_profile/test.mjs tools/taskboard/test.mjs` (290/290).
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`.
- evidence: PASS `node tools/ai.mjs validate --review`.
- workflow manifest: tasks/workflows/T0099.json
- reviewer: PASS
  checked: old strict status evidence compatibility, new current-task evidence artifact rejection for missing/failing/absolute-path current task data, sanitized T0099 compact artifact, current task lookup through taskboard root, facade help alias visibility, and no gameplay/runtime file changes.
  risks: historical subagent tool-use/evidence-probe failures remain visible in rollup but strict unresolved failures are zero; future launch-readiness commands should include the new current-task flag when a green status is meant to imply orchestration readiness.
  action: Use `--require-current-orchestration-task` or `--require-current-orchestration-preflight` for strict orchestration launch-readiness evidence; continue using plain `--require-agent-rollup-ok` for diagnostic/legacy strict rollup evidence.
