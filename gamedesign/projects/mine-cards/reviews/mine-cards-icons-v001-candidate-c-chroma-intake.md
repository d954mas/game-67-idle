# Source Sheet Intake Audit: mine-cards-icons-v001-candidate-c-chroma.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 31
closest_gap_px: 9
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #ff00ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #ff00ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 4335px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 0.137 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 contains 40px of exact key-color-like art > allowed 0px
- component_6 contains 18px of exact key-color-like art > allowed 0px
- component_9 key/halo hue conflict ratio 0.073 > allowed 0.050; choose a safer background or split/preserve this art
- component_13 contains 1995px of exact key-color-like art > allowed 0px
- component_13 key/halo hue conflict ratio 0.070 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 9px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 4
- components_with_key_hue_conflict: 3
- total_exact_key_conflict_px: 6388
- total_key_fringe_hue_px: 9
- total_purple_halo_hue_px: 2581
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.137057, 'bbox': [659, 52, 221, 276]}

## Blocking Reasons
- code: key_color_conflict, count: 7, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_gutter, closest_gap_px: 9, minimum_px: 24, closest_pair: ['component_17', 'component_18'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #ff00ff

## Components
- component_1: bbox=[659, 52, 221, 276] area=31724 border_gap=52 visible=31724 exact_key=4335 key_fringe_hue=0 purple_halo_hue=13 key_hue_ratio=0.137057
- component_2: bbox=[372, 53, 214, 256] area=32288 border_gap=53 visible=32288 exact_key=0 key_fringe_hue=0 purple_halo_hue=34 key_hue_ratio=0.001053
- component_3: bbox=[956, 62, 233, 255] area=40711 border_gap=62 visible=40711 exact_key=40 key_fringe_hue=1 purple_halo_hue=26 key_hue_ratio=0.001646
- component_4: bbox=[62, 90, 228, 224] area=33711 border_gap=62 visible=33711 exact_key=0 key_fringe_hue=0 purple_halo_hue=3 key_hue_ratio=8.9e-05
- component_5: bbox=[642, 378, 249, 247] area=45564 border_gap=363 visible=45564 exact_key=0 key_fringe_hue=1 purple_halo_hue=6 key_hue_ratio=0.000154
- component_6: bbox=[338, 383, 259, 246] area=36653 border_gap=338 visible=36653 exact_key=18 key_fringe_hue=2 purple_halo_hue=55 key_hue_ratio=0.002046
- component_7: bbox=[957, 384, 235, 242] area=44715 border_gap=62 visible=44715 exact_key=0 key_fringe_hue=1 purple_halo_hue=99 key_hue_ratio=0.002236
- component_8: bbox=[71, 397, 206, 203] area=26613 border_gap=71 visible=26613 exact_key=0 key_fringe_hue=1 purple_halo_hue=36 key_hue_ratio=0.00139
- component_9: bbox=[961, 689, 221, 205] area=29712 border_gap=72 visible=29712 exact_key=0 key_fringe_hue=0 purple_halo_hue=2176 key_hue_ratio=0.073236
- component_10: bbox=[651, 692, 220, 191] area=27675 border_gap=371 visible=27675 exact_key=0 key_fringe_hue=1 purple_halo_hue=17 key_hue_ratio=0.00065
- component_11: bbox=[366, 693, 210, 201] area=27793 border_gap=360 visible=27793 exact_key=0 key_fringe_hue=0 purple_halo_hue=71 key_hue_ratio=0.002555
- component_12: bbox=[78, 699, 211, 197] area=26741 border_gap=78 visible=26741 exact_key=0 key_fringe_hue=1 purple_halo_hue=0 key_hue_ratio=3.7e-05
- component_13: bbox=[86, 963, 158, 225] area=28625 border_gap=66 visible=28625 exact_key=1995 key_fringe_hue=1 purple_halo_hue=0 key_hue_ratio=0.069729
- component_14: bbox=[667, 968, 201, 221] area=34142 border_gap=65 visible=34142 exact_key=0 key_fringe_hue=0 purple_halo_hue=17 key_hue_ratio=0.000498
- component_15: bbox=[370, 977, 186, 209] area=30467 border_gap=68 visible=30467 exact_key=0 key_fringe_hue=0 purple_halo_hue=28 key_hue_ratio=0.000919
- component_16: bbox=[969, 982, 37, 47] area=607 border_gap=225 visible=607 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_17: bbox=[1016, 982, 33, 10] area=268 border_gap=205 visible=268 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_18: bbox=[1058, 983, 24, 9] area=191 border_gap=172 visible=191 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_19: bbox=[1092, 983, 32, 9] area=286 border_gap=130 visible=286 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_20: bbox=[1134, 983, 31, 47] area=549 border_gap=89 visible=549 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_21: bbox=[969, 1040, 9, 29] area=261 border_gap=185 visible=261 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_22: bbox=[1156, 1040, 9, 29] area=261 border_gap=89 visible=261 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_23: bbox=[969, 1079, 9, 30] area=263 border_gap=145 visible=263 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_24: bbox=[1156, 1079, 9, 30] area=265 border_gap=89 visible=265 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_25: bbox=[969, 1119, 9, 26] area=228 border_gap=109 visible=228 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_26: bbox=[1155, 1119, 10, 26] area=224 border_gap=89 visible=224 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_27: bbox=[969, 1154, 34, 30] area=420 border_gap=70 visible=420 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_28: bbox=[1135, 1154, 30, 31] area=386 border_gap=69 visible=386 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_29: bbox=[1013, 1175, 32, 9] area=286 border_gap=70 visible=286 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_30: bbox=[1055, 1175, 28, 9] area=251 border_gap=70 visible=251 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_31: bbox=[1093, 1175, 32, 10] area=274 border_gap=69 visible=274 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0

## Candidate Key Colors
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=1374 hue_ratio=0.002736 score=1374.273621
- #ff0000: exact=0 exact_ratio=0.0 hue_band=14316 hue_ratio=0.028509 score=14318.850918
- #00ffff: exact=0 exact_ratio=0.0 hue_band=15698 hue_ratio=0.031261 score=15701.126133
- #00ff00: exact=0 exact_ratio=0.0 hue_band=19250 hue_ratio=0.038335 score=19253.833485
- #ffff00: exact=111 exact_ratio=0.000221 hue_band=37081 hue_ratio=0.073844 score=148088.384388
- #0000ff: exact=5876 exact_ratio=0.011702 hue_band=21080 hue_ratio=0.041979 score=5897084.197915
