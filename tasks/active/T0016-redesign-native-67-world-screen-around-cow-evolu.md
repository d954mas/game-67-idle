---
id: T0016
title: Redesign native 67 World screen around Cow Evolution field-first gameplay
status: review
epic: ""
priority: P1
tags: [gameplay, visual, ui, reference, native]
created: 2026-06-12
updated: 2026-06-12
---

## What

Redesign the native PC first playable screen so it matches the useful gameplay
grammar of Cow Evolution without copying its IP: a field-first world where
crates and 67 characters are the main interaction, not a stack of UI cards and
cropped panels.

## Done when

- [x] `cow_evolution_deconstruction_v2.md` is used as the implementation brief.
- [x] Native PC screen shows a coherent 67 World field with crates/characters
      as game objects.
- [x] Player-facing merge is direct field interaction; DevAPI shortcuts may
      remain hidden.
- [x] Top HUD and collection drawer are compact supporting UI, not the main
      visual mass.
- [x] Badly cropped/generated UI pieces are removed, replaced, or hidden behind
      coherent runtime composition.
- [x] Native screenshot evidence proves the first minute is understandable:
      crate -> Tiny -> second Tiny -> merge -> Berry/reward.

## Open questions

None before the first corrective implementation pass.

## Log

- 2026-06-12: Created after user feedback that the current screen is visually
  bad and the gameplay relationship to Cow Evolution is unclear.
- 2026-06-12: Implemented the first native PC field-first corrective pass in
  `src/main.c`: organic in-world 67 positions, in-field crate, compact HUD,
  compact catalog drawer, and generated character sprites without the old
  cropped board/card/button UI layer. Evidence: `cmake --build --preset
  native-debug`, `py -3.12 tools/devapi/scenarios/first_67_loop.py 9165
  build/captures/scenarios/first_67_loop_field_first_v5.png`, and `py -3.12
  tools/devapi/pixel_health.py
  build/captures/scenarios/first_67_loop_field_first_v5.png`.
- 2026-06-12: Added and integrated the first generated reusable field art kit:
  `gamedesign/meme-evolution/visuals/67-world-field-first-kit-v1.png`,
  `field_*` crop/asset manifest entries, pack ids for grass/dark grass/light
  grass/path/fence/HUD/catalog/tutorial plaque, and native runtime composition
  in `src/main.c`. Evidence: `py -3.12 tools/assets/build_67_world_art.py`,
  `build/game_seed/native-debug/build_67_world_packs.exe
  build/game_seed/67-world-packs`, `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9167
  build/captures/scenarios/first_67_loop_field_artkit_v2.png`, and `py -3.12
  tools/devapi/pixel_health.py
  build/captures/scenarios/first_67_loop_field_artkit_v2.png`. QA note:
  green button, ground shadow, generated selection rings/spark, and generated
  crate were not included in the pack because alpha/keying or atlas validation
  failed; they need a targeted repair pass before use.
