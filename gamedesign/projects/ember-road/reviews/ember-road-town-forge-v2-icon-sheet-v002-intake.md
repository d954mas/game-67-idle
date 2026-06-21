# Source Sheet Intake Audit: ember-road-town-forge-v2-icons-v002-normalized-blue.png

status: fail
analysis_engine: numpy
size: 1536x1024
component_count: 11
closest_gap_px: 31
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ff00
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ff00
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 963px of exact key-color-like art > allowed 0px
- component_3 contains 16px of exact key-color-like art > allowed 0px
- component_10 contains 2189px of exact key-color-like art > allowed 0px
- component_10 key/halo hue conflict ratio 0.122 > allowed 0.050; choose a safer background or split/preserve this art

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 3
- components_with_key_hue_conflict: 1
- total_exact_key_conflict_px: 3168
- total_key_fringe_hue_px: 12
- total_purple_halo_hue_px: 160
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_10', 'ratio': 0.121672, 'bbox': [1203, 631, 123, 196]}

## Blocking Reasons
- code: key_color_conflict, count: 4, action: regenerate_source_sheet_with_safer_key_color

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ff00

## Components
- component_1: bbox=[96, 167, 177, 308] area=36330 border_gap=96 visible=36330 exact_key=963 key_fringe_hue=0 purple_halo_hue=46 key_hue_ratio=0.027773
- component_2: bbox=[374, 189, 196, 289] area=35450 border_gap=189 visible=35450 exact_key=0 key_fringe_hue=4 purple_halo_hue=65 key_hue_ratio=0.001946
- component_3: bbox=[635, 242, 246, 233] area=32302 border_gap=242 visible=32302 exact_key=16 key_fringe_hue=0 purple_halo_hue=1 key_hue_ratio=0.000526
- component_4: bbox=[933, 243, 254, 235] area=46759 border_gap=243 visible=46759 exact_key=0 key_fringe_hue=0 purple_halo_hue=3 key_hue_ratio=6.4e-05
- component_5: bbox=[1235, 247, 241, 231] area=45616 border_gap=60 visible=45616 exact_key=0 key_fringe_hue=0 purple_halo_hue=4 key_hue_ratio=8.8e-05
- component_6: bbox=[74, 583, 243, 251] area=44367 border_gap=74 visible=44367 exact_key=0 key_fringe_hue=0 purple_halo_hue=1 key_hue_ratio=2.3e-05
- component_7: bbox=[377, 599, 194, 222] area=24213 border_gap=203 visible=24213 exact_key=0 key_fringe_hue=8 purple_halo_hue=35 key_hue_ratio=0.001776
- component_8: bbox=[649, 609, 210, 215] area=35530 border_gap=200 visible=35530 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_9: bbox=[918, 615, 238, 211] area=38115 border_gap=198 visible=38115 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_10: bbox=[1203, 631, 123, 196] area=17991 border_gap=197 visible=17991 exact_key=2189 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.121672
- component_11: bbox=[1357, 636, 126, 191] area=16023 border_gap=53 visible=16023 exact_key=0 key_fringe_hue=0 purple_halo_hue=5 key_hue_ratio=0.000312

## Candidate Key Colors
- #00ff00: exact=0 exact_ratio=0.0 hue_band=15 hue_ratio=4e-05 score=15.004025
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=429 hue_ratio=0.001151 score=429.115107
- #00ffff: exact=0 exact_ratio=0.0 hue_band=2447 hue_ratio=0.006566 score=2447.656567
- #ffff00: exact=3 exact_ratio=8e-06 hue_band=28028 hue_ratio=0.075203 score=31035.520338
- #ff0000: exact=0 exact_ratio=0.0 hue_band=52198 hue_ratio=0.140055 score=52212.005517
- #0000ff: exact=684 exact_ratio=0.001835 hue_band=10566 hue_ratio=0.02835 score=694568.835018
