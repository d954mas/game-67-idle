# UI Atlas Pack Audit

atlas_pack: `gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-atlas_pack.json`
asset_manifest: `gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-asset_manifest.json`
verdict: **pass**

## Labeled Preview Policy

Audit requires labeled preview pixels to differ from the clean atlas only inside declared review_label rects.
- mode: `label_overlay_only`
- allowed_delta: `review_label_rects_only`
- debug_outlines: `false`

## Asset Coverage

- expected_asset_ids: 7
- reported_asset_ids: 7
- missing_asset_ids: -
- unexpected_asset_ids: -

## Atlases

- PASS `ui_rune_marches_compact_bases_v5` entries=7, asset_ids=compact_button_disabled_long_v5,compact_button_disabled_medium_v5,compact_button_disabled_short_v5,compact_button_idle_long_v5,compact_button_idle_medium_v5,compact_button_idle_short_v5,compact_journal_panel_v5, physical=7, aliases=0, transparent_nonzero_rgb_pixels=0, outside_padded_visible_pixels=0, labeled_preview_delta_outside_label_pixels=0, analysis_engine=numpy, labeled_preview=`assets/runtime/rune-marches-ui-compact-bases-v5-atlas/ui_rune_marches_compact_bases_v5-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false
