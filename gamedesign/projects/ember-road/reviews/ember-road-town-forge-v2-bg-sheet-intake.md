# Source Sheet Intake Audit: ember-road-town-forge-v2-bg-normalized-green.png

status: fail
analysis_engine: numpy
size: 1536x1024
component_count: 10
closest_gap_px: 26
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 border gap 21px is below required 24px
- component_1 contains 1202px of exact key-color-like art > allowed 0px
- component_2 contains 1300px of exact key-color-like art > allowed 0px
- component_3 contains 13px of exact key-color-like art > allowed 0px
- component_4 contains 73px of exact key-color-like art > allowed 0px
- component_5 contains 441px of exact key-color-like art > allowed 0px
- component_8 contains 166px of exact key-color-like art > allowed 0px

## Problem Summary
- components_with_border_gap: 1
- components_with_exact_key_conflict: 6
- components_with_key_hue_conflict: 0
- total_exact_key_conflict_px: 3195
- total_key_fringe_hue_px: 1
- total_purple_halo_hue_px: 0
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_2', 'ratio': 0.017244, 'bbox': [640, 42, 410, 342]}

## Blocking Reasons
- code: key_color_conflict, count: 6, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 1, minimum_px: 24, action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[63, 21, 488, 360] area=139066 border_gap=21 visible=139066 exact_key=1202 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.008643
- component_2: bbox=[640, 42, 410, 342] area=75448 border_gap=42 visible=75448 exact_key=1300 key_fringe_hue=1 purple_halo_hue=0 key_hue_ratio=0.017244
- component_3: bbox=[1146, 142, 289, 203] area=28368 border_gap=101 visible=28368 exact_key=13 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.000458
- component_4: bbox=[608, 410, 346, 296] area=44743 border_gap=318 visible=44743 exact_key=73 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.001632
- component_5: bbox=[41, 420, 523, 273] area=108680 border_gap=41 visible=108680 exact_key=441 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.004058
- component_6: bbox=[997, 420, 124, 274] area=14788 border_gap=330 visible=14788 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_7: bbox=[1170, 433, 318, 253] area=63708 border_gap=48 visible=63708 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_8: bbox=[68, 738, 407, 231] area=75826 border_gap=55 visible=75826 exact_key=166 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.002189
- component_9: bbox=[537, 758, 485, 215] area=79648 border_gap=51 visible=79648 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_10: bbox=[1078, 762, 357, 213] area=59696 border_gap=49 visible=59696 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=24 hue_ratio=3.5e-05 score=24.003478
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=44 hue_ratio=6.4e-05 score=44.006377
- #00ffff: exact=0 exact_ratio=0.0 hue_band=1048 hue_ratio=0.001519 score=1048.15189
- #ff0000: exact=1 exact_ratio=1e-06 hue_band=39140 hue_ratio=0.056727 score=40145.672702
- #ffff00: exact=4 exact_ratio=6e-06 hue_band=62856 hue_ratio=0.091099 score=66865.109948
- #00ff00: exact=1718 exact_ratio=0.00249 hue_band=21852 hue_ratio=0.031671 score=1739855.16709
