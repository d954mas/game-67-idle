# Source Sheet Intake Audit: mine-cards-fx-v002-candidate-b-clean.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 8
closest_gap_px: 0
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ff00
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ff00
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 3524px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 0.931 > allowed 0.050; choose a safer background or split/preserve this art
- component_2 contains 2px of exact key-color-like art > allowed 0px
- component_2 key/halo hue conflict ratio 0.343 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 key/halo hue conflict ratio 0.283 > allowed 0.050; choose a safer background or split/preserve this art
- component_4 key/halo hue conflict ratio 0.693 > allowed 0.050; choose a safer background or split/preserve this art
- component_5 key/halo hue conflict ratio 0.951 > allowed 0.050; choose a safer background or split/preserve this art
- component_6 key/halo hue conflict ratio 1.250 > allowed 0.050; choose a safer background or split/preserve this art
- component_7 key/halo hue conflict ratio 0.366 > allowed 0.050; choose a safer background or split/preserve this art
- component_8 key/halo hue conflict ratio 0.364 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 0px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 2
- components_with_key_hue_conflict: 8
- total_exact_key_conflict_px: 3526
- total_key_fringe_hue_px: 39604
- total_purple_halo_hue_px: 40706
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_6', 'ratio': 1.250302, 'bbox': [1068, 566, 61, 63]}

## Blocking Reasons
- code: key_color_conflict, count: 10, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_gutter, closest_gap_px: 0, minimum_px: 24, closest_pair: ['component_4', 'component_6'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ff00

## Components
- component_1: bbox=[497, 486, 298, 295] area=55437 border_gap=459 visible=55437 exact_key=3524 key_fringe_hue=24136 purple_halo_hue=23969 key_hue_ratio=0.931309
- component_2: bbox=[282, 522, 182, 218] area=20427 border_gap=282 visible=20427 exact_key=2 key_fringe_hue=3293 purple_halo_hue=3713 key_hue_ratio=0.343075
- component_3: bbox=[44, 524, 182, 224] area=19978 border_gap=44 visible=19978 exact_key=0 key_fringe_hue=2507 purple_halo_hue=3155 key_hue_ratio=0.283412
- component_4: bbox=[1113, 543, 76, 185] area=4104 border_gap=65 visible=4104 exact_key=0 key_fringe_hue=1419 purple_halo_hue=1427 key_hue_ratio=0.69347
- component_5: bbox=[860, 560, 143, 189] area=14137 border_gap=251 visible=14137 exact_key=0 key_fringe_hue=6635 purple_halo_hue=6814 key_hue_ratio=0.951333
- component_6: bbox=[1068, 566, 61, 63] area=1654 border_gap=125 visible=1654 exact_key=0 key_fringe_hue=1054 purple_halo_hue=1014 key_hue_ratio=1.250302
- component_7: bbox=[1061, 631, 43, 83] area=1569 border_gap=150 visible=1569 exact_key=0 key_fringe_hue=272 purple_halo_hue=303 key_hue_ratio=0.366475
- component_8: bbox=[1097, 679, 41, 50] area=1645 border_gap=116 visible=1645 exact_key=0 key_fringe_hue=288 purple_halo_hue=311 key_hue_ratio=0.364134

## Candidate Key Colors
- #00ff00: exact=0 exact_ratio=0.0 hue_band=932 hue_ratio=0.007835 score=932.783516
- #0000ff: exact=0 exact_ratio=0.0 hue_band=1469 hue_ratio=0.01235 score=1470.234962
- #ff0000: exact=0 exact_ratio=0.0 hue_band=6031 hue_ratio=0.050702 score=6036.070155
- #ffff00: exact=36 exact_ratio=0.000303 hue_band=12857 hue_ratio=0.108087 score=48867.808652
- #00ffff: exact=160 exact_ratio=0.001345 hue_band=10952 hue_ratio=0.092072 score=170961.207153
- #ff00ff: exact=3518 exact_ratio=0.029575 hue_band=29469 hue_ratio=0.247741 score=3547493.774067
