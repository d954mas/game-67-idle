---
id: T0032
title: Add agent-aware profiling rollup
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Add an opt-in agent-aware rollup to passive profiling status so orchestration
work can report subagent sessions and transcript-call diagnostics alongside the
existing command/time/failure profile.

## Done when

- [x] `node tools/ai.mjs status --agent-rollup` forwards agent rollup options.
- [x] `status` JSON includes `agent_rollup` with source, ok/problems, call
      count, subagent session count, roles, and agent list.
- [x] Parent-thread metadata mode is covered by tests.
- [x] Trace-session incomplete mode is covered and does not make `status` exit
      nonzero.
- [x] Profiling docs describe the diagnostic, non-acceptance nature of the
      rollup.
- [x] Validation evidence is recorded in this log.

## Open questions

- Should a later task add elapsed time per subagent by reading more than
  `session_meta`, or keep this rollup first-line metadata only for low overhead?

## Log

- orchestration: used
  objective: add agent-aware profiling status rollup without adding live hook overhead
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/orchestration_trace.mjs, tools/ai_profile/test.mjs, tools/ai.mjs, tools/ai.test.mjs, docs/ai-pipeline/profiling-reuse.md, tasks/active/T0032-add-agent-aware-profiling-rollup.md
  expected output: opt-in status agent_rollup JSON/render output, facade flag, focused tests, profiling docs, task evidence
  evidence command: node --test tools/ai_profile/test.mjs; node --test tools/ai.test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: status shows diagnostic agent rollup but does not automatically accept or close agent work
  independent reviewer: Ohm reviewed status integration risks and flag/schema contract

- evidence: PASS `node --test tools/ai_profile/test.mjs` (26 tests) and
  `node --test tools/ai.test.mjs` (15 tests).
- evidence: PASS live `node tools/ai.mjs status --agent-rollup
  --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session`; output showed
  `Agent Rollup` with source `parent-thread`, 18 subagent sessions, roles
  `explorer=16, default=1, worker=1`, and no unresolved profile failures.
- evidence: PASS `node tools/taskboard/cli.mjs validate`,
  `node tools/ai.mjs validate --review`, and `git diff --check`.
- reviewer: Ohm requested distinct `--agent-rollup` / `--trace-session` flags,
  `agent_rollup` JSON, diagnostic non-acceptance semantics, and incomplete
  traces that do not fail normal status. Implemented and covered by tests.
- evidence: PASS current strict `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (26 subagent sessions); refreshed after T0039 strict evidence contract.
