---
id: T0025
title: Make profiler current-scope guard catch archived completed tasks
status: done
epic: E003
priority: P1
tags: [profiling, taskboard]
created: 2026-06-15
updated: 2026-06-15
---

## What

After `T0024` was archived, `node tools/ai.mjs status
--require-current-scope-usable` still reported the old `T0024/...` scope as
usable. Fix the profiler guard so completed archived tasks cannot remain valid
current-scope review evidence.

## Done when

- [x] A current scope pointing at an archived `done` task is classified as
      stale and fails `--require-current-scope-usable`.
- [x] A regression test covers the archived-task case, not only active done
      tasks.
- [x] Normal active current scopes still report usable after a checkpoint or
      profiled command.
- [x] Task/status/profiler validation passes.

## Open questions

## Log

- 2026-06-15: Created after `T0024` closure exposed that a just-archived done
  task could still pass the current-scope guard.
- 2026-06-15: Done. `tools/ai_profile/status.mjs` now resolves slash-suffixed
  work item labels like `T0025/slice-name` to the real task id for taskboard
  lookup, so archived/done tasks are stale even when the profile scope uses a
  descriptive suffix.
- 2026-06-15: Evidence: `node --test tools/ai_profile/test.mjs`, `node
  tools/taskboard/cli.mjs validate`, `node tools/skills_eval.mjs`, `node
  tools/ai.mjs status --require-current-scope-usable` before archiving, and
  profiled `node tools/pipeline_validate.mjs`.
