# Source Sheet Intake Audit: ember-road-town-forge-v2-chars-normalized-green.png

status: fail
analysis_engine: numpy
size: 1536x1024
component_count: 8
closest_gap_px: 5
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 border gap 11px is below required 24px
- component_2 border gap 15px is below required 24px
- component_2 contains 819px of exact key-color-like art > allowed 0px
- component_3 border gap 22px is below required 24px
- component_3 contains 24px of exact key-color-like art > allowed 0px
- component_4 contains 6px of exact key-color-like art > allowed 0px
- component_7 border gap 17px is below required 24px
- component_7 contains 258px of exact key-color-like art > allowed 0px
- closest component gap 5px is below required 24px

## Problem Summary
- components_with_border_gap: 4
- components_with_exact_key_conflict: 4
- components_with_key_hue_conflict: 0
- total_exact_key_conflict_px: 1107
- total_key_fringe_hue_px: 0
- total_purple_halo_hue_px: 0
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_2', 'ratio': 0.014331, 'bbox': [566, 15, 323, 392]}

## Blocking Reasons
- code: key_color_conflict, count: 4, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 4, minimum_px: 24, action: regenerate_source_sheet_with_more_gutter_and_safe_border
- code: unsafe_gutter, closest_gap_px: 5, minimum_px: 24, closest_pair: ['component_5', 'component_7'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[1076, 11, 286, 359] area=54890 border_gap=11 visible=54890 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_2: bbox=[566, 15, 323, 392] area=57150 border_gap=15 visible=57150 exact_key=819 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.014331
- component_3: bbox=[109, 22, 350, 377] area=49965 border_gap=22 visible=49965 exact_key=24 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.00048
- component_4: bbox=[1057, 390, 317, 371] area=54141 border_gap=162 visible=54141 exact_key=6 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.000111
- component_5: bbox=[175, 423, 250, 244] area=30101 border_gap=175 visible=30101 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_6: bbox=[645, 429, 217, 261] area=37240 border_gap=334 visible=37240 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_7: bbox=[195, 672, 184, 335] area=34606 border_gap=17 visible=34606 exact_key=258 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.007455
- component_8: bbox=[627, 912, 258, 54] area=10461 border_gap=58 visible=10461 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #00ffff: exact=0 exact_ratio=0.0 hue_band=2 hue_ratio=6e-06 score=2.000609
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=6 hue_ratio=1.8e-05 score=6.001826
- #ff0000: exact=0 exact_ratio=0.0 hue_band=14337 hue_ratio=0.043637 score=14341.363666
- #ffff00: exact=4 exact_ratio=1.2e-05 hue_band=63568 hue_ratio=0.193478 score=67587.347809
- #00ff00: exact=1075 exact_ratio=0.003272 hue_band=37713 hue_ratio=0.114785 score=1112724.478478
