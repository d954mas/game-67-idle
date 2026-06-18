# Source Sheet Intake Audit: mine-cards-equipment-source-v001-candidate-a.png

status: fail
analysis_engine: numpy
size: 1448x1086
component_count: 12
closest_gap_px: 9
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ff00
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ff00
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 border gap 55px is below required 64px
- component_1 contains 1824px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 0.068 > allowed 0.050; choose a safer background or split/preserve this art
- component_2 border gap 56px is below required 64px
- component_2 contains 1334px of exact key-color-like art > allowed 0px
- component_3 border gap 28px is below required 64px
- component_3 contains 1509px of exact key-color-like art > allowed 0px
- component_4 contains 1099px of exact key-color-like art > allowed 0px
- component_5 contains 1818px of exact key-color-like art > allowed 0px
- component_6 contains 1903px of exact key-color-like art > allowed 0px
- component_7 contains 3964px of exact key-color-like art > allowed 0px
- component_7 key/halo hue conflict ratio 0.090 > allowed 0.050; choose a safer background or split/preserve this art
- component_8 border gap 48px is below required 64px
- component_8 contains 7367px of exact key-color-like art > allowed 0px
- component_8 key/halo hue conflict ratio 0.216 > allowed 0.050; choose a safer background or split/preserve this art
- component_9 border gap 49px is below required 64px
- component_9 contains 2200px of exact key-color-like art > allowed 0px
- component_10 border gap 43px is below required 64px
- component_10 contains 1511px of exact key-color-like art > allowed 0px
- component_10 key/halo hue conflict ratio 0.054 > allowed 0.050; choose a safer background or split/preserve this art
- component_11 border gap 32px is below required 64px
- component_11 contains 1715px of exact key-color-like art > allowed 0px
- component_11 key/halo hue conflict ratio 0.054 > allowed 0.050; choose a safer background or split/preserve this art
- component_12 border gap 31px is below required 64px
- component_12 contains 2832px of exact key-color-like art > allowed 0px
- component_12 key/halo hue conflict ratio 0.052 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 9px is below required 48px

## Problem Summary
- components_with_border_gap: 8
- components_with_exact_key_conflict: 12
- components_with_key_hue_conflict: 6
- total_exact_key_conflict_px: 29076
- total_key_fringe_hue_px: 1
- total_purple_halo_hue_px: 124
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_8', 'ratio': 0.216307, 'bbox': [48, 450, 255, 195]}

## Blocking Reasons
- code: key_color_conflict, count: 18, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 8, minimum_px: 64, action: regenerate_source_sheet_with_more_gutter_and_safe_border
- code: unsafe_gutter, closest_gap_px: 9, minimum_px: 48, closest_pair: ['component_6', 'component_11'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ff00

## Components
- component_1: bbox=[409, 55, 268, 298] area=28001 border_gap=55 visible=28001 exact_key=1824 key_fringe_hue=1 purple_halo_hue=72 key_hue_ratio=0.067748
- component_2: bbox=[65, 56, 275, 296] area=27383 border_gap=56 visible=27383 exact_key=1334 key_fringe_hue=0 purple_halo_hue=5 key_hue_ratio=0.048899
- component_3: bbox=[1110, 60, 310, 290] area=66389 border_gap=28 visible=66389 exact_key=1509 key_fringe_hue=0 purple_halo_hue=9 key_hue_ratio=0.022865
- component_4: bbox=[748, 86, 280, 238] area=45844 border_gap=86 visible=45844 exact_key=1099 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.023973
- component_5: bbox=[1141, 382, 195, 336] area=40661 border_gap=112 visible=40661 exact_key=1818 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.044711
- component_6: bbox=[771, 385, 218, 325] area=44259 border_gap=376 visible=44259 exact_key=1903 key_fringe_hue=0 purple_halo_hue=5 key_hue_ratio=0.04311
- component_7: bbox=[400, 442, 272, 223] area=44100 border_gap=400 visible=44100 exact_key=3964 key_fringe_hue=0 purple_halo_hue=1 key_hue_ratio=0.089909
- component_8: bbox=[48, 450, 255, 195] area=34058 border_gap=48 visible=34058 exact_key=7367 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.216307
- component_9: bbox=[378, 700, 305, 337] area=45453 border_gap=49 visible=45453 exact_key=2200 key_fringe_hue=0 purple_halo_hue=6 key_hue_ratio=0.048534
- component_10: bbox=[43, 714, 270, 315] area=28037 border_gap=43 visible=28037 exact_key=1511 key_fringe_hue=0 purple_halo_hue=2 key_hue_ratio=0.053964
- component_11: bbox=[742, 719, 279, 335] area=32404 border_gap=32 visible=32404 exact_key=1715 key_fringe_hue=0 purple_halo_hue=21 key_hue_ratio=0.053574
- component_12: bbox=[1104, 736, 289, 319] area=54319 border_gap=31 visible=54319 exact_key=2832 key_fringe_hue=0 purple_halo_hue=3 key_hue_ratio=0.052192

## Candidate Key Colors
- #00ff00: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #00ffff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=383 hue_ratio=0.00078 score=383.078019
- #ffff00: exact=1 exact_ratio=2e-06 hue_band=5189 hue_ratio=0.01057 score=6190.057021
- #ff0000: exact=1 exact_ratio=2e-06 hue_band=33949 hue_ratio=0.069156 score=34955.915552
- #0000ff: exact=18092 exact_ratio=0.036854 hue_band=34109 hue_ratio=0.069481 score=18126115.948145
