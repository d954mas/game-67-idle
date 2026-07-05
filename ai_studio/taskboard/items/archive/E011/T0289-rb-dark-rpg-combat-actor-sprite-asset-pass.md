---
id: T0289
title: "rb-dark-rpg: combat actor sprite asset pass"
status: done
project: P003
epic: E011
priority: P2
tags: [rb-dark-rpg, combat, visual, assets]
created: 2026-07-05
updated: 2026-07-05
---

## What

Conditional later iteration for combat actor visuals. Do this only if the
T0286/T0288 review shows placeholder silhouettes cannot sell the animated
clash strongly enough.

Source assets before generation: check the shared asset library and free
CC0/OFL sources first. Generate new raster actor/impact assets only if no
usable sourced fit exists, and record license, provenance, integrity, and
origin for every committed asset.

Scope:

- define minimum actor asset needs for the combat stage: player silhouette,
  enemy silhouette, shadow/grounding, and optional impact flash/slash elements;
- source or generate separated production assets, not a painted full-screen
  combat poster;
- update asset manifests/provenance before wiring assets into runtime;
- verify composed gameplay captures after wiring.

Out of scope:

- changing combat math, reward flow, or active-skill design;
- adding hand/weapon frame animation requirements;
- committing paid or non-redistributable binaries.

## Done when

- [x] The task log records why placeholders were insufficient and links the
      T0286/T0288 review evidence that triggered this pass.
- [x] Asset sourcing order is documented: shared library, free CC0/OFL sources,
      then generation only if needed.
- [x] Every committed asset has license, provenance, integrity, and `origin`.
- [x] Runtime wiring uses separated actor/impact assets that can be animated by
      transforms in the existing combat stage.
- [x] Desktop and phone captures prove the new assets improve the clash without
      obscuring HP, impact damage, stats, or log.
- [x] Existing combat/unit scenario checks still pass.

## Open questions

- Keep this as `idea` until the placeholder-stage review proves it is needed.

## Log

- 2026-07-05: Created as a conditional asset iteration after T0286/T0288, not
  part of the first animated-stage implementation.
- 2026-07-05: User explicitly requested hero/enemy art after the animated placeholder clash. Local shared-asset search returned 0 usable combat actor sprites; CC0 review found Kenney 16x16 top-down/pixel packs with usable license but wrong scale/style for the side-view clash, so this pass proceeds to generated composable actor sprites with provenance.
- 2026-07-05: Implemented generated composable combat actor sprites for hero, gate scavenger, and mill scavenger; wired them through the rb-dark-rpg UI atlas and combat stage transforms. Evidence: game_combat_test/first_scene_tests/game build passed, scenarios_test OK, QCLR_002 responsive captures for gate and mill running combat passed across 6 viewports each.
- 2026-07-05: Trigger source for this pass is the explicit user request to replace placeholder combat silhouettes with hero/enemy art after the first animated clash review. Provenance lives in games/rb-dark-rpg/assets/ui/generated/combat_actor_sprites_01/.
