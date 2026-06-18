# Source Sheet Intake Audit: mine-cards-fx-v001-candidate-a-clean.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 5
closest_gap_px: 17
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ff00
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ff00
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 47729px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 1.602 > allowed 0.050; choose a safer background or split/preserve this art
- component_2 contains 44956px of exact key-color-like art > allowed 0px
- component_2 key/halo hue conflict ratio 1.389 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 contains 46737px of exact key-color-like art > allowed 0px
- component_3 key/halo hue conflict ratio 0.850 > allowed 0.050; choose a safer background or split/preserve this art
- component_4 contains 42069px of exact key-color-like art > allowed 0px
- component_4 key/halo hue conflict ratio 1.591 > allowed 0.050; choose a safer background or split/preserve this art
- component_5 contains 44436px of exact key-color-like art > allowed 0px
- component_5 key/halo hue conflict ratio 1.734 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 17px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 5
- components_with_key_hue_conflict: 5
- total_exact_key_conflict_px: 225927
- total_key_fringe_hue_px: 22
- total_purple_halo_hue_px: 4115
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_5', 'ratio': 1.734431, 'bbox': [1023, 459, 194, 332]}

## Blocking Reasons
- code: key_color_conflict, count: 10, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_gutter, closest_gap_px: 17, minimum_px: 24, closest_pair: ['component_2', 'component_3'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ff00

## Components
- component_1: bbox=[27, 459, 220, 332] area=29789 border_gap=27 visible=29789 exact_key=47729 key_fringe_hue=1 purple_halo_hue=3 key_hue_ratio=1.60237
- component_2: bbox=[267, 459, 218, 332] area=33081 border_gap=267 visible=33081 exact_key=44956 key_fringe_hue=4 purple_halo_hue=973 key_hue_ratio=1.388501
- component_3: bbox=[502, 459, 289, 332] area=57410 border_gap=459 visible=57410 exact_key=46737 key_fringe_hue=11 purple_halo_hue=2073 key_hue_ratio=0.850392
- component_4: bbox=[810, 459, 193, 332] area=26446 border_gap=251 visible=26446 exact_key=42069 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=1.590751
- component_5: bbox=[1023, 459, 194, 332] area=26238 border_gap=37 visible=26238 exact_key=44436 key_fringe_hue=6 purple_halo_hue=1066 key_hue_ratio=1.734431

## Candidate Key Colors
- #00ff00: exact=0 exact_ratio=0.0 hue_band=541 hue_ratio=0.003128 score=541.312782
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=1039 hue_ratio=0.006007 score=1039.600703
- #ff0000: exact=0 exact_ratio=0.0 hue_band=2806 hue_ratio=0.016223 score=2807.622303
- #ffff00: exact=13 exact_ratio=7.5e-05 hue_band=7492 hue_ratio=0.043315 score=20496.331537
- #00ffff: exact=80 exact_ratio=0.000463 hue_band=9270 hue_ratio=0.053595 score=89275.359497
- #0000ff: exact=30358 exact_ratio=0.175516 hue_band=95688 hue_ratio=0.553225 score=30453743.322495
