---
id: T0195
title: Fix asset gallery recorder screencast dimensions
status: done
epic: E001
priority: P1
tags: [assets, viewer, recording, test]
created: 2026-07-01
updated: 2026-07-01
---

## What

`record_gallery.mjs` starts Chrome DevTools screencast with `A.w` and `A.h`, but
the parsed options object is named `a`. That makes gallery recording fail at
runtime before it can capture frames.

## Done when

- [x] Screencast options use the parsed width and height values.
- [x] Regression test covers the screencast option builder.
- [x] Existing record gallery hero selection tests still pass.
- [x] Focused asset viewer/storage checks and core map/doc/taskboard checks pass.

## Open questions

## Log

- 2026-07-01: Created after review found undefined `A.w/A.h` in gallery
  recorder screencast startup.
- 2026-07-01: Added `screencastOptions()` and switched recorder startup to use
  parsed `a.w/a.h` values.
- 2026-07-01: Validation passed: record gallery tests 5/5, asset viewer tests
  32/32, asset storage/source/license/intake/manifest/snapshot/preview tests
  57/57, `node --check ai_studio/assets/viewer/record_gallery.mjs`,
  `validate_map.mjs --strict`, `doc_reference_check.mjs`, and taskboard
  `validate --json`.
