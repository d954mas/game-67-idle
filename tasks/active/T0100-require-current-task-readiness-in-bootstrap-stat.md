---
id: T0100
title: Require current-task readiness in bootstrap status evidence
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents]
created: 2026-06-22
updated: 2026-06-22
---

## What

Make new orchestration-bootstrap tasks reject status-agent-rollup evidence commands unless they include the current orchestration task readiness flag, so newly created orchestration tasks cannot confuse green agent rollup with launch readiness.

## Done when

- [x] Stop after bootstrap rejects weak status evidence by construction, compatibility for trace evidence is covered, strict evidence/workflow/reviewer pass, and no gameplay/runtime files are touched.

## Open questions

## Log

- orchestration: used
  objective: Make new orchestration-bootstrap tasks reject status-agent-rollup evidence commands unless they include the current orchestration task readiness flag, so newly created orchestration tasks cannot confuse green agent rollup with launch readiness.
  allowed files: tools/taskboard/lib.mjs;tools/taskboard/cli.mjs;tools/taskboard/test.mjs;tools/ai.test.mjs;tools/ai_profile/orchestration_evidence.mjs;docs/ai-pipeline/subagent-protocol.md;tasks/active/T*.md;tasks/evidence/T*.json;tasks/workflows/T*.json
  tool-use guard: verify exact repo paths with rg --files/Test-Path before Get-Content/read; use Select-Object -Skip/-First, not Format-Hex -Count or Select-Object -Index, for line windows; use orchestration-evidence --current --run --json or trace/status commands with explicit evidence source and --json-output
  expected output: orchestration-bootstrap accepts trace evidence and status evidence with current-task readiness, rejects status evidence without the readiness flag, and focused/full validation pass
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --require-current-orchestration-task --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0100-status-rollup.json --json
  stop condition: Stop after bootstrap rejects weak status evidence by construction, compatibility for trace evidence is covered, strict evidence/workflow/reviewer pass, and no gameplay/runtime files are touched.
  independent reviewer: A read-only subagent verifies bootstrap status-evidence readiness guard scope, trace-only compatibility, tests, and no gameplay/runtime edits.
- implementation: split bootstrap-ready evidence from generic closeout evidence; orchestration-bootstrap now rejects status evidence without current-task readiness, while trace evidence remains exempt.
- implementation: current subagent packet and orchestration-evidence generation now include --require-current-orchestration-task, so public helper output matches the stricter bootstrap contract.
- implementation: updated subagent protocol docs to name the current-readiness requirement for bootstrap/current status evidence.
- evidence: PASS `node --test --test-name-pattern "subagent-packet-template --current|orchestration-evidence forwards current" tools/taskboard/test.mjs tools/ai.test.mjs`
- evidence: PASS `node tools/ai.mjs subagent-packet-template --current` emitted status evidence with --require-current-orchestration-task.
- evidence: PASS `node tools/ai.mjs orchestration-evidence --current --json` emitted status evidence with --require-current-orchestration-task and repo-local artifact tasks/evidence/T0100-status-rollup.json.
- evidence: PASS `node --test tools/taskboard/test.mjs tools/ai.test.mjs`
- evidence: PASS `node --check tools/ai_profile/orchestration_evidence.mjs`
- evidence: PASS `node --check tools/taskboard/lib.mjs`
- evidence: PASS `node --check tools/taskboard/cli.mjs`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --require-current-orchestration-task --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --agent-rollup-evidence --json-output tasks/evidence/T0100-status-rollup.json --json`
- evidence: PASS `node tools/ai.mjs orchestration-evidence --current --run --json`
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/doc_reference_check.mjs`
- workflow manifest: tasks/workflows/T0100.json
- reviewer: PASS
  checked: Zeno the 2nd verified generated status evidence needs --require-current-orchestration-task, bootstrap may accept the alias, and trace evidence stays exempt.
  risks: generic closeout status evidence remains backward compatible by design; bootstrap uses the stricter predicate.
  action: integrated generator, template, docs, and tests; no gameplay/runtime files touched.
