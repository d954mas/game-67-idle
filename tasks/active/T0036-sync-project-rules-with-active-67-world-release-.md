---
id: T0036
title: Sync project rules with active 67 World release state
status: review
epic: ""
priority: P1
tags: [pipeline, rules, handoff, release]
created: 2026-06-12
updated: 2026-06-13
---

## What

Sync project-level rules and live status with the current 67 World release
track. `AGENTS.md` still described the repo as having no active concept and a
placeholder screen, which can make future agents restart discovery or avoid
release work that is already in progress.

## Done when

- [x] `AGENTS.md` names 67 World as the active concept and describes the native
      game as the current runtime surface.
- [x] `AGENTS.md` still preserves the native-PC-first, generated-art, reference
      study, protected engine/runtime-infrastructure, and taskboard rules.
- [x] `tasks/STATUS.md` lists this task and no longer says implementation must
      wait for concept confirmation.
- [x] The pipeline iteration log records the process lesson.
- [x] Taskboard validation passes.

## Open questions

None. This updates stale project rules to match the user-selected concept; it
does not change gameplay, balance, art, engine code, packaging, or web policy.

## Log

- 2026-06-13: Started after a continuation audit found `AGENTS.md` still
  saying "No active game concept" and "placeholder screen" while `STATUS.md`
  and active tasks clearly target 67 World release quality.
- 2026-06-13: Updated `AGENTS.md` to name 67 World as the active concept and
  native runtime, while keeping engine/submodule protections, taskboard
  source-of-truth rules, generated-art guidance, reference-study gate, and
  native-PC-first validation.
- 2026-06-13: Updated `tasks/STATUS.md` and `AI_PIPELINE_ITERATION_LOG.md` to
  record the project-rule drift and current release-track source of truth.
  Evidence passed: `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- AGENTS.md AI_PIPELINE_ITERATION_LOG.md tasks/STATUS.md tasks/active/T0036-sync-project-rules-with-active-67-world-release-.md`.
