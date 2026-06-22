---
id: T0043
title: Include subagent profiles in status rollup
status: done
epic: ""
priority: P1
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

`node tools/ai.mjs status --agent-rollup` proves that subagent sessions exist,
but it previously did not attribute subagent command time or repeated commands.
Add analysis-time subagent telemetry to the rollup without changing hook hot
paths: read matching profile logs when present and fall back to Codex subagent
transcripts when profile logs are absent.

## Done when

- [x] Agent rollup reports subagent telemetry coverage and recorded command
  time separately from the parent profile rollup.
- [x] Missing profile logs do not fail strict orchestration evidence when
  transcript telemetry is available.
- [x] Cross-agent failure classification does not let one agent's later PASS
  resolve another agent's failed command.
- [x] Focused profile tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: include subagent command telemetry in profile status rollups without slowing hook hot paths
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/profile_lib.mjs, tools/ai_profile/test.mjs, docs/ai-pipeline/profiling-reuse.md, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0043-include-subagent-profiles-in-status-rollup.md
  expected output: `status --agent-rollup` reports subagent telemetry agents, sources, command time, failures, and top time-sinks
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove profile and transcript telemetry rollups and review validation remains green
  independent reviewer: Rawls audits agent profile rollup design and edge cases
- reviewer: PASS Rawls recommended a separate best-effort
  `agent_rollup.profile_rollup`, strict rollup remaining orchestration-only,
  sessionFile-aware profile lookup, and per-agent failure classification.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (32 tests)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (30 subagent sessions; telemetry agents 30/30; transcripts 30; recorded command time 11.7m)
- evidence: PASS `node --test tools/ai_profile/test.mjs tools/ai.test.mjs` (49 tests)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `git diff --check`
- evidence: PASS `node --test tools/bootstrap/export_base.test.mjs`
- evidence: PASS `node tools/ai.mjs validate --review`
