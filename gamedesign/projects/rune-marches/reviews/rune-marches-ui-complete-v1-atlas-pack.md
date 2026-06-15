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

## Atlas Efficiency

- occupancy_ratio: 0.7415
- padded_asset_ratio: 0.6679
- atlas_area: 2507662
- reserved_tile_area: 1859436
- padded_asset_area: 1674972

## Timing

- read_manifest: 0.337 ms
- load_assets: 36.633 ms
- pack_groups: 255.641 ms
- total: 293.214 ms

## Atlases

- `ui_rune_marches_compact_bases_v5` -> `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_compact_bases_v5.png` 803x1073, entries=7, physical=7, aliases=0, occupancy=0.6508, labeled_preview=`gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_compact_bases_v5-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false
- `ui_rune_marches_decor` -> `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_decor.png` 877x879, entries=12, physical=12, aliases=0, occupancy=0.7832, labeled_preview=`gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_decor-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false
- `ui_rune_marches_icons` -> `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_icons.png` 1105x792, entries=12, physical=12, aliases=0, occupancy=0.794, labeled_preview=`gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_icons-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false

## Asset Id Index

### ui_rune_marches_compact_bases_v5

labeled_preview: `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_compact_bases_v5-labeled.png`

- `compact_button_disabled_long_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_disabled_long_v5_slice9.png`, atlas_rect=[355, 3, 445, 119], padded_rect=[353, 1, 449, 123], label_rect=[353, 127, 198, 34], label_placement=bottom, label_lines=['compact_button_disabled_', 'long_v5']
- `compact_button_disabled_medium_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_disabled_medium_v5_slice9.png`, atlas_rect=[453, 752, 298, 119], padded_rect=[451, 750, 302, 123], label_rect=[451, 876, 198, 34], label_placement=bottom, label_lines=['compact_button_disabled_', 'medium_v5']
- `compact_button_disabled_short_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_disabled_short_v5_slice9.png`, atlas_rect=[307, 914, 211, 119], padded_rect=[305, 912, 215, 123], label_rect=[305, 1038, 198, 34], label_placement=bottom, label_lines=['compact_button_disabled_', 'short_v5']
- `compact_button_idle_long_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_idle_long_v5_slice9.png`, atlas_rect=[3, 752, 444, 119], padded_rect=[1, 750, 448, 123], label_rect=[1, 876, 218, 18], label_placement=bottom, label_lines=['compact_button_idle_long_v5']
- `compact_button_idle_medium_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_idle_medium_v5_slice9.png`, atlas_rect=[3, 914, 298, 119], padded_rect=[1, 912, 302, 123], label_rect=[1, 1038, 164, 34], label_placement=bottom, label_lines=['compact_button_idle_', 'medium_v5']
- `compact_button_idle_short_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_button_idle_short_v5_slice9.png`, atlas_rect=[524, 914, 211, 118], padded_rect=[522, 912, 215, 122], label_rect=[522, 1037, 223, 18], label_placement=bottom, label_lines=['compact_button_idle_short_v5']
- `compact_journal_panel_v5`: kind=slice9, source=`assets/runtime/rune-marches-ui-compact-bases-v5/compact_journal_panel_v5_slice9.png`, atlas_rect=[3, 3, 346, 722], padded_rect=[1, 1, 350, 726], label_rect=[1, 730, 198, 18], label_placement=bottom, label_lines=['compact_journal_panel_v5']

### ui_rune_marches_decor

labeled_preview: `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_decor-labeled.png`

- `decor_blue_diamond_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_blue_diamond_v1.png`, atlas_rect=[449, 309, 217, 228], padded_rect=[447, 307, 221, 232], label_rect=[447, 542, 181, 18], label_placement=bottom, label_lines=['decor_blue_diamond_v1']
- `decor_compass_badge_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_compass_badge_v1.png`, atlas_rect=[3, 309, 246, 242], padded_rect=[1, 307, 250, 246], label_rect=[1, 556, 195, 18], label_placement=bottom, label_lines=['decor_compass_badge_v1']
- `decor_corner_top_left_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_corner_top_left_v1.png`, atlas_rect=[334, 3, 246, 245], padded_rect=[332, 1, 250, 249], label_rect=[332, 253, 187, 18], label_placement=bottom, label_lines=['decor_corner_top_left_v1']
- `decor_corner_top_right_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_corner_top_right_v1.png`, atlas_rect=[586, 3, 246, 245], padded_rect=[584, 1, 250, 249], label_rect=[584, 253, 197, 18], label_placement=bottom, label_lines=['decor_corner_top_right_v1']
- `decor_crystal_cluster_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_crystal_cluster_v1.png`, atlas_rect=[672, 309, 202, 220], padded_rect=[670, 307, 206, 224], label_rect=[670, 534, 182, 18], label_placement=bottom, label_lines=['decor_crystal_cluster_v1']
- `decor_leather_tab_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_leather_tab_v1.png`, atlas_rect=[3, 3, 116, 263], padded_rect=[1, 1, 120, 267], label_rect=[1, 271, 115, 34], label_placement=bottom, label_lines=['decor_leather_', 'tab_v1']
- `decor_lock_plate_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_lock_plate_v1.png`, atlas_rect=[255, 309, 188, 236], padded_rect=[253, 307, 192, 240], label_rect=[253, 550, 152, 18], label_placement=bottom, label_lines=['decor_lock_plate_v1']
- `decor_red_seal_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_red_seal_v1.png`, atlas_rect=[125, 3, 203, 248], padded_rect=[123, 1, 207, 252], label_rect=[123, 256, 140, 18], label_placement=bottom, label_lines=['decor_red_seal_v1']
- `decor_ribbon_banner_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_ribbon_banner_v1.png`, atlas_rect=[198, 578, 313, 121], padded_rect=[196, 576, 317, 125], label_rect=[196, 704, 184, 18], label_placement=bottom, label_lines=['decor_ribbon_banner_v1']
- `decor_silver_divider_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_silver_divider_v1.png`, atlas_rect=[3, 776, 261, 79], padded_rect=[1, 774, 265, 83], label_rect=[1, 860, 174, 18], label_placement=bottom, label_lines=['decor_silver_divider_v1']
- `decor_sunburst_gem_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_sunburst_gem_v1.png`, atlas_rect=[3, 578, 189, 171], padded_rect=[1, 576, 193, 175], label_rect=[1, 754, 180, 18], label_placement=bottom, label_lines=['decor_sunburst_gem_v1']
- `decor_wooden_plaque_v1`: kind=decor_overlay, source=`assets/runtime/rune-marches-ui-decor-v1/decor_wooden_plaque_v1.png`, atlas_rect=[517, 578, 276, 121], padded_rect=[515, 576, 280, 125], label_rect=[515, 704, 192, 18], label_placement=bottom, label_lines=['decor_wooden_plaque_v1']

### ui_rune_marches_icons

labeled_preview: `gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_icons-labeled.png`

- `icon_health_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_health_v1.png`, atlas_rect=[221, 554, 232, 212], padded_rect=[219, 552, 236, 216], label_rect=[219, 771, 114, 18], label_placement=bottom, label_lines=['icon_health_v1']
- `icon_locked_pin_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_locked_pin_v1.png`, atlas_rect=[246, 3, 174, 259], padded_rect=[244, 1, 178, 263], label_rect=[244, 267, 145, 18], label_placement=bottom, label_lines=['icon_locked_pin_v1']
- `icon_mana_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_mana_v1.png`, atlas_rect=[695, 289, 170, 234], padded_rect=[693, 287, 174, 238], label_rect=[693, 528, 110, 18], label_placement=bottom, label_lines=['icon_mana_v1']
- `icon_quest_scroll_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_quest_scroll_v1.png`, atlas_rect=[245, 289, 239, 235], padded_rect=[243, 287, 243, 239], label_rect=[243, 529, 153, 18], label_placement=bottom, label_lines=['icon_quest_scroll_v1']
- `icon_rune_crystal_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_rune_crystal_v1.png`, atlas_rect=[3, 289, 236, 238], padded_rect=[1, 287, 240, 242], label_rect=[1, 532, 156, 18], label_placement=bottom, label_lines=['icon_rune_crystal_v1']
- `icon_shield_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_shield_v1.png`, atlas_rect=[490, 289, 199, 235], padded_rect=[488, 287, 203, 239], label_rect=[488, 529, 111, 18], label_placement=bottom, label_lines=['icon_shield_v1']
- `icon_silver_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_silver_v1.png`, atlas_rect=[3, 554, 212, 214], padded_rect=[1, 552, 216, 218], label_rect=[1, 773, 107, 18], label_placement=bottom, label_lines=['icon_silver_v1']
- `icon_star_token_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_star_token_v1.png`, atlas_rect=[857, 3, 243, 242], padded_rect=[855, 1, 247, 246], label_rect=[855, 250, 143, 18], label_placement=bottom, label_lines=['icon_star_token_v1']
- `icon_sun_medallion_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_sun_medallion_v1.png`, atlas_rect=[603, 3, 248, 246], padded_rect=[601, 1, 252, 250], label_rect=[601, 254, 172, 18], label_placement=bottom, label_lines=['icon_sun_medallion_v1']
- `icon_travel_boot_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_travel_boot_v1.png`, atlas_rect=[3, 3, 237, 259], padded_rect=[1, 1, 241, 263], label_rect=[1, 267, 149, 18], label_placement=bottom, label_lines=['icon_travel_boot_v1']
- `icon_unlocked_pin_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_unlocked_pin_v1.png`, atlas_rect=[426, 3, 171, 257], padded_rect=[424, 1, 175, 261], label_rect=[424, 265, 163, 18], label_placement=bottom, label_lines=['icon_unlocked_pin_v1']
- `icon_warden_badge_v1`: kind=icon, source=`assets/runtime/rune-marches-ui-icons-v1/icon_warden_badge_v1.png`, atlas_rect=[871, 289, 231, 233], padded_rect=[869, 287, 235, 237], label_rect=[869, 527, 174, 18], label_placement=bottom, label_lines=['icon_warden_badge_v1']
