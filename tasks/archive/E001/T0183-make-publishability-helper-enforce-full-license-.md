---
id: T0183
title: Make publishability helper enforce full license validation
status: done
epic: E001
priority: P1
tags: [assets, licensing, guard, bug]
created: 2026-06-30
updated: 2026-06-30
---

## What
`restricted.mjs:isPublishable()` currently returns `decideLicense(fm).publishable`.
For custom licenses that can be too weak: a record with `publish=true`,
`redistribution_allowed=true`, and license evidence passes even when
`commercial_use` and `modification_allowed` are missing. The full validator
correctly rejects that. The public-repo asset guard uses `isPublishable()`, so
the helper must enforce the same public-binary validation contract.

## Done when

- [x] `isPublishable()` uses full license validation for public binaries.
- [x] Tests prove custom publish requires redistribution, commercial use,
      modification, and license evidence.
- [x] Existing CC0/CC-BY/restricted guard behavior still passes.
- [x] Validate license guard tests, guard CLI, map, docs, and taskboard.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after direct probe showed `isPublishable()` returning
  true for a custom license missing `commercial_use` and
  `modification_allowed`, while `validateLicenseRecord()` rejected the same
  record.
- 2026-07-01: Changed `isPublishable()` to use
  `validateLicenseRecord(..., { forPublicBinary: true })`, updated guard tests
  to reject incomplete custom publish metadata and accept the complete rights
  set, and validated guard tests, guard CLI, docs, map, and taskboard.
- 2026-07-01: Fixed isPublishable to use full public-binary license validation;
  validated guard tests/CLI/docs/map/taskboard.
