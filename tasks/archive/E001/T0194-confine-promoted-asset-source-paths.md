---
id: T0194
title: Confine promoted asset source paths
status: done
epic: E001
priority: P1
tags: [assets, viewer, promote, security, test]
created: 2026-07-01
updated: 2026-07-01
---

## What

`promote.mjs` reads selected files from a review manifest using `--repo` plus
manifest-relative paths. Those paths are inputs and must stay inside `--repo`;
otherwise a malformed manifest can copy outside files into the asset library.

## Done when

- [x] Asset `relpath` entries that escape `--repo` are rejected before copying.
- [x] License file entries that escape `--repo` are rejected before copying.
- [x] Normal promote behavior and license metadata tests still pass.
- [x] Focused asset viewer/storage checks and core map/doc/taskboard checks pass.

## Open questions

## Log

- 2026-07-01: Created after review found `promote.mjs` resolved manifest asset
  paths without checking that they stay under `--repo`.
- 2026-07-01: Added `resolveManifestPath()` and regression coverage for escaping
  asset `relpath` and `licenseFile` manifest entries.
- 2026-07-01: Validation passed: promote tests 11/11, asset viewer tests 31/31,
  asset storage/source/license/intake/manifest/snapshot/preview tests 57/57,
  `node --check ai_studio/assets/viewer/promote.mjs`, `validate_map.mjs
  --strict`, `doc_reference_check.mjs`, and taskboard `validate --json`.
