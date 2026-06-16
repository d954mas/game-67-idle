---
id: T0002
title: "P0: Bright Roblox art bible + source sheets via agy"
status: todo
epic: E001
priority: P1
tags: [voxelheim, art, visual-first, agy]
created: 2026-06-16
updated: 2026-06-16
---

## What

Lock the Bright Roblox art bible and generate the first runtime-ready source art
for the "Frost Keep Approach" slice, using **agy** (`delegated-image-generation`
skill) and the `generated-game-ui-assets` pipeline. No runtime screen code here
(visual-first: art before the screen).

## Done when

- [ ] `gamedesign/projects/voxelheim/visual/art_bible.md` written: palette,
      materials, outline/shading, hero + enemy silhouette rules, environment
      kit, UI kit, FX, forbidden motifs — anchored to
      `visual/fake_shot_first_screen.png`.
- [ ] agy source sheets generated (Bright Roblox style): hero (idle/walk/attack),
      one frost enemy, environment kit (snow ground, stone path, Frost Keep +
      glowing portal, pines, rocks, distant mountains/dragon), UI kit (HP bar,
      stamina bar, level badge, minimap frame, hotbar slot, quest banner,
      primary button), FX (hit spark, loot sparkle, level-up burst).
- [ ] Sheets cut + audited via `generated-game-ui-assets` (intake → crop →
      runtime PNGs → pixel/atlas audits); atlas pack built.
- [ ] Contact sheets show assets reaching the fake-shot **quality bar** (visual
      review of the assembled set, not just clean crops).

## Open questions

- Tile/sprite resolution + atlas budget for `nt_sprite_renderer`?
- 3/4 top-down vs side framing — confirm against an engine sample scene.

## Log

- 2026-06-16 created. Theme A locked; accepted fake shot in `visual/`.
