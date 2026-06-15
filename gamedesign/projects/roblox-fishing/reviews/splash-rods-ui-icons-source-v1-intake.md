# Source Sheet Intake Audit: splash-rods-ui-icons-source-v1.png

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
- component_1 key/halo hue conflict ratio 0.081 > allowed 0.050; choose a safer background or split/preserve this art

## Problem Summary
- components_with_border_gap: 1
- components_with_exact_key_conflict: 0
- components_with_key_hue_conflict: 1
- total_exact_key_conflict_px: 0
- total_key_fringe_hue_px: 66731
- total_purple_halo_hue_px: 60843
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.081127, 'bbox': [0, 0, 1254, 1254]}

## Blocking Reasons
- code: key_color_conflict, count: 1, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 1, minimum_px: 24, action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[0, 0, 1254, 1254] area=1572516 border_gap=0 visible=1572516 exact_key=0 key_fringe_hue=66731 purple_halo_hue=60843 key_hue_ratio=0.081127

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=4801 hue_ratio=0.003053 score=4801.305307
- #ff0000: exact=0 exact_ratio=0.0 hue_band=16260 hue_ratio=0.01034 score=16261.034012
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=24876 hue_ratio=0.015819 score=24877.581923
- #ffff00: exact=10 exact_ratio=6e-06 hue_band=70847 hue_ratio=0.045053 score=80851.505328
- #00ffff: exact=26 exact_ratio=1.7e-05 hue_band=277612 hue_ratio=0.17654 score=303629.654002
- #00ff00: exact=643978 exact_ratio=0.409521 hue_band=668791 hue_ratio=0.4253 score=644646833.529997
