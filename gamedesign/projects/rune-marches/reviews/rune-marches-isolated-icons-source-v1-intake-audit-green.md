# Source Sheet Intake Audit: rune-marches-isolated-icons-source-v1.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 1
closest_gap_px: None
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #ff00ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #ff00ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 border gap 0px is below required 24px
- component_1 contains 1196995px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 0.993 > allowed 0.050; choose a safer background or split/preserve this art

## Problem Summary
- components_with_border_gap: 1
- components_with_exact_key_conflict: 1
- components_with_key_hue_conflict: 1
- total_exact_key_conflict_px: 1196995
- total_key_fringe_hue_px: 22
- total_purple_halo_hue_px: 0
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.993479, 'bbox': [0, 0, 1254, 1254]}

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #ff00ff

## Components
- component_1: bbox=[0, 0, 1254, 1254] area=1204874 border_gap=0 visible=1204874 exact_key=1196995 key_fringe_hue=22 purple_halo_hue=0 key_hue_ratio=0.993479

## Candidate Key Colors
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=3 hue_ratio=2e-06 score=3.000249
- #0000ff: exact=0 exact_ratio=0.0 hue_band=116 hue_ratio=9.6e-05 score=116.009628
- #ffff00: exact=0 exact_ratio=0.0 hue_band=18218 hue_ratio=0.01512 score=18219.512025
- #00ffff: exact=0 exact_ratio=0.0 hue_band=22005 hue_ratio=0.018263 score=22006.826332
- #ff0000: exact=1 exact_ratio=1e-06 hue_band=26825 hue_ratio=0.022264 score=27827.226374
- #00ff00: exact=868612 exact_ratio=0.720915 hue_band=878715 hue_ratio=0.7293 score=869490787.930032

## Timing
- load_image: 26.078 ms
- find_components: 190.019 ms
- merge_fragments: 318.321 ms
- key_conflicts: 110.758 ms
- candidate_key_scores: 124.653 ms
- component_rules: 0.019 ms
- gutter_scan: 0.004 ms
- total: 769.88 ms
