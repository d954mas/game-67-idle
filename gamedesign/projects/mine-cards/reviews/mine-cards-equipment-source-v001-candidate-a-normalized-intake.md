# Source Sheet Intake Audit: mine-cards-equipment-source-v001-candidate-a-normalized.png

status: fail
analysis_engine: numpy
size: 1448x1086
component_count: 12
closest_gap_px: 10
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ff00
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ff00
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 border gap 56px is below required 64px
- component_2 border gap 57px is below required 64px
- component_3 border gap 29px is below required 64px
- component_5 contains 498px of exact key-color-like art > allowed 0px
- component_7 contains 2800px of exact key-color-like art > allowed 0px
- component_7 key/halo hue conflict ratio 0.065 > allowed 0.050; choose a safer background or split/preserve this art
- component_8 border gap 52px is below required 64px
- component_8 contains 6456px of exact key-color-like art > allowed 0px
- component_8 key/halo hue conflict ratio 0.194 > allowed 0.050; choose a safer background or split/preserve this art
- component_9 border gap 50px is below required 64px
- component_10 border gap 46px is below required 64px
- component_11 border gap 33px is below required 64px
- component_12 border gap 32px is below required 64px
- closest component gap 10px is below required 48px

## Problem Summary
- components_with_border_gap: 8
- components_with_exact_key_conflict: 3
- components_with_key_hue_conflict: 2
- total_exact_key_conflict_px: 9754
- total_key_fringe_hue_px: 1
- total_purple_halo_hue_px: 124
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_8', 'ratio': 0.19447, 'bbox': [52, 452, 247, 192]}

## Blocking Reasons
- code: key_color_conflict, count: 5, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 8, minimum_px: 64, action: regenerate_source_sheet_with_more_gutter_and_safe_border
- code: unsafe_gutter, closest_gap_px: 10, minimum_px: 48, closest_pair: ['component_6', 'component_11'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ff00

## Components
- component_1: bbox=[410, 56, 264, 295] area=26279 border_gap=56 visible=26279 exact_key=0 key_fringe_hue=1 purple_halo_hue=72 key_hue_ratio=0.002778
- component_2: bbox=[68, 57, 270, 294] area=26113 border_gap=57 visible=26113 exact_key=0 key_fringe_hue=0 purple_halo_hue=5 key_hue_ratio=0.000191
- component_3: bbox=[1112, 60, 307, 289] area=64965 border_gap=29 visible=64965 exact_key=0 key_fringe_hue=0 purple_halo_hue=9 key_hue_ratio=0.000139
- component_4: bbox=[751, 87, 274, 234] area=44829 border_gap=87 visible=44829 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_5: bbox=[1145, 384, 188, 334] area=39418 border_gap=115 visible=39418 exact_key=498 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.012634
- component_6: bbox=[772, 386, 214, 324] area=42467 border_gap=376 visible=42467 exact_key=0 key_fringe_hue=0 purple_halo_hue=5 key_hue_ratio=0.000118
- component_7: bbox=[404, 444, 264, 219] area=43053 border_gap=404 visible=43053 exact_key=2800 key_fringe_hue=0 purple_halo_hue=1 key_hue_ratio=0.065059
- component_8: bbox=[52, 452, 247, 192] area=33198 border_gap=52 visible=33198 exact_key=6456 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.19447
- component_9: bbox=[381, 700, 297, 336] area=43413 border_gap=50 visible=43413 exact_key=0 key_fringe_hue=0 purple_halo_hue=6 key_hue_ratio=0.000138
- component_10: bbox=[46, 717, 264, 310] area=26591 border_gap=46 visible=26591 exact_key=0 key_fringe_hue=0 purple_halo_hue=2 key_hue_ratio=7.5e-05
- component_11: bbox=[744, 720, 275, 333] area=30797 border_gap=33 visible=30797 exact_key=0 key_fringe_hue=0 purple_halo_hue=21 key_hue_ratio=0.000682
- component_12: bbox=[1107, 736, 284, 318] area=51670 border_gap=32 visible=51670 exact_key=0 key_fringe_hue=0 purple_halo_hue=3 key_hue_ratio=5.8e-05

## Candidate Key Colors
- #00ff00: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #00ffff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=383 hue_ratio=0.00081 score=383.081008
- #ffff00: exact=1 exact_ratio=2e-06 hue_band=5189 hue_ratio=0.010975 score=6190.09752
- #ff0000: exact=1 exact_ratio=2e-06 hue_band=33949 hue_ratio=0.071805 score=34956.180521
- #0000ff: exact=1214 exact_ratio=0.002568 hue_band=15994 hue_ratio=0.033829 score=1229997.382876
