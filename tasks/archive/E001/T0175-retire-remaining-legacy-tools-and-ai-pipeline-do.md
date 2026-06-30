---
id: T0175
title: Retire remaining legacy tools and ai-pipeline docs
status: done
epic: E001
priority: P2
tags: [core-harness, profiling, bootstrap, legacy, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What
Retire the last tracked legacy pipeline docs/tools outside `ai_studio/`.
Move the pinned optional full-gate Python requirements into the Profiling module,
remove the old `tools/` route map and `docs/ai-pipeline/profiling-reuse.md`,
and update bootstrap/export/profiling references so new projects do not inherit
the old layout.

## Done when

- [x] Reviewed remaining tracked files under `tools/` and `docs/ai-pipeline/`.
- [x] Moved full-gate Python requirements into
      `ai_studio/core_harness/profiling/`.
- [x] Updated Profiling docs, hook diagnostics, and profiling tests.
- [x] Updated Bootstrap export/copy model and tests.
- [x] Removed obsolete `tools/README.md` and
      `docs/ai-pipeline/profiling-reuse.md`.
- [x] Updated `ai_studio/tree.json`.
- [x] Validated profiling, bootstrap export, map, doc references, and taskboard.
- [x] Committed and pushed the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after Architecture Map validation was clean but tracked
  legacy files still existed under `tools/` and `docs/ai-pipeline/`.
- 2026-07-01: Reviewed references. `tools/requirements/ai-pipeline-full.txt`
  is only needed by profiling diagnostics/export; `profiling-reuse.md` duplicates
  current AI Studio module routing.
- 2026-07-01: Moved requirements to
  `ai_studio/core_harness/profiling/requirements-full.txt`, removed obsolete
  legacy docs, updated bootstrap export and template copy model, and added the
  requirements file to the architecture tree.
- 2026-07-01: Validation passed: profiling tests 25/25, export_base test 1/1,
  `validate_map --strict`, doc reference check, and taskboard validation.
- 2026-06-30: Retired remaining tracked legacy tools/docs, moved full-gate requirements into Profiling, updated export/template routes, and validated profiling/export/map/docs/taskboard.
