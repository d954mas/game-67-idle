---
id: T0066
title: Allow orchestration preflight for current task
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

The no-current clean-tail status guidance now says to create/refine one
`doing` pipeline/orchestration task from `node tools/ai.mjs
orchestration-template`, then run `node tools/ai.mjs orchestration-check
<task-id> --json`. Once the task exists, copying the id is still a small but
repeated friction point.

Allow `orchestration-check --current` to resolve exactly one current `doing`
pipeline/orchestration task and preflight it. Keep no-current and multi-current
cases explicit failures, so the command never guesses.

## Done when

- [x] `node tools/taskboard/cli.mjs orchestration-check --current --json`
      resolves exactly one current `doing` pipeline/orchestration task.
- [x] `--current` fails clearly when there are zero or multiple current
      orchestration candidates.
- [x] `node tools/ai.mjs orchestration-check --current --json` forwards through
      the facade.
- [x] No-current clean-tail status guidance points to `--current` after the
      template step.
- [x] Focused taskboard/facade/profile tests, taskboard validation, and review
      validation pass.

## Open questions

## Log

- orchestration: used
  objective: reduce task-id copy friction by allowing preflight of the single
  current doing orchestration task
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/cli.mjs,
  tools/taskboard/test.mjs, tools/ai.mjs, tools/ai.test.mjs,
  tools/ai_profile/status.mjs, tools/ai_profile/test.mjs,
  tasks/active/T0066-allow-orchestration-preflight-for-current-task.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: `orchestration-check --current --json` resolves one current
  task, rejects zero or multiple candidates, and no-current status guidance
  points at `--current` after the template command
  evidence command: node --test tools/taskboard/test.mjs; node --test tools/ai.test.mjs; node --test tools/ai_profile/test.mjs; node tools/ai.mjs orchestration-check --current --json; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: current-task preflight works through taskboard and AI facade,
  status guidance names the current-task form, and review validation passes
  independent reviewer: subagents audit resolution semantics and coverage

- reviewer Confucius: PASS; confirmed `--current` should be a mutually
  exclusive selector using `currentDoingOrchestrationTaskIds(root)`, with clear
  stderr failures for zero or multiple current tasks.
- reviewer Descartes: PASS; requested profile guidance use `--current` after
  creating exactly one current task and for the single-current branch, while
  preserving explicit multi-current resolution guidance.
- PASS node --test tools/taskboard/test.mjs (73/73).
- PASS node --test tools/ai.test.mjs (20/20).
- PASS node --test tools/ai_profile/test.mjs (48/48).
- PASS node tools/ai.mjs orchestration-check --current --json.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose` printed
  `node tools/ai.mjs orchestration-check --current --json` in Next Action
  (56/56 telemetry agents; clean tail 16).
- PASS node tools/taskboard/cli.mjs validate.
- PASS node tools/ai.mjs validate --review.
- PASS git diff --check.
