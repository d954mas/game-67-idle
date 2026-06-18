# Source Sheet Intake Audit: mine-cards-icons-v001-candidate-b-chroma.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 23
closest_gap_px: 13
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_2 key/halo hue conflict ratio 0.058 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 contains 1662px of exact key-color-like art > allowed 0px
- component_3 key/halo hue conflict ratio 0.346 > allowed 0.050; choose a safer background or split/preserve this art
- component_4 key/halo hue conflict ratio 0.051 > allowed 0.050; choose a safer background or split/preserve this art
- component_7 contains 141px of exact key-color-like art > allowed 0px
- component_7 key/halo hue conflict ratio 0.124 > allowed 0.050; choose a safer background or split/preserve this art
- component_8 contains 125px of exact key-color-like art > allowed 0px
- component_8 key/halo hue conflict ratio 0.057 > allowed 0.050; choose a safer background or split/preserve this art
- component_9 contains 210px of exact key-color-like art > allowed 0px
- component_9 key/halo hue conflict ratio 0.111 > allowed 0.050; choose a safer background or split/preserve this art
- component_13 contains 275px of exact key-color-like art > allowed 0px
- component_13 key/halo hue conflict ratio 0.054 > allowed 0.050; choose a safer background or split/preserve this art
- component_14 contains 1588px of exact key-color-like art > allowed 0px
- component_14 key/halo hue conflict ratio 0.200 > allowed 0.050; choose a safer background or split/preserve this art
- component_15 key/halo hue conflict ratio 0.115 > allowed 0.050; choose a safer background or split/preserve this art
- component_17 key/halo hue conflict ratio 0.073 > allowed 0.050; choose a safer background or split/preserve this art
- component_19 key/halo hue conflict ratio 0.115 > allowed 0.050; choose a safer background or split/preserve this art
- component_21 key/halo hue conflict ratio 0.173 > allowed 0.050; choose a safer background or split/preserve this art
- component_22 key/halo hue conflict ratio 0.113 > allowed 0.050; choose a safer background or split/preserve this art
- component_23 key/halo hue conflict ratio 0.147 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 13px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 6
- components_with_key_hue_conflict: 14
- total_exact_key_conflict_px: 4001
- total_key_fringe_hue_px: 8637
- total_purple_halo_hue_px: 13611
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_3', 'ratio': 0.346116, 'bbox': [665, 94, 211, 220]}

