---
id: T0045
title: Surface unresolved agent failure samples
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

`status --agent-rollup` reports an unresolved agent failure count but does not
show which agents or commands caused it. Add compact unresolved failure samples
to the JSON and human output so the lead can act on profile telemetry without
manually parsing subagent transcripts.

## Done when

- [x] Agent profile rollup JSON includes compact unresolved failure samples.
- [x] Human status output prints the first unresolved agent commands with agent
  identity, source line, command key, command, and exit code.
- [x] Existing failure counts and command rollups remain unchanged.
- [x] Independent reviewer confirms shape and noise risk.
- [x] Focused tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: surface concrete unresolved agent failure samples in status rollup
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs, tasks/active/T0045-surface-unresolved-agent-failure-samples.md
  expected output: `status --agent-rollup` names sample unresolved agent commands instead of only showing a count
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove JSON/rendered failure samples and review validation remains green
  independent reviewer: Lagrange audits unresolved failure sample shape and edge cases
- reviewer PASS: Lagrange confirmed capped samples under
  `status.agent_rollup.profile_rollup.unresolved_failure_samples`; requested
  source/line in human output and focused tests for transcript/profile samples.
- evidence: PASS `node --test tools/ai_profile/test.mjs` passed 34/34.
- evidence: PASS `node --test tools/ai_profile/test.mjs tools/ai.test.mjs`
  passed 51/51.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` printed unresolved samples with agent, role, transcript line, command key, exit code, and command.
- evidence: PASS `node tools/taskboard/cli.mjs validate` reported no problems.
- evidence: PASS `node tools/ai.mjs validate --review` passed quick+review validation.
- evidence: PASS `git diff --check` reported no whitespace errors.
