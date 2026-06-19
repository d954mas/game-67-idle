---
id: T0027
title: Source stronger hero mech asset
status: review
epic: ""
priority: P0
tags: [implementation, visual, assets, 3d, mechs, native, sourcing]
created: 2026-06-20
updated: 2026-06-20
---

## What

Replace the current "improved but still weak" hero read with a stronger
downloaded mech source candidate. The first target is Poly Pizza `Mech Assault
Walker` by Alimayo Arango because its cockpit, twin guns, and chunky legs read
more like a real mech than the current single Quaternius body.

Scope boundaries:

- In scope: candidate screening, source GLB/preview download, license/provenance
  record, material-split runtime import, native screenshot, and strict visual
  gate.
- Out of scope: copying Roblox/Mech Arena/IP assets, full animation rig import,
  new combat systems, economy changes, web/mobile export, or UI redesign.

## Done when

- [x] Candidate source note records URL, author, license, attribution
      requirement, local source paths, and texture/material usage class.
- [x] Runtime hangar uses the stronger sourced mech silhouette instead of the
      previous weak hero body as the primary visible hero.
- [x] Native `game_seed` builds and DevAPI smoke captures a T0027 screenshot.
- [x] Strict visual product gate passes with `art_quality >= 4`, or fail is
      logged with next corrective action.
- [x] `node tools/taskboard/cli.mjs validate` passes.

## Log

- 2026-06-20: Created after T0026. Visual priority is now true downloaded asset
  sourcing/replacement, not more overlay polish.
- 2026-06-20: Selected Poly Pizza `Mech Assault Walker` by Alimayo Arango
  (CC-BY 3.0), stored source GLB/preview, and integrated 13 material-split
  runtime meshes as the primary hero render path.
- 2026-06-20: Native build and DevAPI smoke passed. Screenshots:
  `build/captures/mech_t0027_assault_walker_hero_hangar_smoke.png` and
  `build/captures/mech_t0027_assault_walker_battle_smoke.png`.
- 2026-06-20: product gate PASS (desktop-assault-walker-hero); review:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T00-36-10_desktop-assault-walker-hero.md`;
  screenshot:
  `build/captures/mech_t0027_assault_walker_hero_hangar_smoke.png`; next:
  continue to the next narrow slice.
- 2026-06-20: Validation passed: `node tools/taskboard/cli.mjs validate`,
  `node tools/ai.mjs validate --with-assets`, and `git diff --check`.
