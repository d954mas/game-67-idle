---
id: T0021
title: Expand 67 World progression to 30 release-track variants
status: review
epic: ""
priority: P1
tags: [progression,balance,state,art,native]
created: 2026-06-12
updated: 2026-06-12
---

## What

Expand 67 World from 18 variants to a 30-variant release-track progression
with matching persisted state, balance data, generated sprites, runtime pack
ids, native binding, and DevAPI/native evidence.

## Done when

- [x] State schema and generated state expose counts for variants 19-30.
- [x] `game_state_actions` exposes 30 variants, merge chain, reset logic,
      passive values, and next-goal labels.
- [x] Balance data lists variants 1-30 and includes release pacing targets.
- [x] Batch 3 source sheet exists in `gamedesign/meme-evolution/visuals/`.
- [x] Crop manifest creates transparent runtime PNGs for all 12 Batch 3
      sprites.
- [x] Pack/header include stable ids for Batch 3 sprites.
- [x] Native runtime binds and displays unique Batch 3 sprites.
- [x] Native scenario proves Rainbow -> Neon and Golden -> Cosmic progression,
      with screenshot evidence showing at least 3 Batch 3 variants.

## Open questions

None for this implementation slice. Exact one-hour tuning remains a later
simulation/playtest task after the 30-variant content path exists.

## Log

- 2026-06-12: Started after Batch 2 unique sprite integration. The release goal
  needs 25-30 variants and hour-long pacing; current runtime cap is 18.
- 2026-06-12: Added variants 19-30: Neon, Gummy, Pixel, Lava, Donut, Slime,
  Disco, Dragon, Ninja, Galaxy, Golden, Cosmic.
- 2026-06-12: Generated and saved Batch 3 source sheet:
  `gamedesign/meme-evolution/visuals/67-world-batch-3-character-sheet-v1.png`.
  Contact-sheet QA evidence: `tmp/67-world-batch3-runtime-contact-v2.png`.
- 2026-06-12: Evidence passed: `py -3.12 tools/state_codegen/generate_state.py`;
  `py -3.12 tools/assets/build_67_world_art.py`;
  `py -3.12 tools/assets/validate_67_world_pack_inputs.py`;
  `cmake --build --preset native-debug --target build_67_world_packs`;
  `build/game_seed/native-debug/build_67_world_packs.exe build/game_seed/67-world-packs`;
  `cmake --build --preset native-debug`.
- 2026-06-12: Native evidence passed:
  `py -3.12 tools/devapi/scenarios/extended_67_progression.py 9178 build/captures/scenarios/extended_67_progression_30_variants_v1.png`;
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9179 build/captures/scenarios/first_67_loop_30_variants_v1.png`;
  `py -3.12 tools/devapi/scenarios/state_roundtrip.py 9180`; pixel health
  passed for both screenshots.
