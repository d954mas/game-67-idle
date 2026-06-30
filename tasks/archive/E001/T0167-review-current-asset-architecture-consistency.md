---
id: T0167
title: Review current asset architecture consistency
status: done
epic: E001
priority: P2
tags: [assets, review, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What

Review the current `ai_studio/assets` architecture after the storage/viewer
refactor. Focus on consistency, not another broad migration: source registries,
viewer routes, storage manifests/index/previews/license, asset skills, and
remaining legacy references must point to the same model.

## Done when

- [x] Current asset module structure and ownership boundaries are rechecked.
- [x] Remaining asset-related legacy references are classified as current,
      historical, or actionable follow-up.
- [x] Asset source registries and viewer/storage docs agree on source names,
      routes, and refresh behavior.
- [x] One concrete inconsistency or low-risk improvement is fixed.
- [x] Asset module tests and architecture/taskboard validation pass.

## Open questions

- None yet.

## Review

### Current Shape

- `ai_studio/assets/storage/` is the source-of-truth layer for explicit source
  registries, Pack Manifest metadata, generated SQLite indexes, preview cache,
  snapshots, intake, and license guard.
- `ai_studio/assets/viewer/` is the browser surface. It should use registered
  sources and generated indexes; it does not own storage, license policy, or
  preview rendering.
- `ai_studio/assets/prep/` is local preparation: conversion, source sheets,
  crops, cutouts, review atlases, and texture checks.
- `ai_studio/assets/workflow/` is generated-art/provenance workflow, not storage.
- `nt-asset-workflow` is the correct agent-facing router for asset work.

### Legacy Classification

- No asset-owned legacy tools remain in `tools/` according to the current map
  report. The remaining `unmappedLegacy` entries are mostly non-asset legacy
  skills, old epics, and generic tool libraries.
- Asset-related archive task files are historical evidence and should remain
  archived.
- Current asset docs had a few route/name drift issues rather than missing
  module ownership.

### Fixed

- Replaced the old viewer alias in asset workflow guidance and AI Studio docs
  with the stable route: `http://127.0.0.1:8765/asset_viewer/`.
- Changed the Asset Viewer nav link from `/viewer/` to `/asset_viewer/`.
- Replaced a stale `../storage/sources/*_registry.mjs` README reference with
  the real helpers: `libraries.mjs`, `templates.mjs`, and `games.mjs`.
- Replaced an ambiguous local search example using `--mode scan` with
  `--source-type local`.

### Validation

- `node --test ai_studio/assets/**/*.test.mjs`: 113 passed.
- `py -3.12 -m pytest ai_studio/assets/prep`: 65 passed.
- `node ai_studio/assets/storage/license/restricted_assets_guard.mjs`: ok.
- `node ai_studio/architecture_map/validate_map.mjs`: ok.
- `node ai_studio/taskboard/cli.mjs validate --json`: ok.
- `node ai_studio/core_harness/validation/doc_reference_check.mjs`: ok.
- `node ai_studio/core_harness/agent_surfaces/skills_sync.mjs --check`: ok.

## Log

- 2026-07-01: Created for the post-refactor asset architecture consistency pass.
- 2026-07-01: Rechecked asset module ownership and fixed route/helper naming
  drift across asset docs and viewer shell.
- 2026-06-30: Asset architecture consistency pass completed; stable asset_viewer route and current source helper names are aligned across docs and viewer.
