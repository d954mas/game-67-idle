# Source Sheet Intake Audit: ember-road-town-forge-v2-ui-normalized-green.png

status: fail
analysis_engine: numpy
size: 1536x1024
component_count: 20
closest_gap_px: 0
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: keep_current_key_color
next_prompt_key_color: #00ff00
recommended_next_step: regenerate_source_sheet_with_more_gutter_and_safe_border

## Problems
- closest component gap 0px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 0
- components_with_key_hue_conflict: 0
- total_exact_key_conflict_px: 0
- total_key_fringe_hue_px: 0
- total_purple_halo_hue_px: 0
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.0, 'bbox': [53, 39, 308, 477]}

## Blocking Reasons
- code: unsafe_gutter, closest_gap_px: 0, minimum_px: 24, closest_pair: ['component_19', 'component_20'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_more_gutter_and_safe_border
- reason: components are too close to sheet borders or each other for reliable slicing
- key_color: #00ff00

## Components
- component_1: bbox=[53, 39, 308, 477] area=135940 border_gap=39 visible=135940 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_2: bbox=[406, 39, 606, 264] area=139112 border_gap=39 visible=139112 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_3: bbox=[1059, 66, 416, 117] area=47806 border_gap=61 visible=47806 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_4: bbox=[1059, 218, 416, 120] area=49010 border_gap=61 visible=49010 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_5: bbox=[557, 366, 232, 152] area=26563 border_gap=366 visible=26563 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_6: bbox=[390, 374, 140, 137] area=18862 border_gap=374 visible=18862 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_7: bbox=[1059, 374, 416, 120] area=49015 border_gap=61 visible=49015 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_8: bbox=[825, 376, 196, 137] area=21130 border_gap=376 visible=21130 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_9: bbox=[993, 539, 160, 202] area=17286 border_gap=283 visible=17286 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_10: bbox=[1230, 545, 201, 198] area=31103 border_gap=105 visible=31103 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_11: bbox=[64, 567, 171, 169] area=13717 border_gap=64 visible=13717 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_12: bbox=[717, 569, 159, 160] area=11694 border_gap=295 visible=11694 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_13: bbox=[496, 570, 161, 157] area=10627 border_gap=297 visible=10627 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_14: bbox=[283, 571, 164, 163] area=12492 border_gap=283 visible=12492 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_15: bbox=[932, 769, 536, 54] area=7759 border_gap=68 visible=7759 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_16: bbox=[63, 771, 169, 173] area=13563 border_gap=63 visible=13563 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_17: bbox=[277, 776, 163, 160] area=14521 border_gap=88 visible=14521 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_18: bbox=[490, 778, 156, 159] area=11318 border_gap=87 visible=11318 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_19: bbox=[688, 778, 156, 163] area=13134 border_gap=83 visible=13134 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_20: bbox=[834, 859, 657, 101] area=56488 border_gap=45 visible=56488 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #00ffff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #00ff00: exact=0 exact_ratio=0.0 hue_band=12699 hue_ratio=0.018112 score=12700.811193
- #ffff00: exact=1 exact_ratio=1e-06 hue_band=20852 hue_ratio=0.02974 score=21854.974014
- #ff0000: exact=0 exact_ratio=0.0 hue_band=109182 hue_ratio=0.155721 score=109197.572068
