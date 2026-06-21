---
id: T0039
title: Normalize PowerShell profile command rollup
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Profiling status can misclassify PowerShell-prefixed commands such as
`$env:AI_PIPELINE_PYTHON='...'; node tools/ai.mjs validate --full` as a path
fragment like `python.exe';`. Normalize leading PowerShell environment
assignments before computing command rollups so time-sinks and repeat counts
point at the real tool command.

Also separate diagnostic agent rollups from strict closeout evidence:
`status --agent-rollup` stays non-failing for analysis, while
`--require-agent-rollup-ok` makes it a strict evidence command. Taskboard
machine evidence must use the strict form.

## Done when

- [x] Status command rollup reports the real command after a leading
  PowerShell `$env:` assignment.
- [x] Existing POSIX-style env assignment normalization still works.
- [x] `status --agent-rollup --require-agent-rollup-ok` fails incomplete
  agent rollups without changing normal diagnostic status behavior.
- [x] Taskboard accepts status agent-rollup machine evidence only with
  `--require-agent-rollup-ok`.
- [x] Focused profiling tests and pipeline review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: fix profile command rollup classification and require strict status agent-rollup evidence for taskboard closeout
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs, tools/ai.mjs, tools/ai.test.mjs, tools/taskboard/lib.mjs, tools/taskboard/test.mjs, docs/ai-pipeline/profiling-reuse.md, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0032-T0039 orchestration evidence files
  expected output: profiling status attributes PowerShell env-prefixed validation runs to the real node/python tool and taskboard no longer accepts non-strict status agent-rollup evidence
  evidence command: node --test tools/ai_profile/test.mjs; node --test tools/taskboard/test.mjs; node --test tools/ai.test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests pass, status rollup no longer emits the observed `python.exe';` key, and non-strict status agent-rollup evidence is rejected by taskboard
  independent reviewer: explorer agents audit profiling/status and taskboard/status orchestration gaps
- reviewer: PASS Kuhn found that diagnostic `status --agent-rollup` could exit
  0 while `agent_rollup.ok` was false; fixed with
  `--require-agent-rollup-ok` and taskboard strict matching.
- reviewer: PASS Ramanujan found live status/context drift back to Dragon Grove;
  captured as remaining follow-up for taskboard/status context, while this slice
  fixes the strict profiling evidence contract.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (30 tests)
- evidence: PASS `node --test tools/taskboard/test.mjs` (55 tests)
- evidence: PASS `node --test tools/ai.test.mjs` (17 tests)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (26 subagent sessions)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/ai.mjs validate --review`
