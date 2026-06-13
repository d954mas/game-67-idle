---
id: T0017
title: Repair rejected 67 World field art kit assets
status: review
epic: ""
priority: P1
tags: [art, pipeline, visual, native]
created: 2026-06-12
updated: 2026-06-12
---

## What

Repair or regenerate the field-first kit assets that were intentionally excluded
from `world67_art.ntpack` during the first generated field-art integration pass:
green button state, disabled field button, ground shadow, selection rings/spark,
and generated field crate.

The repair must preserve the reusable UI rule: blank states and separate
effects, no baked labels, no counters, no random text, no full-screen board art.

## Done when

- [x] Source sheet or edited replacements use a key color compatible with the
      asset colors, preferably magenta for green-heavy art.
- [x] Crop manifest records the correct `chroma_mode`, pivots, and slice9
      values for repaired assets.
- [x] Runtime PNG alpha validation proves every pack-included repaired asset
      has a non-empty alpha bbox.
- [x] `build_67_world_packs.exe build/game_seed/67-world-packs` includes the
      repaired assets without atlas trim failures.
- [x] Native screenshot evidence shows at least one repaired asset in runtime
      without bad crops, green fringe, or incoherent overlap.

## Open questions

None.

## Log

- 2026-06-12: Created after `field_artkit_v2` integration. The first packable
  field kit intentionally included only stable grass/path/fence/HUD/catalog
  assets; rejected button/effect/crate assets need a targeted repair pass.
- 2026-06-12: Repaired assets via
  `gamedesign/meme-evolution/visuals/67-world-field-repair-kit-v1.png` on
  magenta key, added `magenta_edge`/`magenta_global` slicer modes, restored
  repaired assets to `world67_art.ntpack`, and validated native screenshot
  `build/captures/scenarios/first_67_loop_field_repair_v2.png`. The generated
  button assets are pack-valid reusable slice9 controls, but not currently used
  in the top HUD because that integration reduced readability.
