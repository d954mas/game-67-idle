---
id: T0037
title: Station plastic world composition
status: review
priority: P0
tags: [visual, materials, world, props, native]
created: 2026-06-20
updated: 2026-06-20
---

# T0037 - Station Plastic World Composition

## Why

T0036 imported real CC0 Kenney sci-fi station props, but the hangar screenshot
still reads too flat: the props use one runtime tint each and the world frame is
thin around the hero mech. The next visual pass should make the sourced props
feel like a juicy Roblox-like toy station without expanding combat scope.

## What

- In scope: add a dedicated plastic/station shader for the sourced Kenney
  props, reuse the same downloaded props as additional hangar landmarks, update
  screenshot proof and product gate evidence.
- Out of scope: new downloaded asset search, combat/economy changes, full level
  editor, atlas/trim-sheet generation, or web/mobile export.

## Done when

- [x] Kenney props use a dedicated station/plastic material rather than the
      generic solid overlay material.
- [x] The hangar reuses sourced station props to frame the hero mech more
      clearly without covering UI or the main mech silhouette.
- [x] DevAPI smoke captures `mech_t0037_station_plastic_hangar_smoke.png`.
- [x] Strict product gate records visual scores and remaining material/compo
      debt.
- [x] Taskboard validation passes.

## Activity Log

- 2026-06-20: Created to follow T0036 with a narrow material/composition pass
  before adding new gameplay or more asset families.
- 2026-06-20: Added dedicated Kenney station/plastic shader, increased sourced
  prop hangar framing from 3 to 8 instances, and captured
  `build/captures/mech_t0037_station_plastic_hangar_smoke.png`.
- 2026-06-19: product gate PASS (desktop-station-plastic-world-composition); review: gamedesign\projects\mech-builder-battler\reviews\product_read_gate_2026-06-20T02-24-40_desktop-station-plastic-world-composition.md; screenshot: build/captures/mech_t0037_station_plastic_hangar_smoke.png; next: continue to the next narrow slice
