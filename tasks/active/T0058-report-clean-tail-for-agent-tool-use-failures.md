---
id: T0058
title: Report clean tail for agent tool-use failures
status: review
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Agent status rollup is cumulative across the parent thread. After T0053/T0054,
old classified subagent tool-use failures still keep `Next Action` focused on
applying hints even when later delegated agents have run clean. Add a recent
clean-tail signal for classified agent tool-use failures so status can
distinguish old friction from current delegated-run behavior.

## Done when

- [x] Agent profile rollup reports how many latest telemetry agents have no
      classified `agent_tool_usage` failures.
- [x] If cumulative tool-use failures exist but at least three latest agents are
      clean and no unresolved failures remain, `next_action` stops repeating the
      urgent apply-hints advice and says to keep using hints/watch next run.
- [x] Existing unresolved-failure priority is unchanged.
- [x] Focused profile tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: make agent status distinguish old classified tool-use failures from
  a recent clean delegated-agent tail
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs,
  tasks/active/T0058-report-clean-tail-for-agent-tool-use-failures.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: status renders recent clean tail and next_action is less
  stale when current delegated agents are clean
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs
  status --agent-rollup --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose; node
  tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove clean-tail next_action behavior and live
  status shows the clean tail without hiding cumulative failures
  independent reviewer: Curie audits heuristic and compatibility risk

- reviewer Curie: heuristic is appropriate if ordered by agent timestamp;
  unresolved failures must stay higher priority; add boundary tests for two
  clean agents, reset after later tool-use failure, and unresolved priority.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (45/45).
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose` reported
  `agent tool-usage clean tail: 5 agent(s)` and next action:
  "Recent subagents are clean of classified tool-use failures; keep the printed
  prevention hints in packets and watch the next delegated run."
- evidence: PASS `node tools/taskboard/cli.mjs validate`.
- evidence: PASS `node tools/ai.mjs validate --review`.
