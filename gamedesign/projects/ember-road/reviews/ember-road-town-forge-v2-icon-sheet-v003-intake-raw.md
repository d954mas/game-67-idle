# Source Sheet Intake Audit: ember-road-town-forge-v2-forge-lantern-resource-icon-sheet-v003.png

status: fail
analysis_engine: numpy
size: 1672x941
component_count: 10
closest_gap_px: 0
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 border gap 0px is below required 24px
- component_1 contains 64972px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 2.201 > allowed 0.050; choose a safer background or split/preserve this art
- component_2 contains 6981px of exact key-color-like art > allowed 0px
- component_2 key/halo hue conflict ratio 0.531 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 contains 5097px of exact key-color-like art > allowed 0px
- component_3 key/halo hue conflict ratio 0.272 > allowed 0.050; choose a safer background or split/preserve this art
- component_4 contains 5415px of exact key-color-like art > allowed 0px
- component_4 key/halo hue conflict ratio 0.308 > allowed 0.050; choose a safer background or split/preserve this art
- component_5 contains 7511px of exact key-color-like art > allowed 0px
- component_5 key/halo hue conflict ratio 0.566 > allowed 0.050; choose a safer background or split/preserve this art
- component_6 contains 3244px of exact key-color-like art > allowed 0px
- component_6 key/halo hue conflict ratio 0.242 > allowed 0.050; choose a safer background or split/preserve this art
- component_7 contains 3166px of exact key-color-like art > allowed 0px
- component_7 key/halo hue conflict ratio 0.269 > allowed 0.050; choose a safer background or split/preserve this art
- component_8 contains 3684px of exact key-color-like art > allowed 0px
- component_8 key/halo hue conflict ratio 0.460 > allowed 0.050; choose a safer background or split/preserve this art
- component_9 contains 3796px of exact key-color-like art > allowed 0px
- component_9 key/halo hue conflict ratio 0.261 > allowed 0.050; choose a safer background or split/preserve this art
- component_10 contains 9430px of exact key-color-like art > allowed 0px
- component_10 key/halo hue conflict ratio 0.620 > allowed 0.050; choose a safer background or split/preserve this art
- closest component gap 0px is below required 24px

## Problem Summary
- components_with_border_gap: 1
- components_with_exact_key_conflict: 10
- components_with_key_hue_conflict: 10
- total_exact_key_conflict_px: 113296
- total_key_fringe_hue_px: 56633
- total_purple_halo_hue_px: 60184
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_1', 'ratio': 2.200693, 'bbox': [0, 0, 1672, 941]}

## Blocking Reasons
- code: key_color_conflict, count: 20, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_border, count: 1, minimum_px: 24, action: regenerate_source_sheet_with_more_gutter_and_safe_border
- code: unsafe_gutter, closest_gap_px: 0, minimum_px: 24, closest_pair: ['component_1', 'component_2'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[0, 0, 1672, 941] area=40136 border_gap=0 visible=40136 exact_key=64972 key_fringe_hue=11416 purple_halo_hue=11939 key_hue_ratio=2.200693
- component_2: bbox=[413, 147, 216, 282] area=38920 border_gap=147 visible=38920 exact_key=6981 key_fringe_hue=6718 purple_halo_hue=6957 key_hue_ratio=0.53073
- component_3: bbox=[1308, 164, 280, 265] area=56060 border_gap=84 visible=56060 exact_key=5097 key_fringe_hue=4879 purple_halo_hue=5298 key_hue_ratio=0.272458
- component_4: bbox=[994, 175, 286, 260] area=51286 border_gap=175 visible=51286 exact_key=5415 key_fringe_hue=4985 purple_halo_hue=5377 key_hue_ratio=0.307628
- component_5: bbox=[708, 184, 266, 244] area=37285 border_gap=184 visible=37285 exact_key=7511 key_fringe_hue=6563 purple_halo_hue=7012 key_hue_ratio=0.565536
- component_6: bbox=[112, 545, 236, 237] area=40467 border_gap=112 visible=40467 exact_key=3244 key_fringe_hue=3147 purple_halo_hue=3392 key_hue_ratio=0.241753
- component_7: bbox=[698, 555, 216, 230] area=35377 border_gap=156 visible=35377 exact_key=3166 key_fringe_hue=3067 purple_halo_hue=3289 key_hue_ratio=0.269158
- component_8: bbox=[417, 557, 196, 219] area=25493 border_gap=165 visible=25493 exact_key=3684 key_fringe_hue=3914 purple_halo_hue=4118 key_hue_ratio=0.459577
- component_9: bbox=[966, 560, 269, 228] area=44525 border_gap=153 visible=44525 exact_key=3796 key_fringe_hue=3726 purple_halo_hue=4089 key_hue_ratio=0.260775
- component_10: bbox=[1277, 581, 315, 206] area=42490 border_gap=80 visible=42490 exact_key=9430 key_fringe_hue=8218 purple_halo_hue=8713 key_hue_ratio=0.620405

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=2 hue_ratio=5e-06 score=2.000485
- #00ff00: exact=0 exact_ratio=0.0 hue_band=132 hue_ratio=0.00032 score=132.032036
- #00ffff: exact=0 exact_ratio=0.0 hue_band=1718 hue_ratio=0.00417 score=1718.416951
- #ffff00: exact=3 exact_ratio=7e-06 hue_band=25933 hue_ratio=0.062938 score=28939.293822
- #ff0000: exact=2 exact_ratio=5e-06 hue_band=41116 hue_ratio=0.099787 score=43125.978667
- #ff00ff: exact=48840 exact_ratio=0.118532 hue_band=61470 hue_ratio=0.149185 score=48901484.918491
