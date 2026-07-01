---
id: T0190
title: Harden asset viewer gallery meta routing
status: done
epic: E001
priority: P1
tags: [assets, viewer, api, test]
created: 2026-07-01
updated: 2026-07-01
---

## What

Asset Viewer gallery routes use `.asset-viewer-source.json` to serve library
files through `/asset_viewer/gallery/<source>/lib/...`. If this meta file is
missing required data or becomes corrupted, the static shell route should behave
like a missing asset route instead of throwing and risking a shell request crash.

## Done when

- [x] Corrupt gallery meta returns no file path instead of throwing.
- [x] Gallery meta without `libraryRoot` returns no file path.
- [x] Existing gallery, library, and repo path confinement tests still pass.
- [x] Focused asset viewer/storage checks and core map/doc/taskboard checks pass.

## Open questions

## Log

- 2026-07-01: Created after finding raw JSON parsing in gallery `lib` route.
- 2026-07-01: Added safe meta parsing and regression coverage for corrupt/missing
  `libraryRoot` meta.
- 2026-07-01: Validation passed: asset viewer tests 28/28, asset storage/source/
  license tests 45/45, `node --check ai_studio/assets/viewer/api.mjs`,
  `validate_map.mjs --strict`, `doc_reference_check.mjs`, and taskboard
  `validate --json`.
