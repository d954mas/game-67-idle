---
id: T0019
title: Generate unique Batch 2 67 character sprites
status: review
epic: ""
priority: P1
tags: [art, characters, visual, native]
created: 2026-06-12
updated: 2026-06-12
---

## What

Replace the temporary tinted sprite reuse for Batch 2 variants 8-18 with
unique generated 67 character sprites:

- Jelly 67
- Lemon 67
- Watermelon 67
- Bubblegum 67
- Sticker 67
- Party 67
- Arcade 67
- Cloud 67
- Crown 67
- Rocket 67
- Rainbow 67

Every character must remain a variation of the 67 character, not a separate
fruit/object mascot.

## Done when

- [x] Art request packet lists Batch 2 character requirements and must-not-bake
      rules.
- [x] Source sheet exists in `gamedesign/meme-evolution/visuals/`.
- [x] Crop manifest creates transparent runtime PNGs for all 11 Batch 2 sprites.
- [x] Pack includes the sprites and generated asset header exposes stable ids.
- [x] Native runtime uses the unique sprites instead of tinted reuse.
- [x] Native screenshot shows at least 3 Batch 2 variants clearly in the field
      or collection tray.

## Open questions

None.

## Log

- 2026-06-12: Created after Batch 2 gameplay/state expansion. Current runtime
  proves variants 8-18 through state, DevAPI, and sliding collection UI, but it
  reuses existing sprites with tinting as a temporary visual bridge.
- 2026-06-12: Integrated source sheet
  `gamedesign/meme-evolution/visuals/67-world-batch-2-character-sheet-v1.png`
  into `art_crop_manifest.json`; tightened crop boxes after contact-sheet QA.
- 2026-06-12: Evidence contact sheet:
  `tmp/67-world-batch2-runtime-contact-v2.png`; all 11 Batch 2 sprites are
  transparent runtime PNGs in `assets/runtime/67-world/`.
- 2026-06-12: Pack evidence passed: `py -3.12 tools/assets/build_67_world_art.py`;
  `py -3.12 tools/assets/validate_67_world_pack_inputs.py`;
  `cmake --build --preset native-debug --target build_67_world_packs`;
  `build/game_seed/native-debug/build_67_world_packs.exe build/game_seed/67-world-packs`.
- 2026-06-12: Native evidence passed: `cmake --build --preset native-debug`;
  `py -3.12 tools/devapi/scenarios/extended_67_progression.py 9176 build/captures/scenarios/extended_67_progression_batch2_art_v1.png`;
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9177 build/captures/scenarios/first_67_loop_batch2_art_v1.png`;
  pixel health passed for both screenshots.
