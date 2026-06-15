---
id: T0019
title: Improve profiling scope coverage for prototype sessions
status: done
epic: E003
priority: P0
tags: [profiling, ai-workflow, session-start]
created: 2026-06-15
updated: 2026-06-15
---

## What

Make passive AI profiling useful during real prototype work. The fishing review
showed only 0.3% current wall-clock coverage and a stale scope after task
switches, so the profiler did not explain where the session time actually went.

This is pipeline work only. It must not require deep retrospective artifacts for
normal game implementation.

## Done when

- [x] Starting or switching a task has a low-friction profiler scope path.
- [x] `node tools/ai.mjs status` calls out missing coverage in plain language
      and points to the next concrete command.
- [x] Stale scope detection remains covered by tests.
- [x] A short status/checkpoint workflow is documented where future agents will
      see it before long prototype work.

## Open questions

- Answered for now: keep execution explicit. Auto-start can hide scope mistakes;
  the startup/status workflow prints the exact command and `status` now reports
  review confidence.

## Log

- 2026-06-15: Created from fishing review finding: profile coverage was too low
  to diagnose the real stalls, and current scope stayed stale across work.
- 2026-06-15: Started current profiler scope with
  `node tools/ai.mjs start T0019 profiling-coverage-fix`.
- 2026-06-15: Added profiler review confidence to `node tools/ai.mjs status`.
  Current real profile now reports `Review confidence: partial` with
  `whole_profile_low_wall_clock_coverage`, making the broken fishing telemetry
  explicit instead of hiding it behind a raw percentage.
- 2026-06-15: Validation passed:
  `node --test tools/ai_profile/test.mjs`,
  `node tools/skills_eval.mjs`, and
  `node tools/taskboard/cli.mjs validate`.
