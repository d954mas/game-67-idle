# 67 World Asset Pipeline

Current explicit art iteration path for native PC.

## Source Art

- `gamedesign/meme-evolution/visuals/67-world-ui-art-sheet-v1.png`
- `gamedesign/meme-evolution/visuals/67-world-reusable-ui-kit-v1.png`
- `gamedesign/meme-evolution/visuals/67-world-field-first-kit-v1.png`
- `gamedesign/meme-evolution/visuals/67-world-field-repair-kit-v1.png`
- `gamedesign/meme-evolution/visuals/67-world-batch-2-character-sheet-v1.png`
- `gamedesign/meme-evolution/visuals/67-world-batch-3-character-sheet-v1.png`

## Contracts

- Art request: `gamedesign/meme-evolution/art_requests/67-world-reusable-ui-v1.json`
- Crop/slice9 source: `gamedesign/meme-evolution/data/art_crop_manifest.json`
- Runtime manifest: `gamedesign/meme-evolution/data/asset_manifest.json`

## Commands

```powershell
py -3.12 tools/assets/build_67_world_art.py
py -3.12 tools/assets/validate_67_world_pack_inputs.py
cmake --preset native-debug
cmake --build --preset native-debug --target build_67_world_packs
build/game_seed/native-debug/build_67_world_packs.exe build/game_seed/67-world-packs
```

## Outputs

- Runtime PNGs: `assets/runtime/67-world/`
- Generated C asset ids: `src/generated/assets/world67_assets.h`
- Pack: `build/game_seed/67-world-packs/world67_art.ntpack`

## Current Measurement

- Batch 3 sprite pack run: `world67_art.ntpack` built successfully with 77
  atlas sprites over 2 pages, `Cache: 5 hit / 3 miss`, builder timing total
  `0.1s`, pack size `20502.9 KB`.
- Pre-pack validation: `py -3.12 tools/assets/validate_67_world_pack_inputs.py`
  passed for 76 pack input assets.

Pack generation is explicit and is not wired into every normal game build.

## Native Runtime Evidence

- `build/captures/scenarios/first_67_loop_assets_v3.png`
- `build/captures/scenarios/first_67_loop_field_artkit_v2.png`
- `build/captures/scenarios/first_67_loop_field_repair_v2.png`
- `py -3.12 tools/devapi/scenarios/first_67_loop.py 9148 build/captures/scenarios/first_67_loop_assets_v3.png`
- `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_assets_v3.png`
- `py -3.12 tools/devapi/scenarios/first_67_loop.py 9167 build/captures/scenarios/first_67_loop_field_artkit_v2.png`
- `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_field_artkit_v2.png`
- `py -3.12 tools/devapi/scenarios/first_67_loop.py 9169 build/captures/scenarios/first_67_loop_field_repair_v2.png`
- `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_field_repair_v2.png`
- `tmp/67-world-batch2-runtime-contact-v2.png`
- `build/captures/scenarios/first_67_loop_batch2_art_v1.png`
- `build/captures/scenarios/extended_67_progression_batch2_art_v1.png`
- `py -3.12 tools/devapi/scenarios/first_67_loop.py 9177 build/captures/scenarios/first_67_loop_batch2_art_v1.png`
- `py -3.12 tools/devapi/scenarios/extended_67_progression.py 9176 build/captures/scenarios/extended_67_progression_batch2_art_v1.png`
- `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_batch2_art_v1.png`
- `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/extended_67_progression_batch2_art_v1.png`
- `tmp/67-world-batch3-runtime-contact-v2.png`
- `build/captures/scenarios/first_67_loop_30_variants_v1.png`
- `build/captures/scenarios/extended_67_progression_30_variants_v1.png`
- `py -3.12 tools/devapi/scenarios/first_67_loop.py 9179 build/captures/scenarios/first_67_loop_30_variants_v1.png`
- `py -3.12 tools/devapi/scenarios/extended_67_progression.py 9178 build/captures/scenarios/extended_67_progression_30_variants_v1.png`
- `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/first_67_loop_30_variants_v1.png`
- `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/extended_67_progression_30_variants_v1.png`
- `build/captures/scenarios/first_67_loop_better_crate_v1.png`
- `build/captures/scenarios/extended_67_progression_better_crate_v1.png`
- `build/captures/scenarios/better_crate_progression_v2.png`
- `py -3.12 tools/devapi/scenarios/better_crate_progression.py 9203 build/captures/scenarios/better_crate_progression_v2.png`
- `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/better_crate_progression_v2.png`

The screenshot must be a 960x540 framebuffer capture for the default native
test window. If a capture has a different size, treat it as fallback/window
evidence and investigate before judging art quality.

## Current Field-Kit QA Notes

- Included in pack/runtime: field grass, dark grass, light grass, path, fence
  post/rails, compact HUD panel, catalog drawer/panel, tutorial plaque, repaired
  field crate, selection rings, reward spark, ground shadow, and repaired
  blank green/disabled slice9 buttons.
- Repaired field assets use `magenta_edge` or `magenta_global` because the
  first field sheet proved green chroma key is unsafe for green-heavy art.
- The repaired blank buttons are pack-valid reusable UI, but are not currently
  used in the native top HUD because the first runtime placement hurt
  readability. Track that separately in `T0018`.
- Batch 2 character sprites are unique generated 67 variants, not tinted reuse
  of the first seven. Contact-sheet QA caught edge contamination from neighbor
  crops; the manifest now uses tighter crop boxes for clean transparent PNGs.
- Batch 3 character sprites extend the release-track progression to 30. The
  first contact sheet caught neighbor fragments; the second contact sheet
  verifies clean transparent runtime sprites before pack integration.
