# Source Sheet Intake Audit: mine-cards-equipment-source-v001-candidate-b.png

status: fail
analysis_engine: numpy
size: 1448x1086
component_count: 1
closest_gap_px: None
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ffff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ffff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_count 1 is below required 12
- component_1 border gap 0px is below required 64px
- component_1 contains 1123946px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 0.716 > allowed 0.050; choose a safer background or split/preserve this art

## Problem Summary
- components_with_border_gap: 1
- components_with_exact_key_conflict: 1
- components_with_key_hue_conflict: 1
- total_exact_key_conflict_px: 1123946
- total_key_fringe_hue_px: 2
- total_purple_halo_hue_px: 3
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.71585, 'bbox': [0, 0, 1448, 1086]}

## Blocking Reasons
- code: key_color_conflict, count: 2, action: regenerate_source_sheet_with_safer_key_color
- code: too_few_components, count: 1, minimum: 12, action: regenerate_source_sheet_with_clearer_separation
- code: unsafe_border, count: 1, minimum_px: 64, action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ffff

## Components
- component_1: bbox=[0, 0, 1448, 1086] area=1570092 border_gap=0 visible=1570092 exact_key=1123946 key_fringe_hue=2 purple_halo_hue=3 key_hue_ratio=0.71585

## Candidate Key Colors
- #00ffff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #0000ff: exact=0 exact_ratio=0.0 hue_band=4 hue_ratio=3e-06 score=4.000255
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=45 hue_ratio=2.9e-05 score=45.002866
- #ffff00: exact=19 exact_ratio=1.2e-05 hue_band=3217 hue_ratio=0.002049 score=22217.204892
- #ff0000: exact=12 exact_ratio=8e-06 hue_band=46106 hue_ratio=0.029365 score=58108.936516
- #00ff00: exact=1121510 exact_ratio=0.714296 hue_band=1136630 hue_ratio=0.723926 score=1122646702.392573
