---
id: T0197
title: Expose asset gallery recorder dimensions CLI
status: done
epic: E001
priority: P2
tags: [assets, viewer, recording, cli, test]
created: 2026-07-01
updated: 2026-07-01
---

## What

`record_gallery.mjs` uses `w/h` to size the Chrome window and screencast, but
the CLI did not expose these options. Recorder dimensions should be adjustable
without editing the script.

## Done when

- [x] `--w` and `--h` parse into recorder dimensions.
- [x] Invalid non-positive dimensions fail clearly.
- [x] Existing record gallery tests still pass.
- [x] Focused asset viewer/storage checks and core map/doc/taskboard checks pass.

## Open questions

## Log

- 2026-07-01: Created after review found recorder dimensions existed internally
  but were not reachable from CLI arguments.
- 2026-07-01: Added `--w/--h` parsing with positive-number validation and
  covered the recorder CLI contract in `record_gallery.test.mjs`.
- 2026-07-01: Verified viewer/storage tests, architecture map strict validation,
  core doc references, and taskboard validation.
