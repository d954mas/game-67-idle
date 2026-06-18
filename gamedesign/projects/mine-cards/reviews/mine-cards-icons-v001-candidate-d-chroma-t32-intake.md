# Source Sheet Intake Audit: mine-cards-icons-v001-candidate-d-chroma-t32.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 15
closest_gap_px: 13
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 4271px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 0.315 > allowed 0.050; choose a safer background or split/preserve this art
- component_2 key/halo hue conflict ratio 0.063 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 contains 67px of exact key-color-like art > allowed 0px
- component_3 key/halo hue conflict ratio 0.052 > allowed 0.050; choose a safer background or split/preserve this art
- component_4 key/halo hue conflict ratio 0.054 > allowed 0.050; choose a safer background or split/preserve this art
- component_6 contains 4px of exact key-color-like art > allowed 0px
- component_7 contains 59px of exact key-color-like art > allowed 0px
- component_7 key/halo hue conflict ratio 0.060 > allowed 0.050; choose a safer background or split/preserve this art
- component_8 key/halo hue conflict ratio 0.061 > allowed 0.050; choose a safer background or split/preserve this art
- component_11 key/halo hue conflict ratio 0.068 > allowed 0.050; choose a safer background or split/preserve this art
- component_13 contains 2103px of exact key-color-like art > allowed 0px
- component_13 key/halo hue conflict ratio 0.190 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 13px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 5
- components_with_key_hue_conflict: 8
- total_exact_key_conflict_px: 6504
- total_key_fringe_hue_px: 15633
- total_purple_halo_hue_px: 20680
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.314507, 'bbox': [645, 57, 235, 300]}

## Blocking Reasons
- code: key_color_conflict, count: 13, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_gutter, closest_gap_px: 13, minimum_px: 24, closest_pair: ['component_5', 'component_7'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[645, 57, 235, 300] area=36244 border_gap=57 visible=36244 exact_key=4271 key_fringe_hue=3411 purple_halo_hue=3717 key_hue_ratio=0.314507
- component_2: bbox=[369, 60, 239, 281] area=39205 border_gap=60 visible=39205 exact_key=0 key_fringe_hue=1041 purple_halo_hue=1440 key_hue_ratio=0.063283
- component_3: bbox=[942, 68, 252, 280] area=49629 border_gap=60 visible=49629 exact_key=67 key_fringe_hue=1005 purple_halo_hue=1504 key_hue_ratio=0.051905
- component_4: bbox=[55, 92, 250, 250] area=41494 border_gap=55 visible=41494 exact_key=0 key_fringe_hue=833 purple_halo_hue=1389 key_hue_ratio=0.05355
- component_5: bbox=[625, 391, 272, 277] area=56294 border_gap=357 visible=56294 exact_key=0 key_fringe_hue=795 purple_halo_hue=1040 key_hue_ratio=0.032597
- component_6: bbox=[942, 396, 258, 272] area=55505 border_gap=54 visible=55505 exact_key=4 key_fringe_hue=940 purple_halo_hue=1372 key_hue_ratio=0.041726
- component_7: bbox=[325, 398, 287, 272] area=45241 border_gap=325 visible=45241 exact_key=59 key_fringe_hue=1203 purple_halo_hue=1465 key_hue_ratio=0.060277
- component_8: bbox=[57, 414, 230, 228] area=33996 border_gap=57 visible=33996 exact_key=0 key_fringe_hue=871 purple_halo_hue=1203 key_hue_ratio=0.061007
- component_9: bbox=[358, 706, 197, 218] area=31405 border_gap=330 visible=31405 exact_key=0 key_fringe_hue=565 purple_halo_hue=884 key_hue_ratio=0.046139
- component_10: bbox=[70, 710, 206, 216] area=31428 border_gap=70 visible=31428 exact_key=0 key_fringe_hue=593 purple_halo_hue=885 key_hue_ratio=0.047028
- component_11: bbox=[954, 714, 214, 216] area=21661 border_gap=86 visible=21661 exact_key=0 key_fringe_hue=652 purple_halo_hue=830 key_hue_ratio=0.068418
- component_12: bbox=[646, 718, 244, 209] area=34039 border_gap=327 visible=34039 exact_key=0 key_fringe_hue=702 purple_halo_hue=961 key_hue_ratio=0.048856
- component_13: bbox=[83, 975, 171, 240] area=32189 border_gap=39 visible=32189 exact_key=2103 key_fringe_hue=1845 purple_halo_hue=2159 key_hue_ratio=0.189723
- component_14: bbox=[664, 979, 219, 237] area=40224 border_gap=38 visible=40224 exact_key=0 key_fringe_hue=624 purple_halo_hue=1019 key_hue_ratio=0.040846
- component_15: bbox=[365, 985, 200, 228] area=35828 border_gap=41 visible=35828 exact_key=0 key_fringe_hue=553 purple_halo_hue=812 key_hue_ratio=0.038099

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=228 hue_ratio=0.00039 score=228.039016
- #ff0000: exact=0 exact_ratio=0.0 hue_band=10918 hue_ratio=0.018683 score=10919.868298
- #00ffff: exact=0 exact_ratio=0.0 hue_band=17775 hue_ratio=0.030417 score=17778.041675
- #00ff00: exact=0 exact_ratio=0.0 hue_band=22381 hue_ratio=0.038299 score=22384.829858
- #ffff00: exact=71 exact_ratio=0.000121 hue_band=45978 hue_ratio=0.078678 score=116985.867799
- #ff00ff: exact=3446 exact_ratio=0.005897 hue_band=20751 hue_ratio=0.035509 score=3466754.550931
