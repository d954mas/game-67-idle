---
id: T0041
title: Require agents for strict status rollup
status: done
epic: ""
priority: P1
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

`status --agent-rollup --require-agent-rollup-ok` should prove that at least
one matching subagent session exists by default. Today the diagnostic
`--agent-rollup` path defaults `--min-agents` to `0`, which is fine for
analysis, but weak for strict task evidence.

## Done when

- [x] Strict status rollup without explicit `--min-agents` fails when no
  matching subagent session exists.
- [x] Normal diagnostic `status --agent-rollup` behavior remains non-failing.
- [x] Docs explain the strict default.
- [x] Focused profiling tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: require at least one agent for strict status rollup evidence by default
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs, docs/ai-pipeline/profiling-reuse.md, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0041-require-agents-for-strict-status-rollup.md
  expected output: strict status agent-rollup evidence cannot pass with a valid source but zero matching subagents
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove strict zero-agent rollup fails while diagnostic rollup stays non-failing
  independent reviewer: Mendel audits strict agent-rollup min-agents semantics
- reviewer: PASS Mendel confirmed the gap and recommended defaulting
  `minAgents` to 1 only under `--require-agent-rollup-ok`, leaving taskboard as
  a static shape/PASS matcher.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (31 tests)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (28 subagent sessions)
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
