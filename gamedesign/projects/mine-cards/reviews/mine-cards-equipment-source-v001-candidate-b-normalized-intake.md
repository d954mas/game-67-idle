# Source Sheet Intake Audit: mine-cards-equipment-source-v001-candidate-b-normalized.png

status: fail
analysis_engine: numpy
size: 1448x1086
component_count: 12
closest_gap_px: 12
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ffff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ffff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 border gap 59px is below required 64px
- component_2 border gap 60px is below required 64px
- component_3 border gap 39px is below required 64px
- component_5 contains 444px of exact key-color-like art > allowed 0px
- component_6 contains 22px of exact key-color-like art > allowed 0px
- component_7 contains 2509px of exact key-color-like art > allowed 0px
- component_7 key/halo hue conflict ratio 0.060 > allowed 0.050; choose a safer background or split/preserve this art
- component_8 contains 6034px of exact key-color-like art > allowed 0px
- component_8 key/halo hue conflict ratio 0.166 > allowed 0.050; choose a safer background or split/preserve this art
- component_10 border gap 57px is below required 64px
- component_11 border gap 52px is below required 64px
- component_12 border gap 51px is below required 64px
- closest component gap 12px is below required 48px

## Problem Summary
- components_with_border_gap: 6
- components_with_exact_key_conflict: 4
- components_with_key_hue_conflict: 2
- total_exact_key_conflict_px: 9009
- total_key_fringe_hue_px: 2
- total_purple_halo_hue_px: 3
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_8', 'ratio': 0.166428, 'bbox': [66, 450, 235, 186]}

## Blocking Reasons
- code: key_color_conflict, count: 6, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 6, minimum_px: 64, action: regenerate_source_sheet_with_more_gutter_and_safe_border
- code: unsafe_gutter, closest_gap_px: 12, minimum_px: 48, closest_pair: ['component_6', 'component_11'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ffff

## Components
- component_1: bbox=[412, 59, 257, 288] area=24795 border_gap=59 visible=24795 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_2: bbox=[76, 60, 264, 286] area=24652 border_gap=60 visible=24652 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_3: bbox=[1111, 61, 298, 282] area=61818 border_gap=39 visible=61818 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_4: bbox=[750, 88, 267, 227] area=42670 border_gap=88 visible=42670 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_5: bbox=[1143, 386, 182, 323] area=37290 border_gap=123 visible=37290 exact_key=444 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.011907
- component_6: bbox=[780, 389, 205, 310] area=39079 border_gap=387 visible=39079 exact_key=22 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.000563
- component_7: bbox=[411, 442, 253, 210] area=41718 border_gap=411 visible=41718 exact_key=2509 key_fringe_hue=2 purple_halo_hue=3 key_hue_ratio=0.060262
- component_8: bbox=[66, 450, 235, 186] area=36256 border_gap=66 visible=36256 exact_key=6034 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.166428
- component_9: bbox=[386, 689, 290, 327] area=41455 border_gap=70 visible=41455 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_10: bbox=[57, 707, 257, 301] area=25340 border_gap=57 visible=25340 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_11: bbox=[750, 711, 267, 323] area=29051 border_gap=52 visible=29051 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_12: bbox=[1105, 726, 278, 309] area=49573 border_gap=51 visible=49573 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0

## Candidate Key Colors
- #00ffff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #0000ff: exact=0 exact_ratio=0.0 hue_band=4 hue_ratio=9e-06 score=4.000882
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=45 hue_ratio=9.9e-05 score=45.009919
- #ffff00: exact=19 exact_ratio=4.2e-05 hue_band=3217 hue_ratio=0.007091 score=22217.709064
- #ff0000: exact=12 exact_ratio=2.6e-05 hue_band=46106 hue_ratio=0.101623 score=58116.162289
- #00ff00: exact=8930 exact_ratio=0.019683 hue_band=20235 hue_ratio=0.0446 score=8950239.460025
