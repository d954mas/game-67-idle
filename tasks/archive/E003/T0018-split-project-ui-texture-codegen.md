---
id: T0018
title: Split project UI texture codegen
status: done
epic: E003
priority: P1
tags: [pipeline, assets, codegen]
created: 2026-06-15
updated: 2026-06-15
---

## What

Split closed-project fishing UI textures out of the temporary Rune Marches C
texture-array generator. Each project UI builder should own its own generated
header/source, so a future prototype does not inherit unrelated asset ids or
manifests from a closed game.

## Done when

- [x] `tools/assets/build_rune_marches_assets.py` no longer imports or embeds
  Roblox fishing UI assets.
- [x] `tools/assets/build_roblox_fishing_ui_assets.py` writes its own generated
  C header/source for fishing UI textures.
- [x] `src/main.c` uses the fishing generated asset ids through a separate
  texture set instead of `RUNE_ASSET_FISHING_*`.
- [x] CMake tracks the separate generated outputs and the native build passes.
- [x] Taskboard and reusable pipeline validation pass.

## Open questions

None; keep this as a project-boundary cleanup, not a product restart.

## Log
- 2026-06-15: Split Roblox Fishing UI generated C into
  `src/generated/roblox_fishing_ui_assets.gen.h/.c`, removed fishing imports and
  embeddings from `build_rune_marches_assets.py`, added
  `roblox_fishing_ui_asset_codegen`, and updated `src/main.c` to use
  `FISHING_ASSET_*` ids via a separate texture set. Evidence:
  `cmake --build --preset native-debug` passed;
  `node tools/assets/audit_project_asset_boundaries.mjs --name
  rune-marches-no-fishing-ui ...` passed; generated asset node tests passed.
- 2026-06-15: Final validation passed: `node tools/taskboard/cli.mjs
  validate`, `node --test tools/taskboard/test.mjs`, and
  `node tools/pipeline_validate.mjs`.
- 2026-06-15: Closed after project UI generated C split, native build, asset-boundary audit, and full pipeline validation passed.
