---
id: T0184
title: Preserve publishability metadata on asset pull
status: done
epic: E001
priority: P1
tags: [assets, licensing, pull, bug]
created: 2026-07-01
updated: 2026-07-01
---

## What
`pull.mjs` routes a shared-library asset into `packs/` or `restricted/packs/`
using `isPublishable(record)`, but the game-local manifest row created by
`localRecord()` does not preserve `publish`, `commercial_use`,
`modification_allowed`, or `redistribution_allowed`. A custom publishable asset
can therefore be copied into public `packs/` and then fail the stricter
public-repo license guard from its local manifest row.

## Done when

- [x] `pull.mjs` is safe to import for unit tests.
- [x] `localRecord()` preserves publishability/right flags from the library
      record.
- [x] Tests prove a pulled custom publishable record remains publishable after
      local manifest conversion.
- [x] Architecture tree includes the pull test.
- [x] Validate pull tests, viewer/license tests, map, docs, and taskboard.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after reviewing `pull.mjs` after strict publishability:
  copied local manifest records were dropping the rights flags required by the
  public-repo guard.
- 2026-07-01: Added an import-safe CLI guard, preserved publishability fields
  in local records, added pull regression tests, and validated pull/promote/
  license tests, architecture map, markdown references, and taskboard state.
- 2026-07-01: Preserved pull publishability metadata and added pull tests; validated pull/promote/license/map/docs/taskboard.
