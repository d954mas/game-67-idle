---
id: T0193
title: Confine asset pull target path
status: done
epic: E001
priority: P1
tags: [assets, viewer, pull, security, test]
created: 2026-07-01
updated: 2026-07-01
---

## What

`pull.mjs` copies reusable library assets into a game/template asset root. The
library path may be external shared storage, but `--to` is a local game/template
assets directory and must not write outside the repository.

## Done when

- [x] Relative and absolute `--to` paths inside the repository resolve normally.
- [x] `--to` paths outside the repository are rejected.
- [x] Existing pull metadata behavior still passes.
- [x] Focused asset viewer/storage checks and core map/doc/taskboard checks pass.

## Open questions

## Log

- 2026-07-01: Created after review found `pull.mjs` resolved `--to` without a
  repository containment check.
- 2026-07-01: Added `resolvePullTarget()` and tests for repo-contained relative
  and absolute targets, outside paths, and repo-root misuse.
- 2026-07-01: Validation passed: pull tests 3/3, asset viewer tests 29/29, asset
  storage/source/license/intake/manifest/snapshot/preview tests 57/57,
  `node --check ai_studio/assets/viewer/pull.mjs`, `validate_map.mjs --strict`,
  `doc_reference_check.mjs`, and taskboard `validate --json`.
