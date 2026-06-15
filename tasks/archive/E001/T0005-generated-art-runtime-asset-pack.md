---
id: T0005
title: Generated art runtime asset pack
status: done
epic: E001
priority: P0
tags: [art, assets, visual, runtime, web]
created: 2026-06-13
updated: 2026-06-15
---

## What

Turn the selected Rune Marches generated/free visual direction into reusable
runtime assets: map background, landmark icons, Mire Wisp sprite, Spark effect,
resource icons, and slice9 UI/button panels. Visual quality is prioritized over
minimal file size for the first audience-testable build, but runtime copies
should still be prepared for web/mobile compression.

## Done when

- [x] Source art and selected direction are documented in
  `gamedesign/projects/rune-marches/art/art_direction.md`.
- [x] Crop/slice manifest lists accepted source sheets, crop boxes, pivots,
  transparency/chroma-key rules, and slice9 margins.
- [x] Runtime assets are generated under `assets/runtime/rune-marches-v1/`.
- [x] Native runtime uses at least the map background, landmark icons, enemy
  sprite, and UI panel/button assets instead of shape-only placeholders.
- [x] Desktop and portrait screenshots prove no critical overlap and readable
  first action.
- [x] Web/mobile asset-size plan is recorded before `T0001` validation.

## Open questions

- Should runtime use PNG first and optimize to WebP/compressed pack later, or
  wire compressed pack output immediately?
- Are we accepting the current fake shot's darker RPG frame, or should the next
  candidate be brighter/more arcade for Poki?

## Log

- 2026-06-13: Created after user explicitly allowed generated/free assets and
  clarified that visual quality matters more than file size. Current visual
  source: `gamedesign/projects/rune-marches/art/fake_shots/rune-marches-gameplay-v1.png`.
- 2026-06-13: Implemented reproducible crop/codegen path via
  `tools/assets/build_rune_marches_assets.py`, generated runtime PNGs in
  `assets/runtime/rune-marches-v1/`, and integrated native `nt_gfx` texture
  quads for map, landmarks, Wispfen panel sprite, Mire Wisp sprite, and
  panel/button assets. Validated with native build, smoke/full probes, and
  desktop + portrait scenario screenshots:
  `tmp/rune_marches/native_first_slice_labeled.png`,
  `tmp/rune_marches/native_first_slice_portrait_current.png`.
- 2026-06-15: Archived during pipeline cleanup; generated art runtime pack remains historical evidence.
