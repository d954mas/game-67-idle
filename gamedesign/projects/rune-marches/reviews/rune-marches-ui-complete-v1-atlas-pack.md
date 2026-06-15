# UI Atlas Review Pack

purpose: review/validation atlas, not the engine runtime pack
asset_manifest: `gamedesign/projects/rune-marches/data/rune-marches-ui-complete-v1-asset_manifest.json`
output_dir: `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1`
atlases: **3**

## Labeled Preview Policy

Names are drawn only on labeled preview PNGs, in reserved free-space label rects outside asset padded rects.
- mode: `label_overlay_only`
- allowed_delta: `review_label_rects_only`
- debug_outlines: `false`
- font_size: `18`

## Atlas Efficiency

- occupancy_ratio: 0.7365
- padded_asset_ratio: 0.6321
- atlas_area: 2649936
- reserved_tile_area: 1951604
- padded_asset_area: 1674972

## Timing

- read_manifest: 0.356 ms
- load_assets: 61.816 ms
- pack_groups: 243.266 ms
- total: 306.267 ms

## Atlases

- `ui_rune_marches_compact_bases_v5` -> `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_compact_bases_v5.png` 803x1113, entries=7, physical=7, aliases=0, occupancy=0.6647, labeled_preview=`gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_compact_bases_v5-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false
- `ui_rune_marches_decor` -> `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_decor.png` 1087x777, entries=12, physical=12, aliases=0, occupancy=0.7658, labeled_preview=`gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_decor-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false
- `ui_rune_marches_icons` -> `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_icons.png` 1109x822, entries=12, physical=12, aliases=0, occupancy=0.7797, labeled_preview=`gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_icons-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false

## Asset Id Index

### ui_rune_marches_compact_bases_v5

labeled_preview: `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_compact_bases_v5-labeled.png`

- `compact_button_disabled_long_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_disabled_long_v5_slice9.png`, atlas_rect=[355, 3, 445, 119], padded_rect=[353, 1, 449, 123], label_rect=[353, 127, 163, 42], label_placement=bottom, label_lines=['compact_button_', 'disabled_long_v5']
- `compact_button_disabled_medium_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_disabled_medium_v5_slice9.png`, atlas_rect=[453, 776, 298, 119], padded_rect=[451, 774, 302, 123], label_rect=[451, 900, 196, 42], label_placement=bottom, label_lines=['compact_button_', 'disabled_medium_v5']
- `compact_button_disabled_short_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_disabled_short_v5_slice9.png`, atlas_rect=[307, 946, 211, 119], padded_rect=[305, 944, 215, 123], label_rect=[305, 1070, 167, 42], label_placement=bottom, label_lines=['compact_button_', 'disabled_short_v5']
- `compact_button_idle_long_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_idle_long_v5_slice9.png`, atlas_rect=[3, 776, 444, 119], padded_rect=[1, 774, 448, 123], label_rect=[1, 900, 204, 42], label_placement=bottom, label_lines=['compact_button_idle_', 'long_v5']
- `compact_button_idle_medium_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_idle_medium_v5_slice9.png`, atlas_rect=[3, 946, 298, 119], padded_rect=[1, 944, 302, 123], label_rect=[1, 1070, 204, 42], label_placement=bottom, label_lines=['compact_button_idle_', 'medium_v5']
- `compact_button_idle_short_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_idle_short_v5_slice9.png`, atlas_rect=[524, 946, 211, 118], padded_rect=[522, 944, 215, 122], label_rect=[522, 1069, 204, 42], label_placement=bottom, label_lines=['compact_button_idle_', 'short_v5']
- `compact_journal_panel_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_journal_panel_v5_slice9.png`, atlas_rect=[3, 3, 346, 722], padded_rect=[1, 1, 350, 726], label_rect=[1, 730, 224, 42], label_placement=bottom, label_lines=['compact_journal_panel_', 'v5']

### ui_rune_marches_decor

labeled_preview: `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_decor-labeled.png`

- `decor_blue_diamond_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_blue_diamond_v1.png`, atlas_rect=[197, 337, 217, 228], padded_rect=[195, 335, 221, 232], label_rect=[195, 570, 223, 22], label_placement=bottom, label_lines=['decor_blue_diamond_v1']
- `decor_compass_badge_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_compass_badge_v1.png`, atlas_rect=[838, 3, 246, 242], padded_rect=[836, 1, 250, 246], label_rect=[836, 250, 220, 42], label_placement=bottom, label_lines=['decor_compass_badge_', 'v1']
- `decor_corner_top_left_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_corner_top_left_v1.png`, atlas_rect=[334, 3, 246, 245], padded_rect=[332, 1, 250, 249], label_rect=[332, 253, 210, 42], label_placement=bottom, label_lines=['decor_corner_top_left_', 'v1']
- `decor_corner_top_right_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_corner_top_right_v1.png`, atlas_rect=[586, 3, 246, 245], padded_rect=[584, 1, 250, 249], label_rect=[584, 253, 222, 42], label_placement=bottom, label_lines=['decor_corner_top_right_', 'v1']
- `decor_crystal_cluster_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_crystal_cluster_v1.png`, atlas_rect=[422, 337, 202, 220], padded_rect=[420, 335, 206, 224], label_rect=[420, 562, 206, 42], label_placement=bottom, label_lines=['decor_crystal_cluster_', 'v1']
- `decor_leather_tab_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_leather_tab_v1.png`, atlas_rect=[3, 3, 116, 263], padded_rect=[1, 1, 120, 267], label_rect=[1, 271, 119, 62], label_placement=bottom, label_lines=['decor_', 'leather_tab_', 'v1']
- `decor_lock_plate_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_lock_plate_v1.png`, atlas_rect=[3, 337, 188, 236], padded_rect=[1, 335, 192, 240], label_rect=[1, 578, 188, 22], label_placement=bottom, label_lines=['decor_lock_plate_v1']
- `decor_red_seal_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_red_seal_v1.png`, atlas_rect=[125, 3, 203, 248], padded_rect=[123, 1, 207, 252], label_rect=[123, 256, 172, 22], label_placement=bottom, label_lines=['decor_red_seal_v1']
- `decor_ribbon_banner_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_ribbon_banner_v1.png`, atlas_rect=[3, 608, 313, 121], padded_rect=[1, 606, 317, 125], label_rect=[1, 734, 225, 22], label_placement=bottom, label_lines=['decor_ribbon_banner_v1']
- `decor_silver_divider_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_silver_divider_v1.png`, atlas_rect=[604, 608, 261, 79], padded_rect=[602, 606, 265, 83], label_rect=[602, 692, 216, 22], label_placement=bottom, label_lines=['decor_silver_divider_v1']
- `decor_sunburst_gem_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_sunburst_gem_v1.png`, atlas_rect=[630, 337, 189, 171], padded_rect=[628, 335, 193, 175], label_rect=[628, 513, 153, 42], label_placement=bottom, label_lines=['decor_sunburst_', 'gem_v1']
- `decor_wooden_plaque_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_wooden_plaque_v1.png`, atlas_rect=[322, 608, 276, 121], padded_rect=[320, 606, 280, 125], label_rect=[320, 734, 216, 42], label_placement=bottom, label_lines=['decor_wooden_plaque_', 'v1']

### ui_rune_marches_icons

labeled_preview: `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_icons-labeled.png`

- `icon_health_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_health_v1.png`, atlas_rect=[221, 580, 232, 212], padded_rect=[219, 578, 236, 216], label_rect=[219, 797, 141, 22], label_placement=bottom, label_lines=['icon_health_v1']
- `icon_locked_pin_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_locked_pin_v1.png`, atlas_rect=[246, 3, 174, 259], padded_rect=[244, 1, 178, 263], label_rect=[244, 267, 179, 22], label_placement=bottom, label_lines=['icon_locked_pin_v1']
- `icon_mana_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_mana_v1.png`, atlas_rect=[695, 311, 170, 234], padded_rect=[693, 309, 174, 238], label_rect=[693, 550, 136, 22], label_placement=bottom, label_lines=['icon_mana_v1']
- `icon_quest_scroll_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_quest_scroll_v1.png`, atlas_rect=[245, 311, 239, 235], padded_rect=[243, 309, 243, 239], label_rect=[243, 551, 190, 22], label_placement=bottom, label_lines=['icon_quest_scroll_v1']
- `icon_rune_crystal_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_rune_crystal_v1.png`, atlas_rect=[3, 311, 236, 238], padded_rect=[1, 309, 240, 242], label_rect=[1, 554, 194, 22], label_placement=bottom, label_lines=['icon_rune_crystal_v1']
- `icon_shield_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_shield_v1.png`, atlas_rect=[490, 311, 199, 235], padded_rect=[488, 309, 203, 239], label_rect=[488, 551, 137, 22], label_placement=bottom, label_lines=['icon_shield_v1']
- `icon_silver_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_silver_v1.png`, atlas_rect=[3, 580, 212, 214], padded_rect=[1, 578, 216, 218], label_rect=[1, 799, 133, 22], label_placement=bottom, label_lines=['icon_silver_v1']
- `icon_star_token_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_star_token_v1.png`, atlas_rect=[863, 3, 243, 242], padded_rect=[861, 1, 247, 246], label_rect=[861, 250, 178, 22], label_placement=bottom, label_lines=['icon_star_token_v1']
- `icon_sun_medallion_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_sun_medallion_v1.png`, atlas_rect=[609, 3, 248, 246], padded_rect=[607, 1, 252, 250], label_rect=[607, 254, 213, 22], label_placement=bottom, label_lines=['icon_sun_medallion_v1']
- `icon_travel_boot_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_travel_boot_v1.png`, atlas_rect=[3, 3, 237, 259], padded_rect=[1, 1, 241, 263], label_rect=[1, 267, 186, 22], label_placement=bottom, label_lines=['icon_travel_boot_v1']
- `icon_unlocked_pin_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_unlocked_pin_v1.png`, atlas_rect=[427, 3, 171, 257], padded_rect=[425, 1, 175, 261], label_rect=[425, 265, 180, 42], label_placement=bottom, label_lines=['icon_unlocked_pin_', 'v1']
- `icon_warden_badge_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_warden_badge_v1.png`, atlas_rect=[871, 311, 231, 233], padded_rect=[869, 309, 235, 237], label_rect=[869, 549, 215, 22], label_placement=bottom, label_lines=['icon_warden_badge_v1']
