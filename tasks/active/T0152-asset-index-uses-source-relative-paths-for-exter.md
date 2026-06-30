---
id: T0152
title: Asset index uses source-relative paths for external unregistered files
status: review
epic: E001
priority: P1
tags: [assets, index, viewer]
created: 2026-06-30
updated: 2026-06-30
---

## What

Raw/unregistered asset records must use paths relative to the selected asset
source, not paths relative to the git repository. External libraries such as
`C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets` must not show
`../../Users/...` in Asset Viewer or API results.

## Done when

- [x] Raw scan `asset_id` and `resource` are built from `source.path`.
- [x] External source outside repo root has a regression test.
- [x] Real global-library unregistered files show source-relative paths.
- [x] Asset index tests pass.
- [x] Full asset JS/Python validation passes.

## Open questions

## Log
- 2026-06-30: Start: fix external unregistered file paths to use asset-source-relative paths instead of repo-relative ../../Users paths.
- 2026-06-30: Fixed raw/unregistered asset scanner to use source-relative paths. Evidence: node --test ai_studio/assets/storage/index/tests/index.test.mjs; full asset JS tests 101/101; Python prep tests 61/61.
- 2026-06-30: Review fix: bumped asset index schemaVersion to force existing SQLite caches to rebuild after raw asset id/path normalization changed.
