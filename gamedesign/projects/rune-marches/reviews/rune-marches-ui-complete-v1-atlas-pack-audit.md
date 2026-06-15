# UI Atlas Pack Audit

atlas_pack: `gamedesign/projects/rune-marches/data/rune-marches-ui-complete-v1-atlas_pack.json`
asset_manifest: `gamedesign/projects/rune-marches/data/rune-marches-ui-complete-v1-asset_manifest.json`
verdict: **pass**

## Timing

- total: 250.983 ms

## Labeled Preview Policy

Audit requires labeled preview pixels to differ from the clean atlas only inside declared review_label rects.
- mode: `label_overlay_only`
- allowed_delta: `review_label_rects_only`
- debug_outlines: `false`

## Asset Coverage

- expected_asset_ids: 31
- reported_asset_ids: 31
- missing_asset_ids: -
- unexpected_asset_ids: -

## Atlases

- PASS `ui_rune_marches_compact_bases_v5` entries=7, asset_ids=compact_button_disabled_long_v5,compact_button_disabled_medium_v5,compact_button_disabled_short_v5,compact_button_idle_long_v5,compact_button_idle_medium_v5,compact_button_idle_short_v5,compact_journal_panel_v5, physical=7, aliases=0, transparent_nonzero_rgb_pixels=0, outside_padded_visible_pixels=0, labeled_preview_delta_outside_label_pixels=0, analysis_engine=numpy, labeled_preview=`gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_compact_bases_v5-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false
- PASS `ui_rune_marches_decor` entries=12, asset_ids=decor_blue_diamond_v1,decor_compass_badge_v1,decor_corner_top_left_v1,decor_corner_top_right_v1,decor_crystal_cluster_v1,decor_leather_tab_v1,decor_lock_plate_v1,decor_red_seal_v1,decor_ribbon_banner_v1,decor_silver_divider_v1,decor_sunburst_gem_v1,decor_wooden_plaque_v1, physical=12, aliases=0, transparent_nonzero_rgb_pixels=0, outside_padded_visible_pixels=0, labeled_preview_delta_outside_label_pixels=0, analysis_engine=numpy, labeled_preview=`gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_decor-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false
- PASS `ui_rune_marches_icons` entries=12, asset_ids=icon_health_v1,icon_locked_pin_v1,icon_mana_v1,icon_quest_scroll_v1,icon_rune_crystal_v1,icon_shield_v1,icon_silver_v1,icon_star_token_v1,icon_sun_medallion_v1,icon_travel_boot_v1,icon_unlocked_pin_v1,icon_warden_badge_v1, physical=12, aliases=0, transparent_nonzero_rgb_pixels=0, outside_padded_visible_pixels=0, labeled_preview_delta_outside_label_pixels=0, analysis_engine=numpy, labeled_preview=`gamedesign/projects/rune-marches/art/review-atlases/rune-marches-ui-complete-v1/ui_rune_marches_icons-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false
