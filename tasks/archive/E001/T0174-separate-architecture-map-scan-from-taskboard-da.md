---
id: T0174
title: Separate architecture map scan from taskboard data
status: done
epic: E001
priority: P2
tags: [architecture-map, taskboard, validation, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What
Architecture Map validation should track AI Studio architecture sources and
legacy AI/tooling surfaces, not Taskboard data files. Epics and active tasks are
durable Taskboard state; they are validated by Taskboard itself and should not
appear as unmapped legacy architecture work.

## Done when

- [x] Reviewed why `tasks/epics/*.md` appears as unmapped legacy.
- [x] Removed Taskboard data roots from Architecture Map scan roots.
- [x] Documented the boundary in `ai_studio/architecture_map/README.md`.
- [x] Verified Architecture Map reports no unmapped AI Studio or legacy files.
- [x] Verified Taskboard still validates task/epic state.
- [x] Committed and pushed the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after `validate_map` showed only 8 unmapped legacy files,
  all under `tasks/epics/`.
- 2026-07-01: Reviewed Taskboard README: active tasks and epics are Taskboard
  data, and epics are metadata, not a second architecture navigation layer.
- 2026-07-01: Removed `tasks/active` and `tasks/epics` from Architecture Map
  scan roots. `validate_map` now reports `unmapped_ai_studio=0` and
  `unmapped_legacy=0`; Taskboard validation remains green.
- 2026-06-30: Separated Architecture Map scanning from Taskboard data, documented boundary, and validated map strict/taskboard/docs/tests.
