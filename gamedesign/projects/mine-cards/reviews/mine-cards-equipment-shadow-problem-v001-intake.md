# Source Sheet Intake Audit: mine-cards-equipment-source-sheet-shadow-problem-v001.png

status: fail
analysis_engine: numpy
size: 1448x1086
component_count: 12
closest_gap_px: 15
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 border gap 22px is below required 24px
- component_1 contains 247px of exact key-color-like art > allowed 0px
- component_2 border gap 23px is below required 24px
- component_2 contains 437px of exact key-color-like art > allowed 0px
- component_3 contains 191px of exact key-color-like art > allowed 0px
- component_4 contains 528px of exact key-color-like art > allowed 0px
- component_5 contains 610px of exact key-color-like art > allowed 0px
- component_6 contains 146px of exact key-color-like art > allowed 0px
- component_7 contains 1px of exact key-color-like art > allowed 0px
- component_8 contains 643px of exact key-color-like art > allowed 0px
- component_9 border gap 22px is below required 24px
- component_9 contains 725px of exact key-color-like art > allowed 0px
- component_10 contains 24px of exact key-color-like art > allowed 0px
- component_11 contains 294px of exact key-color-like art > allowed 0px
- component_12 contains 168px of exact key-color-like art > allowed 0px
- closest component gap 15px is below required 24px

## Problem Summary
- components_with_border_gap: 3
- components_with_exact_key_conflict: 12
- components_with_key_hue_conflict: 0
- total_exact_key_conflict_px: 4014
- total_key_fringe_hue_px: 0
- total_purple_halo_hue_px: 0
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_9', 'ratio': 0.02017, 'bbox': [1143, 742, 181, 322]}

## Blocking Reasons
- code: key_color_conflict, count: 12, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 3, minimum_px: 24, action: regenerate_source_sheet_with_more_gutter_and_safe_border
- code: unsafe_gutter, closest_gap_px: 15, minimum_px: 24, closest_pair: ['component_7', 'component_10'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[740, 22, 273, 354] area=23253 border_gap=22 visible=23253 exact_key=247 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.010622
- component_2: bbox=[1076, 23, 300, 352] area=31948 border_gap=23 visible=31948 exact_key=437 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.013678
- component_3: bbox=[403, 27, 294, 347] area=43712 border_gap=27 visible=43712 exact_key=191 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.00437
- component_4: bbox=[72, 32, 284, 337] area=30615 border_gap=32 visible=30615 exact_key=528 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.017246
- component_5: bbox=[73, 399, 237, 314] area=55363 border_gap=73 visible=55363 exact_key=610 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.011018
- component_6: bbox=[400, 399, 273, 313] area=46488 border_gap=374 visible=46488 exact_key=146 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.003141
- component_7: bbox=[737, 409, 268, 321] area=61273 border_gap=356 visible=61273 exact_key=1 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=1.6e-05
- component_8: bbox=[1071, 412, 324, 307] area=73034 border_gap=53 visible=73034 exact_key=643 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.008804
- component_9: bbox=[1143, 742, 181, 322] area=35944 border_gap=22 visible=35944 exact_key=725 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.02017
- component_10: bbox=[766, 745, 199, 304] area=36460 border_gap=37 visible=36460 exact_key=24 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.000658
- component_11: bbox=[398, 799, 261, 215] area=40112 border_gap=72 visible=40112 exact_key=294 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.007329
- component_12: bbox=[64, 803, 248, 191] area=32195 border_gap=64 visible=32195 exact_key=168 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.005218

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #00ff00: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #00ffff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #ffff00: exact=1 exact_ratio=2e-06 hue_band=289 hue_ratio=0.000566 score=1289.056623
- #ff0000: exact=0 exact_ratio=0.0 hue_band=14917 hue_ratio=0.029226 score=14919.922627
