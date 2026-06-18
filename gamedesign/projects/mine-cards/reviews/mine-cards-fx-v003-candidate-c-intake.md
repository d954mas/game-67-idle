# Source Sheet Intake Audit: mine-cards-fx-v003-candidate-c-clean.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 5
closest_gap_px: 18
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: keep_current_key_color
next_prompt_key_color: #00ff00
recommended_next_step: regenerate_source_sheet_with_more_gutter_and_safe_border

## Problems
- closest component gap 18px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 0
- components_with_key_hue_conflict: 0
- total_exact_key_conflict_px: 0
- total_key_fringe_hue_px: 0
- total_purple_halo_hue_px: 0
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.0, 'bbox': [527, 460, 279, 323]}

## Blocking Reasons
- code: unsafe_gutter, closest_gap_px: 18, minimum_px: 24, closest_pair: ['component_3', 'component_5'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_more_gutter_and_safe_border
- reason: components are too close to sheet borders or each other for reliable slicing
- key_color: #00ff00

## Components
- component_1: bbox=[527, 460, 279, 323] area=59198 border_gap=448 visible=59198 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_2: bbox=[293, 504, 210, 240] area=30872 border_gap=293 visible=30872 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_3: bbox=[1025, 514, 199, 225] area=23449 border_gap=30 visible=23449 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_4: bbox=[27, 521, 222, 223] area=32140 border_gap=27 visible=32140 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_5: bbox=[830, 525, 177, 213] area=23834 border_gap=247 visible=23834 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=4 hue_ratio=2.4e-05 score=4.00236
- #ff0000: exact=0 exact_ratio=0.0 hue_band=2441 hue_ratio=0.014402 score=2442.440177
- #00ff00: exact=0 exact_ratio=0.0 hue_band=23923 hue_ratio=0.141144 score=23937.114447
- #ffff00: exact=158 exact_ratio=0.000932 hue_band=34757 hue_ratio=0.205065 score=192777.506452
- #00ffff: exact=216 exact_ratio=0.001274 hue_band=23160 hue_ratio=0.136643 score=239173.664281
