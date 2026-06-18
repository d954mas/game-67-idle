# Source Sheet Intake Audit: mine-cards-icons-v001-candidate-b.png

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
- component_1 border gap 0px is below required 24px
- component_1 contains 313326px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 13.646 > allowed 0.050; choose a safer background or split/preserve this art
- component_2 contains 14695px of exact key-color-like art > allowed 0px
- component_2 key/halo hue conflict ratio 0.885 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 contains 18687px of exact key-color-like art > allowed 0px
- component_3 key/halo hue conflict ratio 1.359 > allowed 0.050; choose a safer background or split/preserve this art
- component_4 contains 16719px of exact key-color-like art > allowed 0px
- component_4 key/halo hue conflict ratio 1.221 > allowed 0.050; choose a safer background or split/preserve this art
- component_5 contains 21145px of exact key-color-like art > allowed 0px
- component_5 key/halo hue conflict ratio 1.804 > allowed 0.050; choose a safer background or split/preserve this art
- component_6 contains 10009px of exact key-color-like art > allowed 0px
- component_6 key/halo hue conflict ratio 0.572 > allowed 0.050; choose a safer background or split/preserve this art
- component_7 contains 10301px of exact key-color-like art > allowed 0px
- component_7 key/halo hue conflict ratio 0.588 > allowed 0.050; choose a safer background or split/preserve this art
- component_8 contains 13702px of exact key-color-like art > allowed 0px
- component_8 key/halo hue conflict ratio 1.136 > allowed 0.050; choose a safer background or split/preserve this art
- component_9 contains 12508px of exact key-color-like art > allowed 0px
- component_9 key/halo hue conflict ratio 0.820 > allowed 0.050; choose a safer background or split/preserve this art
- component_10 contains 17618px of exact key-color-like art > allowed 0px
- component_10 key/halo hue conflict ratio 1.517 > allowed 0.050; choose a safer background or split/preserve this art
- component_11 contains 4221px of exact key-color-like art > allowed 0px
- component_11 key/halo hue conflict ratio 0.508 > allowed 0.050; choose a safer background or split/preserve this art
- component_12 contains 8203px of exact key-color-like art > allowed 0px
- component_12 key/halo hue conflict ratio 0.854 > allowed 0.050; choose a safer background or split/preserve this art
- component_13 contains 5385px of exact key-color-like art > allowed 0px
- component_13 key/halo hue conflict ratio 0.692 > allowed 0.050; choose a safer background or split/preserve this art
- component_14 border gap 23px is below required 24px
- component_14 contains 17579px of exact key-color-like art > allowed 0px
- component_14 key/halo hue conflict ratio 0.953 > allowed 0.050; choose a safer background or split/preserve this art
- component_15 border gap 14px is below required 24px
- component_15 contains 21041px of exact key-color-like art > allowed 0px
- component_15 key/halo hue conflict ratio 1.321 > allowed 0.050; choose a safer background or split/preserve this art
- component_16 contains 38845px of exact key-color-like art > allowed 0px
- component_16 key/halo hue conflict ratio 3.470 > allowed 0.050; choose a safer background or split/preserve this art
- component_17 contains 6058px of exact key-color-like art > allowed 0px
- component_17 key/halo hue conflict ratio 0.487 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 0px is below required 24px

## Problem Summary
- components_with_border_gap: 3
- components_with_exact_key_conflict: 17
- components_with_key_hue_conflict: 17
- total_exact_key_conflict_px: 550042
- total_key_fringe_hue_px: 198636
- total_purple_halo_hue_px: 203593
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_1', 'ratio': 13.646068, 'bbox': [0, 0, 1254, 1254]}

