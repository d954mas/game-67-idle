---
id: T0031
title: Add orchestration trace evidence check
status: review
epic: E001
priority: P1
tags: [pipeline, orchestration, subagents, taskboard]
created: 2026-06-21
updated: 2026-06-21
---

## What

Add a focused evidence command for multi-agent orchestration so substantial
pipeline work can prove subagent usage through transcript/session artifacts,
not only task log prose.

## Done when

- [x] `node tools/ai.mjs orchestration-trace` exists as the public facade.
- [x] Transcript fixtures verify spawn -> wait -> close and fail missing or
      unordered orchestration.
- [x] Session fixtures verify subagent `session_meta` by `parent_thread_id`.
- [x] The subagent protocol documents when to use trace evidence.
- [x] Validation evidence is recorded in this log.

## Open questions

- Should a later task make taskboard validation parse trace JSON directly, or
  keep it as explicit lead-supplied evidence?
- Should profiling status aggregate subagent sessions and agent wall-clock as a
  first-class rollup? Current profiling does not.

## Log

- orchestration: used
  objective: add transcript/session trace evidence for subagent orchestration
  allowed files: tools/ai_profile/orchestration_trace.mjs, tools/ai_profile/test.mjs, tools/ai.mjs, tools/ai.test.mjs, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0031-add-orchestration-trace-evidence-check.md
  expected output: facade command, focused tests, protocol note, task evidence
  evidence command: node --test tools/ai_profile/test.mjs; node --test tools/ai.test.mjs; node tools/ai.mjs orchestration-trace --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --cwd C:\projects\game-67-idle --min-agents 2 --json-output tmp/orchestration-trace-t0031.json --json; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: trace command produces machine-readable evidence, but taskboard does not parse trace JSON directly yet
  independent reviewer: Gibbs reviewed transcript design; Cicero reviewed evidence/protocol risks

- evidence: PASS `node --test tools/ai_profile/test.mjs` (23 tests) and
  `node --test tools/ai.test.mjs` (14 tests).
- evidence: PASS `node tools/ai.mjs orchestration-trace --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --cwd
  C:\projects\game-67-idle --min-agents 2 --json-output
  tmp\orchestration-trace-t0031.json --json`; trace recorded 17 matching
  subagent sessions and no problems.
- evidence: PASS `node tools/taskboard/cli.mjs validate`,
  `node tools/ai.mjs validate --review`, and `git diff --check`.
- reviewer: Arendt found P1/P2 gaps: no-input trace passed, incomplete
  wait/close outputs could pass, broad waits could false-negative, and
  parent-thread mode needed clearer limits. Fixed with stricter source
  requirement, completed-output checks, output-derived wait targets, protocol
  wording, and added regression tests.
- profiling note: `node tools/ai.mjs status` currently profiles tool-call
  records, not subagent sessions as first-class work. Agent-aware profiling is
  intentionally left as a follow-up pipeline improvement, separate from this
  trace evidence command.
