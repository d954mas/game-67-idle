---
id: T0054
title: Use agent prevention hints in status next action
status: review
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

`status --agent-rollup` now prints prevention hints for known subagent
tool-usage mistakes, but `next_action` still tells the orchestrator to inspect
the same samples again. Make the next action use the computed hints when no
unresolved agent failures remain, so the orchestrator gets an implementation
step instead of a repeated diagnosis step.

## Done when

- [x] `status.next_action` prioritizes unresolved agent failures over
      tool-usage prevention, as before.
- [x] When agent tool-usage failures have prevention hints and no unresolved
      agent failures, `status.next_action` tells the orchestrator to apply the
      printed hints to subagent packets/templates.
- [x] Focused profiling tests pass.
- [x] Taskboard validation and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: make profiling status next action consume computed subagent
  prevention hints instead of repeating sample-inspection advice
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs,
  tasks/active/T0054-use-agent-prevention-hints-in-status-next-action.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: status next_action is more actionable for classified
  subagent tool-use failures while unresolved failures keep priority
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs
  status --agent-rollup --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose; node
  tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove hint-aware next_action and validation
  remains green
  independent reviewer: Pauli audits the next orchestration improvement
- reviewer: PASS Pauli confirmed T0054 is the next smallest high-value
  orchestration improvement and recommended closeout after evidence; noted the
  only compatibility risk is external automation matching the old exact
  `Inspect agent tool-usage failure samples...` text.
- evidence: PASS `node --test tools/ai_profile/test.mjs`; output showed 41
  tests passed.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`; output ended
  with `Apply the printed agent tool-use prevention hints to subagent packets,
  prompts, or templates before the next delegated run.`
- evidence: PASS `node tools/taskboard/cli.mjs validate`; output showed no
  problems found.
- evidence: PASS `node tools/ai.mjs validate --review`; output showed reusable
  pipeline quick+review validation passed.
