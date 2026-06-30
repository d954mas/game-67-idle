---
id: T0182
title: Cover asset intake license metadata fields
status: done
epic: E001
priority: P2
tags: [assets, licensing, tests]
created: 2026-06-30
updated: 2026-06-30
---

## What
Asset Intake docs and help now point agents to richer license/provenance fields,
but the intake tests only prove simple CC0 publish and custom restricted routing.
Add regression coverage that `accept.mjs` persists custom-license provenance,
rights flags, attribution/notice flags, and publishability metadata.

## Done when

- [x] Intake tests cover a custom publishable asset with source page,
      author/vendor, license kind, rights flags, attribution/notice flags, and
      credit text.
- [x] The test verifies the manifest row, scanned record, and publishable path.
- [x] Validate intake tests, map, docs, and taskboard.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after reviewing intake tests: they covered CC0 publishable
  and custom restricted assets, but not the richer metadata fields now shown in
  docs and CLI help.
- 2026-07-01: Added an intake regression test for a custom publishable model
  that records source page, author/vendor, license kind, rights flags,
  attribution/notice flags, credit text, manifest row fields, scanned record
  fields, and `packs/` routing. Validated intake tests, docs, map, and
  taskboard.
- 2026-07-01: Added intake regression coverage for custom publishable license
  metadata; validated intake tests/docs/map/taskboard.
