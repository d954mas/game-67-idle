# Source Sheet Intake Audit: mine-cards-blank-ui-kit-v001-candidate-a-clean.png

status: fail
analysis_engine: numpy
size: 1536x1024
component_count: 11
closest_gap_px: 4
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ffff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ffff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 4159px of exact key-color-like art > allowed 0px
- component_2 contains 4114px of exact key-color-like art > allowed 0px
- component_3 contains 1750px of exact key-color-like art > allowed 0px
- component_4 contains 1718px of exact key-color-like art > allowed 0px
- component_5 contains 1583px of exact key-color-like art > allowed 0px
- component_6 contains 1800px of exact key-color-like art > allowed 0px
- component_7 contains 3333px of exact key-color-like art > allowed 0px
- component_8 contains 1706px of exact key-color-like art > allowed 0px
- component_9 contains 1769px of exact key-color-like art > allowed 0px
- component_9 key/halo hue conflict ratio 0.113 > allowed 0.050; choose a safer background or split/preserve this art
- component_10 contains 7744px of exact key-color-like art > allowed 0px
- component_10 key/halo hue conflict ratio 0.089 > allowed 0.050; choose a safer background or split/preserve this art
- component_11 contains 4272px of exact key-color-like art > allowed 0px
- closest component gap 4px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 11
- components_with_key_hue_conflict: 2
- total_exact_key_conflict_px: 33948
- total_key_fringe_hue_px: 151
- total_purple_halo_hue_px: 8078
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_9', 'ratio': 0.112596, 'bbox': [1126, 667, 372, 144]}

## Blocking Reasons
- code: key_color_conflict, count: 13, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_gutter, closest_gap_px: 4, minimum_px: 24, closest_pair: ['component_7', 'component_10'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ffff

## Components
- component_1: bbox=[783, 43, 710, 340] area=231073 border_gap=43 visible=231073 exact_key=4159 key_fringe_hue=76 purple_halo_hue=2260 key_hue_ratio=0.028108
- component_2: bbox=[43, 44, 716, 339] area=235048 border_gap=43 visible=235048 exact_key=4114 key_fringe_hue=0 purple_halo_hue=193 key_hue_ratio=0.018324
- component_3: bbox=[399, 393, 259, 243] area=59764 border_gap=388 visible=59764 exact_key=1750 key_fringe_hue=0 purple_halo_hue=6 key_hue_ratio=0.029382
- component_4: bbox=[75, 397, 255, 239] area=58583 border_gap=75 visible=58583 exact_key=1718 key_fringe_hue=11 purple_halo_hue=313 key_hue_ratio=0.034857
- component_5: bbox=[717, 433, 355, 165] area=55202 border_gap=426 visible=55202 exact_key=1583 key_fringe_hue=0 purple_halo_hue=1085 key_hue_ratio=0.048332
- component_6: bbox=[1123, 433, 365, 165] area=56655 border_gap=48 visible=56655 exact_key=1800 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.031771
- component_7: bbox=[47, 644, 610, 183] area=107500 border_gap=47 visible=107500 exact_key=3333 key_fringe_hue=21 purple_halo_hue=223 key_hue_ratio=0.033274
- component_8: bbox=[703, 667, 377, 144] area=51782 border_gap=213 visible=51782 exact_key=1706 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.032946
- component_9: bbox=[1126, 667, 372, 144] area=51121 border_gap=38 visible=51121 exact_key=1769 key_fringe_hue=43 purple_halo_hue=3944 key_hue_ratio=0.112596
- component_10: bbox=[67, 831, 1410, 72] area=87321 border_gap=59 visible=87321 exact_key=7744 key_fringe_hue=0 purple_halo_hue=49 key_hue_ratio=0.089245
- component_11: bbox=[67, 907, 1409, 70] area=88778 border_gap=47 visible=88778 exact_key=4272 key_fringe_hue=0 purple_halo_hue=5 key_hue_ratio=0.048176

## Candidate Key Colors
- #00ffff: exact=0 exact_ratio=0.0 hue_band=145 hue_ratio=0.000134 score=145.013391
- #ffff00: exact=0 exact_ratio=0.0 hue_band=2836 hue_ratio=0.002619 score=2836.261907
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=4202 hue_ratio=0.003881 score=4202.388058
- #ff0000: exact=0 exact_ratio=0.0 hue_band=5687 hue_ratio=0.005252 score=5687.525199
- #0000ff: exact=0 exact_ratio=0.0 hue_band=70192 hue_ratio=0.064823 score=70198.482291
- #00ff00: exact=31055 exact_ratio=0.02868 hue_band=114488 hue_ratio=0.105731 score=31169498.573065
