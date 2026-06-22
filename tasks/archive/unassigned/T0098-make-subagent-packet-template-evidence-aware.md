---
id: T0098
title: Make subagent packet template evidence-aware
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, taskboard, tooling]
created: 2026-06-21
updated: 2026-06-22
---

## What

Make the reusable subagent packet template itself show explicit trace/status evidence-source and compact status artifact requirements so copied packets do not rely on nearby docs. Add a current-task mode that fills task-derived packet fields and the strict evidence command by construction.

## Done when

- [x] current subagent packet template check, focused tests, taskboard validate, workflow-check T0098, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Make the reusable subagent packet template itself show explicit trace/status evidence-source and compact status artifact requirements so copied packets do not rely on nearby docs, and add a current-task mode that fills task-derived packet fields and strict evidence command.
  allowed files: tools/taskboard/lib.mjs;tools/taskboard/cli.mjs;tools/taskboard/test.mjs;tools/ai.mjs;tools/ai.test.mjs;tasks/active/T*.md;tasks/evidence/T*.json;tasks/workflows/T*.json
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: subagent-packet-template output includes explicit trace/status evidence-source guidance; subagent-packet-template --current emits a checker-valid packet with current task fields and strict evidence command; focused tests prove the template/checker facade still pass.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0098-status-rollup.json --json
  stop condition: current subagent packet template check, focused tests, taskboard validate, workflow-check T0098, validate --review, and strict compact status evidence pass.
  independent reviewer: Read-only reviewer verifies the CLI template now makes evidence-source requirements visible without reading docs and that --current emits a checker-valid packet.
- evidence: PASS `node tools/ai.mjs subagent-packet-template --current | node tools/ai.mjs subagent-packet-check --stdin --json`
- evidence: PASS `node --test tools/taskboard/test.mjs`
- evidence: PASS `node --test tools/ai.test.mjs`
- evidence: PASS `node --test tools/taskboard/test.mjs tools/ai_profile/test.mjs`
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --agent-rollup-evidence --json-output tasks\evidence\T0098-status-rollup.json --no-import-codex-session --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd`
- evidence: PASS `node tools/ai.mjs validate --review`
- workflow manifest: tasks/workflows/T0098.json
- evidence: PASS `node tools/taskboard/cli.mjs orchestration-workflow-check T0098 --json`
- reviewer: PASS
  checked: generic subagent-packet-template evidence-source guidance, subagent-packet-template --current task-derived packet, generated packet check, strict compact status artifact, workflow manifest, taskboard validate, and validate --review.
  risks: none remaining for T0098; strict artifact still records historical non-blocking tool-use/evidence-probe failures, but current strict rollup is valid with no unresolved failures.
  action: ready for review.
