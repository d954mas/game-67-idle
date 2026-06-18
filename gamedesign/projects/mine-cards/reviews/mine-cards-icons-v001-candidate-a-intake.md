# Source Sheet Intake Audit: mine-cards-icons-v001-candidate-a.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 1
closest_gap_px: None
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #ff00ff
key_color_action: keep_current_key_color
next_prompt_key_color: #ff00ff
recommended_next_step: regenerate_source_sheet_with_more_gutter_and_safe_border

## Problems
- component_1 border gap 0px is below required 24px

## Problem Summary
- components_with_border_gap: 1
- components_with_exact_key_conflict: 0
- components_with_key_hue_conflict: 0
- total_exact_key_conflict_px: 0
- total_key_fringe_hue_px: 0
- total_purple_halo_hue_px: 4
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_1', 'ratio': 3e-06, 'bbox': [0, 0, 1254, 1254]}

## Blocking Reasons
- code: unsafe_border, count: 1, minimum_px: 24, action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_more_gutter_and_safe_border
- reason: components are too close to sheet borders or each other for reliable slicing
- key_color: #ff00ff

## Components
- component_1: bbox=[0, 0, 1254, 1254] area=1572516 border_gap=0 visible=1572516 exact_key=0 key_fringe_hue=0 purple_halo_hue=4 key_hue_ratio=3e-06

## Candidate Key Colors
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=403 hue_ratio=0.000256 score=403.025628
- #0000ff: exact=0 exact_ratio=0.0 hue_band=749 hue_ratio=0.000476 score=749.047631
- #00ffff: exact=0 exact_ratio=0.0 hue_band=4547 hue_ratio=0.002892 score=4547.289154
- #ff0000: exact=0 exact_ratio=0.0 hue_band=28760 hue_ratio=0.018289 score=28761.828916
- #ffff00: exact=6 exact_ratio=4e-06 hue_band=35956 hue_ratio=0.022865 score=41958.286527
- #00ff00: exact=1090327 exact_ratio=0.693365 hue_band=1133308 hue_ratio=0.720697 score=1091460380.069728
