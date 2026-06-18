# Source Sheet Intake Audit: mine-cards-icons-v001-candidate-d-chroma.png

status: fail
analysis_engine: numpy
size: 1254x1254
component_count: 15
closest_gap_px: 14
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 4271px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 0.286 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 contains 67px of exact key-color-like art > allowed 0px
- component_13 contains 2103px of exact key-color-like art > allowed 0px
- component_13 key/halo hue conflict ratio 0.164 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 14px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 3
- components_with_key_hue_conflict: 2
- total_exact_key_conflict_px: 6441
- total_key_fringe_hue_px: 6690
- total_purple_halo_hue_px: 11737
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.286231, 'bbox': [646, 58, 234, 298]}

## Blocking Reasons
- code: key_color_conflict, count: 5, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_gutter, closest_gap_px: 14, minimum_px: 24, closest_pair: ['component_5', 'component_7'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[646, 58, 234, 298] area=35646 border_gap=58 visible=35646 exact_key=4271 key_fringe_hue=2813 purple_halo_hue=3119 key_hue_ratio=0.286231
- component_2: bbox=[370, 60, 238, 281] area=38406 border_gap=60 visible=38406 exact_key=0 key_fringe_hue=242 purple_halo_hue=641 key_hue_ratio=0.022991
- component_3: bbox=[942, 69, 252, 279] area=48956 border_gap=60 visible=48956 exact_key=67 key_fringe_hue=332 purple_halo_hue=831 key_hue_ratio=0.025125
- component_4: bbox=[56, 93, 248, 249] area=40817 border_gap=56 visible=40817 exact_key=0 key_fringe_hue=156 purple_halo_hue=712 key_hue_ratio=0.021266
- component_5: bbox=[625, 392, 271, 275] area=55640 border_gap=358 visible=55640 exact_key=0 key_fringe_hue=141 purple_halo_hue=386 key_hue_ratio=0.009472
- component_6: bbox=[943, 397, 257, 270] area=54807 border_gap=54 visible=54807 exact_key=0 key_fringe_hue=242 purple_halo_hue=674 key_hue_ratio=0.016713
- component_7: bbox=[326, 398, 285, 271] area=44305 border_gap=326 visible=44305 exact_key=0 key_fringe_hue=267 purple_halo_hue=529 key_hue_ratio=0.017966
- component_8: bbox=[58, 415, 229, 226] area=33312 border_gap=58 visible=33312 exact_key=0 key_fringe_hue=187 purple_halo_hue=519 key_hue_ratio=0.021194
- component_9: bbox=[358, 706, 196, 218] area=30966 border_gap=330 visible=30966 exact_key=0 key_fringe_hue=126 purple_halo_hue=445 key_hue_ratio=0.01844
- component_10: bbox=[71, 710, 205, 215] area=30973 border_gap=71 visible=30973 exact_key=0 key_fringe_hue=138 purple_halo_hue=430 key_hue_ratio=0.018339
- component_11: bbox=[955, 714, 212, 215] area=21198 border_gap=87 visible=21198 exact_key=0 key_fringe_hue=189 purple_halo_hue=367 key_hue_ratio=0.026229
- component_12: bbox=[646, 718, 243, 208] area=33552 border_gap=328 visible=33552 exact_key=0 key_fringe_hue=215 purple_halo_hue=474 key_hue_ratio=0.020535
- component_13: bbox=[84, 975, 169, 240] area=31743 border_gap=39 visible=31743 exact_key=2103 key_fringe_hue=1399 purple_halo_hue=1713 key_hue_ratio=0.164288
- component_14: bbox=[665, 979, 217, 237] area=39733 border_gap=38 visible=39733 exact_key=0 key_fringe_hue=133 purple_halo_hue=528 key_hue_ratio=0.016636
- component_15: bbox=[366, 986, 199, 227] area=35385 border_gap=41 visible=35385 exact_key=0 key_fringe_hue=110 purple_halo_hue=369 key_hue_ratio=0.013537

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=228 hue_ratio=0.000396 score=228.039622
- #ff0000: exact=0 exact_ratio=0.0 hue_band=10918 hue_ratio=0.018973 score=10919.897334
- #00ffff: exact=0 exact_ratio=0.0 hue_band=17775 hue_ratio=0.030889 score=17778.088946
- #00ff00: exact=0 exact_ratio=0.0 hue_band=22381 hue_ratio=0.038894 score=22384.889378
- #ffff00: exact=71 exact_ratio=0.000123 hue_band=45978 hue_ratio=0.079901 score=116985.990074
- #ff00ff: exact=3390 exact_ratio=0.005891 hue_band=12203 hue_ratio=0.021206 score=3402205.120642
