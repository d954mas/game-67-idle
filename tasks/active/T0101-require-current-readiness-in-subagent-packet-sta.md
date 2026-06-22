---
id: T0101
title: Require current readiness in subagent packet status evidence
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-22
updated: 2026-06-22
---

## What

Make subagent-packet-check reject status-agent-rollup evidence commands unless they include current orchestration readiness, keeping trace evidence exempt, so manual subagent packets cannot drift from the stricter bootstrap/current evidence contract.

## Done when

- [x] Stop after subagent packet validator enforces current-readiness for status evidence, trace compatibility is covered, strict evidence/workflow/reviewer pass, and no gameplay/runtime files are touched.

## Open questions

## Log

- orchestration: used
  objective: Make subagent-packet-check reject status-agent-rollup evidence commands unless they include current orchestration readiness, keeping trace evidence exempt, so manual subagent packets cannot drift from the stricter bootstrap/current evidence contract.
  allowed files: tools/taskboard/lib.mjs;tools/taskboard/test.mjs;tools/ai.test.mjs;tasks/active/T*.md;tasks/evidence/T*.json;tasks/workflows/T*.json
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: subagent-packet-check rejects status evidence without current readiness, accepts the primary flag and alias, keeps trace evidence accepted, and focused/full validation pass
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --require-current-orchestration-task --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0101-status-rollup.json --json
  stop condition: Stop after subagent packet validator enforces current-readiness for status evidence, trace compatibility is covered, strict evidence/workflow/reviewer pass, and no gameplay/runtime files are touched.
  independent reviewer: A read-only subagent verifies packet-check status readiness guard scope, trace-only compatibility, tests, and no gameplay/runtime edits.
- implementation: subagent-packet-check now rejects status-agent-rollup evidence without --require-current-orchestration-task or --require-current-orchestration-preflight.
- implementation: added coverage for reject-without-readiness, primary flag, alias, and orchestration-trace exemption.
- evidence: PASS `node --test --test-name-pattern "subagent packet check" tools/taskboard/test.mjs`
- evidence: PASS `node --check tools/taskboard/lib.mjs`
- evidence: PASS weak packet probe returned `current status readiness`.
- evidence: PASS `node --test tools/taskboard/test.mjs tools/ai.test.mjs`
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --require-current-orchestration-task --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0101-status-rollup.json --json`
- evidence: PASS `node tools/ai.mjs orchestration-evidence --current --run --json`
- workflow manifest: tasks/workflows/T0101.json
- reviewer: PASS
  checked: Galileo the 2nd confirmed subagent-packet-check accepted weak status evidence and recommended enforcing current readiness in tools/taskboard/lib.mjs.
  risks: orchestration-trace remains exempt by design; status --trace-session is still status evidence and now requires current readiness.
  action: integrated validator and regression tests; no gameplay/runtime files touched.
