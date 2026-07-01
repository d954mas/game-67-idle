---
id: T0196
title: Confine asset scan review roots
status: done
epic: E001
priority: P1
tags: [assets, viewer, scan, security, test]
created: 2026-07-01
updated: 2026-07-01
---

## What

`build_review.mjs --mode scan` builds a review manifest for game/template assets.
Its scan root should stay inside `--repo`; otherwise the review manifest can
contain `../...` relpaths and later promote/export paths become ambiguous.
External shared libraries remain handled by `--mode library`.

## Done when

- [x] Default scan root resolves to `<repo>/assets`.
- [x] Relative and absolute scan paths inside `--repo` resolve normally.
- [x] Scan paths outside `--repo` are rejected clearly.
- [x] Existing build review tests and asset viewer/storage checks pass.

## Open questions

## Log

- 2026-07-01: Created after review found scan mode accepted paths outside
  `--repo`.
- 2026-07-01: Added `resolveScanRoot()` and tests for default, inside, and
  outside scan roots.
- 2026-07-01: Validation passed: build review tests 7/7, asset viewer tests
  33/33, asset storage/source/license/intake/manifest/snapshot/preview tests
  57/57, `node --check ai_studio/assets/viewer/build_review.mjs`,
  `validate_map.mjs --strict`, `doc_reference_check.mjs`, and taskboard
  `validate --json`.
