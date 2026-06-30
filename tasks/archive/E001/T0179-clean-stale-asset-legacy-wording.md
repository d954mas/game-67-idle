---
id: T0179
title: Clean stale asset legacy wording
status: done
epic: E001
priority: P2
tags: [assets, docs, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What
Some current asset docs, comments, and map descriptions still use stale legacy
wording even though the reviewed asset module is now under `ai_studio/assets`.
Clean the wording without changing behavior.

## Done when

- [x] Asset module map description no longer says unresolved legacy asset
      pipeline areas still need separate review.
- [x] Asset kind map is described as current manifest/viewer/index support, not
      legacy readers.
- [x] Asset viewer/storage comments describe current fallbacks and helper
      surfaces without legacy wording.
- [x] Validate map, docs, taskboard, and touched viewer promote tests.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after live asset scan found stale legacy wording in the
  architecture tree and asset helper comments, while current code behavior did
  not need a legacy compatibility path.
- 2026-07-01: Cleaned stale asset wording in the tree, viewer promote fallback
  comment, storage intake README, and restricted helper comment. Validated
  architecture map, doc references, taskboard, and viewer promote tests.
- 2026-07-01: Cleaned stale asset legacy wording; validated map, docs,
  taskboard, and viewer promote tests.
