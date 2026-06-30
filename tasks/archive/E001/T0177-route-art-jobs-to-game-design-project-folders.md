---
id: T0177
title: Route art jobs to game design project folders
status: done
epic: E001
priority: P2
tags: [assets, game-design, art-jobs, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What
Generated-art job scaffolding must create project-specific files under
`gamedesign/projects/<game-id>/` by default. The old fallback to
`gamedesign/<game-id>/` can create new work in a retired layout.

## Done when

- [x] Reviewed `new_art_job.mjs` project-dir routing.
- [x] Removed the legacy `gamedesign/<concept>` fallback.
- [x] Added/updated tests proving default `--concept` output uses
      `gamedesign/projects/<concept>`.
- [x] Validated art job tests, map, docs, and taskboard.
- [x] Committed and pushed the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after stale-route audit found `new_art_job.mjs` still
  selecting `gamedesign/<concept>` when `gamedesign/projects/<concept>` does not
  exist.
- 2026-07-01: Removed the legacy fallback, added dry-run regression tests for
  default concept routing and explicit project-dir overrides, and validated art
  job tests, architecture map, doc references, and taskboard.
- 2026-07-01: Removed legacy gamedesign/<concept> fallback, added new_art_job
  regression tests, and validated art job tests/map/docs/taskboard.
