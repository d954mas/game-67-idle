---
id: T0035
title: Hint omitted agent rollup in profiling status
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Ordinary profiling status can look complete while omitting subagent work unless
the agent remembers `--agent-rollup`. Add a cheap hint when the parent session
id and matching subagent session metadata are available: `status` should stay
fast and passive, but it should point to the exact focused agent-rollup command
instead of silently hiding the agent dimension.

## Done when

- [x] `status` emits a visible agent-rollup hint when `CODEX_SESSION_FILE`
  exposes a parent session id, matching subagent metadata exists, and
  `--agent-rollup` was not requested.
- [x] The hint is represented in JSON output for tooling.
- [x] `status --agent-rollup` behavior remains unchanged.
- [x] Profiling docs describe the hint and keep agent rollup diagnostic, not
  acceptance evidence by itself.
- [x] Focused tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: make omitted subagent profiling visible without adding hot-path cost
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs, docs/ai-pipeline/profiling-reuse.md, tasks/active/T0035-hint-omitted-agent-rollup-in-profiling-status.md
  expected output: ordinary status reports a focused agent-rollup command when matching subagent metadata exists but agent rollup is omitted
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: status hint reads only lightweight session metadata and does not render full agent rollup unless requested
  independent reviewer: Jason audits hint scope, noise risk, and false positives
- reviewer: PASS Jason confirmed the hint is high-value, but requested gating
  it on matching subagent session metadata, including `--session-root` and
  `--agent-cwd` in the suggested command, and adding negative/cwd tests.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (29 tests)
- evidence: PASS `node --test tools/ai.test.mjs` (16 tests)
- evidence: PASS `node tools/ai.mjs status --agent-rollup --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (21 subagent sessions)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS current strict `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (26 subagent sessions); refreshed after T0039 strict evidence contract.
