---
id: T0063
title: Infer preflight task id in status next action
status: done
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Clean-tail status guidance now recommends
`node tools/ai.mjs orchestration-check <task-id> --json`, but it can only print
a placeholder. When there is exactly one current `doing` pipeline/orchestration
task, infer that task id from the taskboard and print the concrete preflight
command. If there are zero or multiple candidates, keep the placeholder.

## Done when

- [x] Clean-tail `status --agent-rollup` uses a concrete task id when exactly
      one `doing` pipeline/orchestration task exists.
- [x] Clean-tail status keeps `<task-id>` when no unique task can be inferred.
- [x] Existing unresolved/short-tail priorities remain unchanged.
- [x] Focused profile tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: infer a concrete preflight task id for clean-tail status guidance
  allowed files: tools/taskboard/lib.mjs, tools/ai_profile/status.mjs,
  tools/ai_profile/test.mjs,
  tasks/active/T0063-infer-preflight-task-id-in-status-next-action.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: clean-tail Next Action prints a concrete
  `orchestration-check T0063 --json` command when the taskboard has exactly one
  doing orchestration task
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: tests cover unique-task inference and fallback placeholder
  behavior, and live status prints the current task id
  independent reviewer: Kierkegaard audits taskboard coupling and tests

- reviewer Kierkegaard: PASS; requested taskboard-owned helper to avoid keyword
  drift and tests for unique, ambiguous, and non-current fallback behavior.
- PASS node --test tools/ai_profile/test.mjs (48/48).
- PASS node --test tools/taskboard/test.mjs (69/69).
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose` printed
  `node tools/ai.mjs orchestration-check T0063 --json` in Next Action
  (50/50 telemetry agents; clean tail 10).
- PASS node tools/taskboard/cli.mjs validate.
- PASS node tools/ai.mjs validate --review.
- PASS git diff --check.
