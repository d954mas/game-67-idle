---
id: T0029
title: Split quick and full reusable pipeline validation
status: done
epic: E003
priority: P1
tags: [profiling, pipeline, validation]
created: 2026-06-15
updated: 2026-06-15
---

## What

`tools/pipeline_validate.mjs` currently runs the full reusable-base/export
gate every time. That made normal pipeline cleanup slow and noisy, and it
encouraged agents to spend broad validation budget after narrow changes.

Split the command into a quick default core-pipeline validation and an explicit
full mode for portable export/runtime/deep asset gates.

## Done when

- [x] `node tools/pipeline_validate.mjs --dry-run` shows a quick default that
      skips portable export/exported validation.
- [x] `node tools/pipeline_validate.mjs --full --dry-run` still includes the
      portable export and exported-project validation path.
- [x] Planner/docs point normal scoped validation at the quick path and reserve
      full validation for final portable-base gates.
- [x] Tests cover quick vs full command selection without running heavy checks.
- [x] Taskboard/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Started after T0028. Previous profiled full pipeline validation
  took about 87 seconds and duplicated many checks in the exported project.
- 2026-06-15: Added quick default, explicit `--full`, and `--dry-run` to
  `tools/pipeline_validate.mjs`; updated planner/profile classification so
  only `node tools/pipeline_validate.mjs --full` is broad/final evidence.
  Validation: `node --test tools/pipeline_validate.test.mjs`,
  `node tools/pipeline_validate.mjs --dry-run`,
  `node tools/pipeline_validate.mjs --full --dry-run`, `node --test
  tools/ai_profile/test.mjs`, profiled quick
  `node tools/pipeline_validate.mjs`, `git diff --check ...`,
  `node tools/taskboard/cli.mjs validate`, and
  `node tools/ai.mjs status --require-current-scope-usable` passed.
