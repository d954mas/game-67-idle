# UI Atlas Pack Audit

atlas_pack: `gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-atlas_pack.json`
asset_manifest: `gamedesign/projects/rune-marches/data/rune-marches-ui-compact-bases-v5-asset_manifest.json`
verdict: **pass**

## Timing

- total: 89.276 ms

## Labeled Preview Policy

Audit requires labeled preview pixels to differ from the clean atlas only inside declared review_label rects.
- mode: `label_overlay_only`
- allowed_delta: `review_label_rects_only`
- debug_outlines: `false`

## Atlases

- PASS `ui_rune_marches_compact_bases_v5` entries=7, physical=7, aliases=0, transparent_nonzero_rgb_pixels=0, outside_padded_visible_pixels=0, labeled_preview_delta_outside_label_pixels=0, analysis_engine=numpy, labeled_preview=`assets/runtime/rune-marches-ui-compact-bases-v5-atlas/ui_rune_marches_compact_bases_v5-labeled.png`, labels=label_overlay_only/review_label_rects_only/debug_outlines=false