## Blocking Reasons
- code: key_color_conflict, count: 20, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_gutter, closest_gap_px: 13, minimum_px: 24, closest_pair: ['component_15', 'component_16'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[376, 73, 188, 246] area=31790 border_gap=73 visible=31790 exact_key=0 key_fringe_hue=195 purple_halo_hue=441 key_hue_ratio=0.020006
- component_2: bbox=[952, 93, 218, 222] area=23711 border_gap=84 visible=23711 exact_key=0 key_fringe_hue=474 purple_halo_hue=900 key_hue_ratio=0.057948
- component_3: bbox=[665, 94, 211, 220] area=15151 border_gap=94 visible=15151 exact_key=1662 key_fringe_hue=1693 purple_halo_hue=1889 key_hue_ratio=0.346116
- component_4: bbox=[71, 98, 189, 211] area=21066 border_gap=71 visible=21066 exact_key=0 key_fringe_hue=286 purple_halo_hue=794 key_hue_ratio=0.051267
- component_5: bbox=[646, 384, 221, 225] area=38755 border_gap=384 visible=38755 exact_key=0 key_fringe_hue=278 purple_halo_hue=596 key_hue_ratio=0.022552
- component_6: bbox=[955, 387, 220, 223] area=37793 border_gap=79 visible=37793 exact_key=0 key_fringe_hue=289 purple_halo_hue=498 key_hue_ratio=0.020824
- component_7: bbox=[73, 400, 193, 200] area=23973 border_gap=73 visible=23973 exact_key=141 key_fringe_hue=1001 purple_halo_hue=1819 key_hue_ratio=0.123514
- component_8: bbox=[349, 420, 224, 191] area=29642 border_gap=349 visible=29642 exact_key=125 key_fringe_hue=646 purple_halo_hue=921 key_hue_ratio=0.057081
- component_9: bbox=[956, 691, 199, 194] area=17065 border_gap=99 visible=17065 exact_key=210 key_fringe_hue=616 purple_halo_hue=1062 key_hue_ratio=0.110636
- component_10: bbox=[368, 694, 186, 180] area=22998 border_gap=368 visible=22998 exact_key=0 key_fringe_hue=231 purple_halo_hue=526 key_hue_ratio=0.032916
- component_11: bbox=[668, 710, 194, 164] area=20443 border_gap=380 visible=20443 exact_key=0 key_fringe_hue=181 purple_halo_hue=431 key_hue_ratio=0.029937
- component_12: bbox=[97, 716, 158, 152] area=17037 border_gap=97 visible=17037 exact_key=0 key_fringe_hue=205 purple_halo_hue=516 key_hue_ratio=0.04232
- component_13: bbox=[362, 959, 206, 213] area=32448 border_gap=82 visible=32448 exact_key=275 key_fringe_hue=524 purple_halo_hue=963 key_hue_ratio=0.054302
- component_14: bbox=[90, 967, 149, 202] area=25032 border_gap=85 visible=25032 exact_key=1588 key_fringe_hue=1531 purple_halo_hue=1896 key_hue_ratio=0.200344
- component_15: bbox=[956, 981, 53, 48] area=740 border_gap=225 visible=740 exact_key=0 key_fringe_hue=61 purple_halo_hue=24 key_hue_ratio=0.114865
- component_16: bbox=[1022, 981, 57, 10] area=570 border_gap=175 visible=570 exact_key=0 key_fringe_hue=10 purple_halo_hue=10 key_hue_ratio=0.035088
- component_17: bbox=[1092, 981, 59, 48] area=860 border_gap=103 visible=860 exact_key=0 key_fringe_hue=43 purple_halo_hue=20 key_hue_ratio=0.073256
- component_18: bbox=[666, 985, 189, 189] area=27617 border_gap=80 visible=27617 exact_key=0 key_fringe_hue=45 purple_halo_hue=210 key_hue_ratio=0.009233
- component_19: bbox=[956, 1044, 11, 60] area=624 border_gap=150 visible=624 exact_key=0 key_fringe_hue=38 purple_halo_hue=34 key_hue_ratio=0.115385
- component_20: bbox=[1140, 1045, 11, 59] area=600 border_gap=103 visible=600 exact_key=0 key_fringe_hue=12 purple_halo_hue=10 key_hue_ratio=0.036667
- component_21: bbox=[956, 1119, 53, 48] area=793 border_gap=87 visible=793 exact_key=0 key_fringe_hue=111 purple_halo_hue=26 key_hue_ratio=0.172762
- component_22: bbox=[1092, 1119, 59, 48] area=887 border_gap=87 visible=887 exact_key=0 key_fringe_hue=84 purple_halo_hue=16 key_hue_ratio=0.11274
- component_23: bbox=[1022, 1156, 57, 11] area=626 border_gap=87 visible=626 exact_key=0 key_fringe_hue=83 purple_halo_hue=9 key_hue_ratio=0.146965

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=375 hue_ratio=0.000961 score=375.096099
- #00ffff: exact=0 exact_ratio=0.0 hue_band=1568 hue_ratio=0.004018 score=1568.401824
- #ff0000: exact=0 exact_ratio=0.0 hue_band=16968 hue_ratio=0.043483 score=16972.348305
- #00ff00: exact=0 exact_ratio=0.0 hue_band=24681 hue_ratio=0.063249 score=24687.324877
- #ffff00: exact=2 exact_ratio=5e-06 hue_band=25409 hue_ratio=0.065114 score=27415.511438
- #ff00ff: exact=2759 exact_ratio=0.00707 hue_band=16854 hue_ratio=0.043191 score=2775858.319091
