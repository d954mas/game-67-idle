---
id: T0094
title: Fix orchestration evidence closeout contract
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Make strict status orchestration evidence closeout-valid from bootstrap through wrapper output: require compact artifact flags for status evidence, print a validator-accepted PASS command line, and test bootstrap-to-closeout guidance so regular operators do not create tasks that later require command rewrites.

## Done when

- [x] Focused taskboard/ai_profile/ai facade tests, taskboard validate, workflow-check T0094, validate --review, and strict compact status evidence pass.

## Open questions

## Log

- orchestration: used
  objective: Make strict status orchestration evidence closeout-valid from bootstrap through wrapper output: require compact artifact flags for status evidence, print a validator-accepted PASS command line, and test bootstrap-to-closeout guidance so regular operators do not create tasks that later require command rewrites.
  allowed files: tools/taskboard/cli.mjs; tools/taskboard/lib.mjs; tools/taskboard/test.mjs; tools/ai_profile/orchestration_evidence.mjs; tools/ai_profile/status.mjs; tools/ai_profile/test.mjs; tools/ai.test.mjs; AI_PIPELINE.md; docs/ai-pipeline/subagent-protocol.md; tasks/README.md; tasks/active/T*.md; tasks/evidence/T*.json; tasks/workflows/T*.json
  tool-use guard: verify paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence or trace/status commands with evidence source and --json-output
  expected output: Bootstrap rejects status evidence commands missing --agent-rollup-evidence/--json-output, orchestration-evidence prints a full machine-command PASS line, tests prove advised PASS closeout validates, and docs stay aligned.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0094-status-rollup.json --json
  stop condition: Focused taskboard/ai_profile/ai facade tests, taskboard validate, workflow-check T0094, validate --review, and strict compact status evidence pass.
  independent reviewer: A read-only subagent verifies evidence command guidance is closeout-valid and no longer prints artifact-only PASS instructions.
- evidence: PASS `node --test --test-name-pattern "orchestration-bootstrap|orchestration evidence" tools/taskboard/test.mjs`
- evidence: PASS `node --test --test-name-pattern "orchestration-bootstrap" tools/ai.test.mjs`
- evidence: PASS `node --test --test-name-pattern "orchestration evidence" tools/ai_profile/test.mjs`
- evidence: PASS `node --test --test-name-pattern "workflow-check alias|structured validator probes" tools/taskboard/test.mjs tools/ai_profile/test.mjs`
- evidence: PASS `node --test tools/taskboard/test.mjs`
- evidence: PASS `node --test tools/ai.test.mjs`
- evidence: PASS `node --test tools/ai_profile/test.mjs`
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --agent-rollup-evidence --json-output tasks\evidence\T0094-status-rollup.json --no-import-codex-session --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd`
- workflow manifest: tasks/workflows/T0094.json
- reviewer: PASS
  checked: Independent closeout review verified both prior P1 fixes: bootstrap rejects weak status evidence without compact artifact flags, and orchestration-evidence --run prints a full validator-accepted PASS command. The initial workflow status mismatch concern is resolved by moving this task to review with the manifest already set to review.
  risks: none remaining for the closeout evidence contract; broader orchestration maturity remains ongoing under the active thread goal.
  action: ready for review
