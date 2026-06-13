---
id: T0080
title: Add one-command AI profile iteration start
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a low-overhead `tools/ai_profile/start.mjs` helper that begins a
profiled work item in one command: persist the current work-item/iteration
scope and append an initial profiling checkpoint.

## Done when

- [x] `start.mjs` writes persistent scope metadata and appends a
      `phase_start` profile event with work-item/iteration fields.
- [x] It supports explicit `--profile`, `--scope`, `--phase`, `--category`,
      `--intent`, and `--notes` options while keeping sensible defaults.
- [x] AI profile tests cover the start helper and the reusable pipeline runs
      those tests.
- [x] Profiling docs and reflection skill mention `start.mjs` as the default
      low-overhead session/iteration entry point.
- [x] Validation passes for AI profile tests, taskboard, skill eval, diff
      check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started after T0079 because profiler behavior is tested, but
  session setup still requires remembering separate scope and event commands.
- 2026-06-13: Added `tools/ai_profile/start.mjs` to write persistent scope and
  append a `phase_start` profile event in one command.
- 2026-06-13: Extended `tools/ai_profile/test.mjs` to cover custom
  profile/scope paths, work-item/iteration metadata, event type, notes, and
  tool attribution for `start.mjs`.
- 2026-06-13: Updated profiling docs, reflection skill, skill eval anchors,
  and iteration log so `start.mjs` is the default entry point for focused
  profiled iterations.
- 2026-06-13: Evidence: `node --test tools/ai_profile/test.mjs` passed 5
  tests; `node --check tools/ai_profile/start.mjs`; `node --check
  tools/skills_eval.mjs`; `node tools/skills_sync.mjs`; `node
  tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`; `node
  tools/pipeline_validate.mjs` passed, including exported AI profile tests.
- 2026-06-13: Moved to review after start helper implementation, docs/skill updates, ai profile tests, skill eval, taskboard validation, and reusable pipeline validation passed.
