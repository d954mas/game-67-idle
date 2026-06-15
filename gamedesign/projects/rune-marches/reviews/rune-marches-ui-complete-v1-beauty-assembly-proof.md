# UI Composition Proof

asset_manifest: `gamedesign/projects/rune-marches/data/rune-marches-ui-complete-v1-asset_manifest.json`
output: `gamedesign/projects/rune-marches/art/previews/rune-marches-ui-complete-v1-beauty-assembly-proof.png`
verdict: **pass**

## Timing

- render_items: 160.741 ms
- render_sheet: 13.758 ms
- save_output: 27.199 ms
- total: 208.214 ms

## Cache

- image_hits: 0
- image_misses: 9
- slice_tile_hits: 0
- slice_tile_misses: 3
- resized_tile_hits: 0
- resized_tile_misses: 15
- panel_hits: 0
- panel_misses: 3
- overlay_resize_hits: 0
- overlay_resize_misses: 6

## Items

- PASS `compact_journal_panel_v5` 300x380
  - overlay `decor_corner_top_left_v1` source=[246, 245] render=[76, 76] mode=size rect=[0, 0, 76, 76] anchor=top_left
  - overlay `decor_corner_top_right_v1` source=[246, 245] render=[76, 76] mode=size rect=[224, 0, 76, 76] anchor=top_right
  - overlay `decor_wooden_plaque_v1` source=[276, 121] render=[150, 66] mode=size rect=[75, 304, 150, 66] anchor=bottom_center
- PASS `compact_button_idle_long_v5` 376x72
  - overlay `decor_blue_diamond_v1` source=[217, 228] render=[44, 46] mode=size rect=[10, 13, 44, 46] anchor=left_mid
  - overlay `icon_rune_crystal_v1` source=[236, 238] render=[42, 42] mode=size rect=[324, 15, 42, 42] anchor=right_mid
- PASS `compact_button_disabled_medium_v5` 256x72
  - overlay `icon_locked_pin_v1` source=[174, 259] render=[34, 50] mode=size rect=[12, 11, 34, 50] anchor=left_mid
