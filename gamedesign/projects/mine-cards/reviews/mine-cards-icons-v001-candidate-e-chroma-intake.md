# Source Sheet Intake Audit: mine-cards-icons-v001-candidate-e-chroma.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 15
closest_gap_px: 50
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ffff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ffff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 4694px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 0.141 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 contains 31px of exact key-color-like art > allowed 0px
- component_4 contains 2px of exact key-color-like art > allowed 0px
- component_6 contains 11px of exact key-color-like art > allowed 0px
- component_7 contains 2px of exact key-color-like art > allowed 0px
- component_14 contains 1735px of exact key-color-like art > allowed 0px
- component_14 key/halo hue conflict ratio 0.071 > allowed 0.050; choose a safer background or split/preserve this art

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 6
- components_with_key_hue_conflict: 2
- total_exact_key_conflict_px: 6475
- total_key_fringe_hue_px: 19
- total_purple_halo_hue_px: 369
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.141148, 'bbox': [642, 76, 224, 279]}

## Blocking Reasons
- code: key_color_conflict, count: 8, action: regenerate_source_sheet_with_safer_key_color

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ffff

## Components
- component_1: bbox=[642, 76, 224, 279] area=33610 border_gap=76 visible=33610 exact_key=4694 key_fringe_hue=1 purple_halo_hue=49 key_hue_ratio=0.141148
- component_2: bbox=[367, 85, 223, 247] area=32177 border_gap=85 visible=32177 exact_key=0 key_fringe_hue=1 purple_halo_hue=43 key_hue_ratio=0.001367
- component_3: bbox=[949, 91, 234, 255] area=41826 border_gap=71 visible=41826 exact_key=31 key_fringe_hue=0 purple_halo_hue=16 key_hue_ratio=0.001124
- component_4: bbox=[66, 107, 232, 233] area=35643 border_gap=66 visible=35643 exact_key=2 key_fringe_hue=1 purple_halo_hue=2 key_hue_ratio=0.00014
- component_5: bbox=[646, 405, 234, 244] area=42441 border_gap=374 visible=42441 exact_key=0 key_fringe_hue=0 purple_halo_hue=5 key_hue_ratio=0.000118
- component_6: bbox=[946, 413, 233, 236] area=43080 border_gap=75 visible=43080 exact_key=11 key_fringe_hue=1 purple_halo_hue=43 key_hue_ratio=0.001277
- component_7: bbox=[336, 414, 259, 234] area=36043 border_gap=336 visible=36043 exact_key=2 key_fringe_hue=8 purple_halo_hue=99 key_hue_ratio=0.003024
- component_8: bbox=[82, 420, 204, 206] area=29004 border_gap=82 visible=29004 exact_key=0 key_fringe_hue=2 purple_halo_hue=21 key_hue_ratio=0.000793
- component_9: bbox=[370, 723, 181, 192] area=25304 border_gap=339 visible=25304 exact_key=0 key_fringe_hue=0 purple_halo_hue=35 key_hue_ratio=0.001383
- component_10: bbox=[955, 724, 197, 193] area=16898 border_gap=102 visible=16898 exact_key=0 key_fringe_hue=1 purple_halo_hue=13 key_hue_ratio=0.000829
- component_11: bbox=[87, 725, 186, 193] area=25612 border_gap=87 visible=25612 exact_key=0 key_fringe_hue=2 purple_halo_hue=1 key_hue_ratio=0.000117
- component_12: bbox=[660, 727, 218, 185] area=26407 border_gap=342 visible=26407 exact_key=0 key_fringe_hue=1 purple_halo_hue=16 key_hue_ratio=0.000644
- component_13: bbox=[666, 975, 192, 210] area=31097 border_gap=69 visible=31097 exact_key=0 key_fringe_hue=1 purple_halo_hue=3 key_hue_ratio=0.000129
- component_14: bbox=[100, 978, 150, 208] area=24456 border_gap=68 visible=24456 exact_key=1735 key_fringe_hue=0 purple_halo_hue=2 key_hue_ratio=0.071026
- component_15: bbox=[377, 983, 183, 202] area=28339 border_gap=69 visible=28339 exact_key=0 key_fringe_hue=0 purple_halo_hue=21 key_hue_ratio=0.000741

## Candidate Key Colors
- #00ffff: exact=0 exact_ratio=0.0 hue_band=219 hue_ratio=0.000464 score=219.046404
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=567 hue_ratio=0.001201 score=567.120143
- #ff0000: exact=0 exact_ratio=0.0 hue_band=8660 hue_ratio=0.01835 score=8661.834991
- #00ff00: exact=0 exact_ratio=0.0 hue_band=15130 hue_ratio=0.032059 score=15133.205936
- #ffff00: exact=157 exact_ratio=0.000333 hue_band=37173 hue_ratio=0.078767 score=194180.876687
- #0000ff: exact=6398 exact_ratio=0.013557 hue_band=13958 hue_ratio=0.029576 score=6411960.957598
