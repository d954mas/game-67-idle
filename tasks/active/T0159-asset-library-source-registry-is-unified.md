---
id: T0159
title: Asset library source registry is unified
status: review
epic: ""
priority: P1
tags: [assets, storage, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What

Make the global reusable asset library use the same explicit source-registry
model as templates and games. Review current asset storage and classify
remaining asset-related legacy surfaces.

## Done when

- [x] Global asset libraries are listed in `ai_studio/assets/storage/sources/libraries.json`.
- [x] Asset Viewer source list reads library sources from the registry.
- [x] Asset CLI defaults use the registered global library instead of a hidden hard-coded path.
- [x] Architecture map includes the new library registry nodes.
- [x] Current global library and legacy asset references are reviewed.

## Open questions

- Resolved by T0161: bootstrap/export moved to `ai_studio/bootstrap/`.

## Log

- 2026-07-01: Current source API now lists `global-library` from `libraries.json`
  and the `template` registry source. Search through the default registry source
  works and reports 3778 global records.
- 2026-07-01: Global asset facets: 3775 model, 2 texture, 1 font; 2925 CC0-1.0,
  849 CC-BY-4.0, 2 unknown, 1 CC0, 1 OFL-1.1.
- 2026-07-01: Found two accepted legacy intake examples still visible as
  unregistered because their original files remain in external `_incoming/`:
  `_incoming/khronos-gltf-sample-assets/box-glb/Box.glb` and
  `_incoming/poly-haven/brown-mud-leaves-01-diffuse/brown_mud_leaves_01_diff_1k.jpg`.
- 2026-07-01: Legacy asset tools are mostly gone from `tools/`; remaining
  current asset ownership is under `ai_studio/assets/` and
  `.codex/skills/nt-asset-workflow` / `nt-asset-image-generation`. Remaining
  legacy references are historical task logs, bootstrap/export docs, and
  non-asset module docs.
- 2026-06-30: Unified global library source registry; validated asset source tests 30/30, architecture map, taskboard, skills sync, and Asset Viewer source API.
- 2026-06-30: Review correction: defaultLibrarySourceRoot now prefers active global-library before falling back to another active library; asset tests 31/31 and source API smoke passed.
