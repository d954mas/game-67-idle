# Source Sheet Intake Audit: ember-road-town-forge-v2-forge-lantern-resource-icon-sheet-v002.png

status: fail
analysis_engine: numpy
size: 1536x1024
component_count: 11
closest_gap_px: 22
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #00ff00
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #00ff00
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 1795px of exact key-color-like art > allowed 0px
- component_2 contains 2008px of exact key-color-like art > allowed 0px
- component_2 key/halo hue conflict ratio 0.055 > allowed 0.050; choose a safer background or split/preserve this art
- component_3 contains 1861px of exact key-color-like art > allowed 0px
- component_3 key/halo hue conflict ratio 0.055 > allowed 0.050; choose a safer background or split/preserve this art
- component_4 contains 1066px of exact key-color-like art > allowed 0px
- component_5 contains 919px of exact key-color-like art > allowed 0px
- component_6 contains 1007px of exact key-color-like art > allowed 0px
- component_7 contains 1412px of exact key-color-like art > allowed 0px
- component_7 key/halo hue conflict ratio 0.057 > allowed 0.050; choose a safer background or split/preserve this art
- component_8 contains 789px of exact key-color-like art > allowed 0px
- component_9 contains 847px of exact key-color-like art > allowed 0px
- component_10 contains 2977px of exact key-color-like art > allowed 0px
- component_10 key/halo hue conflict ratio 0.158 > allowed 0.050; choose a safer background or split/preserve this art
- component_11 contains 662px of exact key-color-like art > allowed 0px
- closest component gap 22px is below required 24px

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 11
- components_with_key_hue_conflict: 4
- total_exact_key_conflict_px: 15343
- total_key_fringe_hue_px: 12
- total_purple_halo_hue_px: 160
- gutter_below_min: true
- worst_key_hue_component: {'id': 'component_10', 'ratio': 0.157965, 'bbox': [1200, 630, 131, 199]}

## Blocking Reasons
- code: key_color_conflict, count: 15, action: regenerate_source_sheet_with_safer_key_color
- code: unsafe_gutter, closest_gap_px: 22, minimum_px: 24, closest_pair: ['component_10', 'component_11'], action: regenerate_source_sheet_with_more_gutter_and_safe_border

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #00ff00

## Components
- component_1: bbox=[96, 166, 178, 311] area=37238 border_gap=96 visible=37238 exact_key=1795 key_fringe_hue=0 purple_halo_hue=46 key_hue_ratio=0.049439
- component_2: bbox=[373, 189, 201, 291] area=37516 border_gap=189 visible=37516 exact_key=2008 key_fringe_hue=4 purple_halo_hue=65 key_hue_ratio=0.055363
- component_3: bbox=[635, 240, 248, 236] area=34133 border_gap=240 visible=34133 exact_key=1861 key_fringe_hue=0 purple_halo_hue=1 key_hue_ratio=0.054551
- component_4: bbox=[931, 240, 258, 240] area=47887 border_gap=240 visible=47887 exact_key=1066 key_fringe_hue=0 purple_halo_hue=3 key_hue_ratio=0.022323
- component_5: bbox=[1232, 247, 247, 234] area=46640 border_gap=57 visible=46640 exact_key=919 key_fringe_hue=0 purple_halo_hue=4 key_hue_ratio=0.01979
- component_6: bbox=[72, 583, 249, 253] area=45479 border_gap=72 visible=45479 exact_key=1007 key_fringe_hue=0 purple_halo_hue=1 key_hue_ratio=0.022164
- component_7: bbox=[377, 598, 197, 225] area=25722 border_gap=201 visible=25722 exact_key=1412 key_fringe_hue=8 purple_halo_hue=35 key_hue_ratio=0.056566
- component_8: bbox=[648, 608, 215, 216] area=36380 border_gap=200 visible=36380 exact_key=789 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.021688
- component_9: bbox=[918, 613, 238, 216] area=39008 border_gap=195 visible=39008 exact_key=847 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.021713
- component_10: bbox=[1200, 630, 131, 199] area=18846 border_gap=195 visible=18846 exact_key=2977 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.157965
- component_11: bbox=[1353, 634, 131, 195] area=16715 border_gap=52 visible=16715 exact_key=662 key_fringe_hue=0 purple_halo_hue=5 key_hue_ratio=0.039904

## Candidate Key Colors
- #00ff00: exact=0 exact_ratio=0.0 hue_band=15 hue_ratio=3.9e-05 score=15.00389
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=429 hue_ratio=0.001113 score=429.111266
- #00ffff: exact=0 exact_ratio=0.0 hue_band=2447 hue_ratio=0.006347 score=2447.634655
- #ffff00: exact=3 exact_ratio=8e-06 hue_band=28028 hue_ratio=0.072694 score=31035.269351
- #ff0000: exact=0 exact_ratio=0.0 hue_band=52198 hue_ratio=0.135381 score=52211.53809
- #0000ff: exact=11880 exact_ratio=0.030812 hue_band=23434 hue_ratio=0.060778 score=11903440.07785