## Blocking Reasons
- code: key_color_conflict, count: 34, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 3, minimum_px: 24, action: regenerate_source_sheet_with_more_gutter_and_safe_border
- code: unsafe_gutter, closest_gap_px: 0, minimum_px: 24, closest_pair: ['component_1', 'component_2'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[0, 0, 1254, 1254] area=26895 border_gap=0 visible=26895 exact_key=313326 key_fringe_hue=26843 purple_halo_hue=26842 key_hue_ratio=13.646068
- component_2: bbox=[336, 64, 264, 264] area=43274 border_gap=64 visible=43274 exact_key=14695 key_fringe_hue=11679 purple_halo_hue=11925 key_hue_ratio=0.885035
- component_3: bbox=[57, 69, 224, 266] area=34867 border_gap=57 visible=34867 exact_key=18687 key_fringe_hue=14087 purple_halo_hue=14595 key_hue_ratio=1.358563
- component_4: bbox=[925, 76, 257, 259] area=38109 border_gap=72 visible=38109 exact_key=16719 key_fringe_hue=14681 purple_halo_hue=15124 key_hue_ratio=1.220814
- component_5: bbox=[633, 77, 276, 251] area=28488 border_gap=77 visible=28488 exact_key=21145 key_fringe_hue=15030 purple_halo_hue=15226 key_hue_ratio=1.804304
- component_6: bbox=[930, 336, 255, 277] area=45377 border_gap=69 visible=45377 exact_key=10009 key_fringe_hue=7872 purple_halo_hue=8078 key_hue_ratio=0.572074
- component_7: bbox=[587, 361, 300, 266] area=46984 border_gap=361 visible=46984 exact_key=10301 key_fringe_hue=8507 purple_halo_hue=8825 key_hue_ratio=0.588136
- component_8: bbox=[57, 371, 226, 246] area=36373 border_gap=57 visible=36373 exact_key=13702 key_fringe_hue=13401 purple_halo_hue=14219 key_hue_ratio=1.136062
- component_9: bbox=[329, 386, 269, 235] area=38315 border_gap=329 visible=38315 exact_key=12508 key_fringe_hue=9319 purple_halo_hue=9594 key_hue_ratio=0.82007
- component_10: bbox=[947, 665, 234, 242] area=32354 border_gap=73 visible=32354 exact_key=17618 key_fringe_hue=15520 purple_halo_hue=15955 key_hue_ratio=1.51737
- component_11: bbox=[356, 679, 214, 207] area=27499 border_gap=356 visible=27499 exact_key=4221 key_fringe_hue=4732 purple_halo_hue=5027 key_hue_ratio=0.508382
- component_12: bbox=[658, 700, 225, 184] area=28215 border_gap=370 visible=28215 exact_key=8203 key_fringe_hue=7825 purple_halo_hue=8056 key_hue_ratio=0.853589
- component_13: bbox=[52, 707, 216, 167] area=21379 border_gap=52 visible=21379 exact_key=5385 key_fringe_hue=4547 purple_halo_hue=4858 key_hue_ratio=0.6918
- component_14: bbox=[324, 932, 266, 299] area=43768 border_gap=23 visible=43768 exact_key=17579 key_fringe_hue=11844 purple_halo_hue=12283 key_hue_ratio=0.952888
- component_15: bbox=[45, 939, 215, 301] area=37674 border_gap=14 visible=37674 exact_key=21041 key_fringe_hue=14173 purple_halo_hue=14538 key_hue_ratio=1.320592
- component_16: bbox=[947, 950, 246, 244] area=19072 border_gap=60 visible=19072 exact_key=38845 key_fringe_hue=13814 purple_halo_hue=13521 key_hue_ratio=3.470008
- component_17: bbox=[651, 960, 226, 219] area=32334 border_gap=75 visible=32334 exact_key=6058 key_fringe_hue=4762 purple_halo_hue=4927 key_hue_ratio=0.487011

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=375 hue_ratio=0.000645 score=375.064546
- #00ffff: exact=0 exact_ratio=0.0 hue_band=1569 hue_ratio=0.002701 score=1569.270062
- #ff0000: exact=0 exact_ratio=0.0 hue_band=17058 hue_ratio=0.029361 score=17060.936089
- #00ff00: exact=0 exact_ratio=0.0 hue_band=24960 hue_ratio=0.042962 score=24964.296211
- #ffff00: exact=2 exact_ratio=3e-06 hue_band=25541 hue_ratio=0.043962 score=27545.396215
- #ff00ff: exact=168728 exact_ratio=0.290421 hue_band=206084 hue_ratio=0.35472 score=168934119.471972
