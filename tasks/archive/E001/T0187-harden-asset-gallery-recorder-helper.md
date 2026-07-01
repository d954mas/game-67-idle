---
id: T0187
title: Harden asset gallery recorder helper
status: done
epic: E001
priority: P1
tags: [assets, viewer, recorder, test]
created: 2026-07-01
updated: 2026-07-01
---

## What
`record_gallery.mjs` is a secondary Asset Viewer evidence helper, but it still
starts work at module import time and keeps argument parsing, hero selection,
and CDP port routing coupled to globals. Make the pure parts testable without
changing the recording scenario.

## Done when

- [x] `record_gallery.mjs` can be imported without recording or spawning Chrome.
- [x] Argument parsing rejects unknown and missing-value options.
- [x] Hero selection is covered for preferred pack/asset, fallback pack, and
      missing payload/empty gallery failures.
- [x] Architecture tree maps the new recorder tests.
- [x] Focused viewer/storage tests, map validation, doc references, and
      taskboard validation pass.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after hardening `serve_gallery.mjs`; `record_gallery.mjs`
  has the same import-safety gap and untested pure helper behavior.
- 2026-07-01: Made `record_gallery.mjs` import-safe, localized argv/port inside
  `main`, hardened argument parsing, added explicit empty-gallery errors, and
  switched output-directory creation to `dirname(out)`.
- 2026-07-01: Added recorder tests and validated 23 focused viewer tests, 33
  focused storage tests, architecture map, markdown references, and taskboard
  state.
- 2026-07-01: Hardened Asset Viewer recorder import path, argument parsing, and hero selection; added record_gallery tests and mapped them in architecture tree.
