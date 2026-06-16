---
id: T0070
title: Replace rough procedural UI with generated casual art
status: done
epic: E004
priority: P0
tags: []
created: 2026-06-16
updated: 2026-06-16
---

## What
Lead rejected the current visual/UI pass as rough, ugly, and not visibly
generated. Replace the procedural-looking casual UI and critter art with real
generated source assets, keep those sources in the project, and integrate the
selected cuts into the native runtime.

Visual session contract:
- Goal: make the current Critter Corral first screen read as bright, juicy,
  casual mobile game art from generated source assets.
- Non-goal: no gameplay, economy, content, web, or engine expansion while the
  first screen visual gate is failing.
- Proof: project-saved generated source images, prompt/provenance records,
  crop/runtime manifests, rebuilt native pack, desktop and portrait native
  screenshots, and product-read/visual gate notes.
- Stop condition: a product gate fail or lead rejection freezes feature/content
  expansion until the generated visual pass is fixed or the lead explicitly
  accepts the debt.
- Likely files: `gamedesign/projects/critter-corral/art/generated/`,
  `gamedesign/projects/critter-corral/art/sprites/`,
  `gamedesign/projects/critter-corral/reviews/`,
  `tools/critter_corral/`, `src/clean_seed_main.c`, and this task.

## Done when

- [x] Generated source art is saved inside the project, with prompt/provenance
      records.
- [x] Runtime sprites/UI are rebuilt from generated source assets, not only
      procedural Python drawing.
- [x] Native desktop and portrait screenshots show a colorful casual mobile UI
      that no longer reads as debug/prototype art.
- [x] A strict visual/product-read gate records remaining gaps or a pass.

## Open questions

## Log

- 2026-06-16: Reopened visual quality after lead rejection. Previous
  procedural polish is not acceptable as generated-art proof.
- 2026-06-16: Performance issue found: the shared chroma helper is too slow on
  large 1.5K generated PNGs for an art iteration loop. T0070 importer uses a
  faster NumPy path for the generated source images; the shared helper should
  get a separate profiling/optimization task if this appears outside this pass.
- 2026-06-16: product gate PASS (portrait-upgrade); review: gamedesign\projects\critter-corral\reviews\T0070_portrait_upgrade_generated_gate.md; screenshot: build/captures/corral_portrait_upgrade.png; next: continue to the next narrow slice
- 2026-06-16: product gate PASS (landscape-play); review: gamedesign\projects\critter-corral\reviews\T0070_landscape_play_generated_gate.md; screenshot: build/captures/corral_visual_ui_landscape_play.png; next: continue to the next narrow slice
- 2026-06-16: Generated source assets saved under
  `gamedesign/projects/critter-corral/art/generated/T0070/`; runtime cuts and
  manifests saved under `gamedesign/projects/critter-corral/art/sprites/` and
  `gamedesign/projects/critter-corral/data/`. Pixel audit passed for 11
  generated-derived runtime assets.
