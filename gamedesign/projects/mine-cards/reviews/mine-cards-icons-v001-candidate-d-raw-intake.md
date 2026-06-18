# Source Sheet Intake Audit: mine-cards-icons-v001-candidate-d.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 1
closest_gap_px: None
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 border gap 0px is below required 24px
- component_1 contains 895913px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 1.779 > allowed 0.050; choose a safer background or split/preserve this art

## Problem Summary
- components_with_border_gap: 1
- components_with_exact_key_conflict: 1
- components_with_key_hue_conflict: 1
- total_exact_key_conflict_px: 895913
- total_key_fringe_hue_px: 499808
- total_purple_halo_hue_px: 504855
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_1', 'ratio': 1.778638, 'bbox': [0, 0, 1254, 1254]}

## Blocking Reasons
- code: key_color_conflict, count: 2, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 1, minimum_px: 24, action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[0, 0, 1254, 1254] area=1068557 border_gap=0 visible=1068557 exact_key=895913 key_fringe_hue=499808 purple_halo_hue=504855 key_hue_ratio=1.778638

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=228 hue_ratio=0.000213 score=228.021337
- #ff0000: exact=0 exact_ratio=0.0 hue_band=10918 hue_ratio=0.010218 score=10919.021752
- #00ffff: exact=0 exact_ratio=0.0 hue_band=17775 hue_ratio=0.016635 score=17776.663458
- #00ff00: exact=0 exact_ratio=0.0 hue_band=22381 hue_ratio=0.020945 score=22383.094507
- #ffff00: exact=71 exact_ratio=6.6e-05 hue_band=45978 hue_ratio=0.043028 score=116982.302812
- #ff00ff: exact=485151 exact_ratio=0.454024 hue_band=504926 hue_ratio=0.472531 score=485655973.253071
