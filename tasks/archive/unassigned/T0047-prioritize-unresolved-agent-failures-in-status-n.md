---
id: T0047
title: Prioritize unresolved agent failures in status next action
status: done
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

`node tools/ai.mjs status --agent-rollup` can print unresolved agent failures
and concrete samples, but the final `## Next Action` can still say no profiling
action is needed because next-action selection only considers parent-session
failures.

Make unresolved agent profile failures a first-class next-action signal so the
status output directs the lead to inspect agent samples before generic command
rollups.

## Done when

- [x] Agent rollup unresolved failures produce a next action that names agent
  failure samples.
- [x] Parent-session unresolved failures keep priority, while unresolved agent
  failures outrank environment blockers.
- [x] Clean agent rollups still produce the existing generic no-action message.
- [x] Independent reviewer confirms priority/risk.
- [x] Focused tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: make status next action actionable when agent rollup has unresolved failures
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs, tasks/active/T0047-prioritize-unresolved-agent-failures-in-status-n.md
  expected output: `node tools/ai.mjs status --agent-rollup` points at unresolved agent samples instead of saying no profiling action is needed
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove next-action priority and review validation remains green
  independent reviewer: Hooke audits agent failure next-action priority and edge cases
- reviewer PASS: Hooke confirmed `buildStatus()` computed agent rollup but
  ignored unresolved agent failures in `next_action`; recommended priority
  order missing/invalid/empty profile, parent unresolved, agent unresolved,
  environment blockers, low coverage, then generic no-action.
- evidence: PASS `node --test tools/ai_profile/test.mjs` passed 36/36.
- evidence: PASS `node --test tools/ai_profile/test.mjs tools/ai.test.mjs`
  passed 53/53.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` printed `Inspect unresolved agent failure samples before trusting the orchestration rollup.`
- evidence: PASS `node tools/taskboard/cli.mjs validate` reported no problems.
- evidence: PASS `node tools/ai.mjs validate --review` passed quick+review validation.
- evidence: PASS `git diff --check` reported no whitespace errors.
