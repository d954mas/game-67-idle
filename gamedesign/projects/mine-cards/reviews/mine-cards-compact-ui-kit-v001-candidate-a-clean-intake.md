# Source Sheet Intake Audit: mine-cards-compact-ui-kit-v001-candidate-a-clean.png

status: fail
analysis_engine: numpy
size: 1536x1024
component_count: 1
closest_gap_px: None
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ff00
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
- total_purple_halo_hue_px: 186
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.000118, 'bbox': [0, 0, 1536, 1024]}

## Blocking Reasons
- code: unsafe_border, count: 1, minimum_px: 24, action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_more_gutter_and_safe_border
- reason: components are too close to sheet borders or each other for reliable slicing
- key_color: #ff00ff

## Components
- component_1: bbox=[0, 0, 1536, 1024] area=1572864 border_gap=0 visible=1572864 exact_key=0 key_fringe_hue=0 purple_halo_hue=186 key_hue_ratio=0.000118

## Candidate Key Colors
- #00ff00: exact=0 exact_ratio=0.0 hue_band=1256 hue_ratio=0.000799 score=1256.079854
- #ff0000: exact=0 exact_ratio=0.0 hue_band=1406 hue_ratio=0.000894 score=1406.089391
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=2205 hue_ratio=0.001402 score=2205.14019
- #ffff00: exact=0 exact_ratio=0.0 hue_band=6221 hue_ratio=0.003955 score=6221.395521
- #0000ff: exact=0 exact_ratio=0.0 hue_band=43390 hue_ratio=0.027587 score=43392.758662
- #00ffff: exact=1213489 exact_ratio=0.771516 hue_band=1232883 hue_ratio=0.783846 score=1214721961.38459
