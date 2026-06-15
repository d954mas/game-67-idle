# Source Sheet Intake Audit: splash-rods-ui-icons-source-v2-magenta-clean.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 17
closest_gap_px: 0
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_4 key/halo hue conflict ratio 1.364 > allowed 0.050; choose a safer background or split/preserve this art
- component_7 contains 7px of exact key-color-like art > allowed 0px
- component_12 contains 839px of exact key-color-like art > allowed 0px
- component_12 key/halo hue conflict ratio 0.090 > allowed 0.050; choose a safer background or split/preserve this art
- component_13 key/halo hue conflict ratio 0.305 > allowed 0.050; choose a safer background or split/preserve this art
- component_14 key/halo hue conflict ratio 0.330 > allowed 0.050; choose a safer background or split/preserve this art
- component_15 key/halo hue conflict ratio 0.054 > allowed 0.050; choose a safer background or split/preserve this art
- component_16 key/halo hue conflict ratio 0.379 > allowed 0.050; choose a safer background or split/preserve this art
- component_17 key/halo hue conflict ratio 0.357 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 0px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 2
- components_with_key_hue_conflict: 7
- total_exact_key_conflict_px: 846
- total_key_fringe_hue_px: 79990
- total_purple_halo_hue_px: 83190
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_4', 'ratio': 1.36441, 'bbox': [69, 316, 522, 190]}

## Blocking Reasons
- code: key_color_conflict, count: 9, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_gutter, closest_gap_px: 0, minimum_px: 24, closest_pair: ['component_14', 'component_17'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[67, 61, 539, 218] area=111781 border_gap=61 visible=111781 exact_key=0 key_fringe_hue=801 purple_halo_hue=687 key_hue_ratio=0.013312
- component_2: bbox=[664, 63, 523, 217] area=108599 border_gap=63 visible=108599 exact_key=0 key_fringe_hue=510 purple_halo_hue=377 key_hue_ratio=0.008168
- component_3: bbox=[647, 315, 551, 193] area=98946 border_gap=56 visible=98946 exact_key=0 key_fringe_hue=530 purple_halo_hue=248 key_hue_ratio=0.007863
- component_4: bbox=[69, 316, 522, 190] area=95313 border_gap=69 visible=95313 exact_key=0 key_fringe_hue=61782 purple_halo_hue=68264 key_hue_ratio=1.36441
- component_5: bbox=[61, 536, 556, 214] area=115721 border_gap=61 visible=115721 exact_key=0 key_fringe_hue=871 purple_halo_hue=704 key_hue_ratio=0.01361
- component_6: bbox=[682, 574, 489, 145] area=67312 border_gap=83 visible=67312 exact_key=0 key_fringe_hue=927 purple_halo_hue=885 key_hue_ratio=0.026919
- component_7: bbox=[998, 760, 214, 215] area=32513 border_gap=42 visible=32513 exact_key=7 key_fringe_hue=427 purple_halo_hue=337 key_hue_ratio=0.023714
- component_8: bbox=[748, 761, 208, 220] area=23904 border_gap=273 visible=23904 exact_key=0 key_fringe_hue=585 purple_halo_hue=427 key_hue_ratio=0.042336
- component_9: bbox=[502, 773, 225, 201] area=31516 border_gap=280 visible=31516 exact_key=0 key_fringe_hue=384 purple_halo_hue=283 key_hue_ratio=0.021164
- component_10: bbox=[280, 780, 195, 198] area=32072 border_gap=276 visible=32072 exact_key=0 key_fringe_hue=347 purple_halo_hue=259 key_hue_ratio=0.018895
- component_11: bbox=[50, 783, 185, 186] area=27170 border_gap=50 visible=27170 exact_key=0 key_fringe_hue=300 purple_halo_hue=220 key_hue_ratio=0.019139
- component_12: bbox=[44, 995, 181, 220] area=33897 border_gap=39 visible=33897 exact_key=839 key_fringe_hue=1127 purple_halo_hue=1077 key_hue_ratio=0.089772
- component_13: bbox=[599, 1011, 316, 211] area=42137 border_gap=32 visible=42137 exact_key=0 key_fringe_hue=7188 purple_halo_hue=5679 key_hue_ratio=0.305361
- component_14: bbox=[972, 1018, 147, 174] area=10785 border_gap=62 visible=10785 exact_key=0 key_fringe_hue=1900 purple_halo_hue=1661 key_hue_ratio=0.330181
- component_15: bbox=[281, 1019, 244, 186] area=22191 border_gap=49 visible=22191 exact_key=0 key_fringe_hue=680 purple_halo_hue=529 key_hue_ratio=0.054482
- component_16: bbox=[1123, 1028, 74, 91] area=4001 border_gap=57 visible=4001 exact_key=0 key_fringe_hue=774 purple_halo_hue=742 key_hue_ratio=0.378905
- component_17: bbox=[1078, 1116, 86, 95] area=4674 border_gap=43 visible=4674 exact_key=0 key_fringe_hue=857 purple_halo_hue=811 key_hue_ratio=0.356868

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=3188 hue_ratio=0.003696 score=3188.369609
- #ff0000: exact=0 exact_ratio=0.0 hue_band=12094 hue_ratio=0.014022 score=12095.402151
- #00ff00: exact=0 exact_ratio=0.0 hue_band=22470 hue_ratio=0.026051 score=22472.605121
- #ffff00: exact=6 exact_ratio=7e-06 hue_band=50184 hue_ratio=0.058182 score=56189.818219
- #00ffff: exact=68 exact_ratio=7.9e-05 hue_band=224066 hue_ratio=0.259777 score=292091.977703
- #ff00ff: exact=833 exact_ratio=0.000966 hue_band=24003 hue_ratio=0.027829 score=857005.782853
