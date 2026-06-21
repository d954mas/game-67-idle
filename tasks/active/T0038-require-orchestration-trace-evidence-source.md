---
id: T0038
title: Require orchestration trace evidence source
status: review
epic: ""
priority: P1
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

`node tools/ai.mjs orchestration-trace` fails without an evidence source, but
the taskboard machine-evidence matcher could still treat a source-less
`orchestration-trace` command as a valid machine signature. Require
`orchestration-trace` evidence to include either `--session` or
`--parent-thread-id`, and require later PASS evidence to carry the same source
identity.

## Done when

- [x] Packet evidence with `orchestration-trace` but no `--session` or
  `--parent-thread-id` is rejected as missing machine evidence.
- [x] Declared trace evidence with a source cannot be closed by a source-less
  trace PASS line.
- [x] Existing source-bearing trace/status evidence remains accepted.
- [x] Focused taskboard tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: require source-bearing orchestration-trace evidence in taskboard machine checks
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0038-require-orchestration-trace-evidence-source.md
  expected output: taskboard rejects source-less orchestration-trace evidence and preserves existing source-bearing trace/status acceptance
  evidence command: node --test tools/taskboard/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: validation stays static and mirrors orchestration-trace's required evidence-source contract
  independent reviewer: Nietzsche audits trace source requirement and false-positive risks
- reviewer: PASS Nietzsche confirmed source identity is required because
  `node tools/ai.mjs orchestration-trace --json` exits with missing evidence
  source, and requested protocol prose that names `--session`/`--parent-thread-id`.
- evidence: PASS `node --test tools/taskboard/test.mjs` (54 tests)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (24 subagent sessions)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `git diff --check`
- evidence: PASS current strict `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (26 subagent sessions); refreshed after T0039 strict evidence contract.
