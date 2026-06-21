# Source Sheet Intake Audit: ember-road-town-forge-v2-icons-v003-normalized-magenta.png

status: fail
analysis_engine: numpy
size: 1672x941
component_count: 11
closest_gap_px: 34
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 937px of exact key-color-like art > allowed 0px
- component_1 key/halo hue conflict ratio 0.166 > allowed 0.050; choose a safer background or split/preserve this art
- component_2 contains 3px of exact key-color-like art > allowed 0px
- component_2 key/halo hue conflict ratio 0.050 > allowed 0.050; choose a safer background or split/preserve this art
- component_7 key/halo hue conflict ratio 0.058 > allowed 0.050; choose a safer background or split/preserve this art
- component_10 contains 1993px of exact key-color-like art > allowed 0px
- component_10 key/halo hue conflict ratio 0.253 > allowed 0.050; choose a safer background or split/preserve this art
- component_11 key/halo hue conflict ratio 0.059 > allowed 0.050; choose a safer background or split/preserve this art

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 3
- components_with_key_hue_conflict: 5
- total_exact_key_conflict_px: 2933
- total_key_fringe_hue_px: 7400
- total_purple_halo_hue_px: 10951
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_10', 'ratio': 0.253217, 'bbox': [1285, 587, 128, 197]}

## Blocking Reasons
- code: key_color_conflict, count: 8, action: regenerate_source_sheet_with_safer_key_color

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[143, 145, 158, 282] area=30522 border_gap=143 visible=30522 exact_key=937 key_fringe_hue=1802 purple_halo_hue=2325 key_hue_ratio=0.165913
- component_2: bbox=[419, 152, 193, 272] area=32907 border_gap=152 visible=32907 exact_key=3 key_fringe_hue=705 purple_halo_hue=944 key_hue_ratio=0.050202
- component_3: bbox=[1317, 174, 254, 249] area=51694 border_gap=101 visible=51694 exact_key=0 key_fringe_hue=513 purple_halo_hue=932 key_hue_ratio=0.027953
- component_4: bbox=[1005, 177, 256, 247] area=46741 border_gap=177 visible=46741 exact_key=0 key_fringe_hue=440 purple_halo_hue=832 key_hue_ratio=0.027214
- component_5: bbox=[713, 191, 233, 225] area=31276 border_gap=191 visible=31276 exact_key=0 key_fringe_hue=554 purple_halo_hue=1003 key_hue_ratio=0.049783
- component_6: bbox=[117, 552, 223, 228] area=37720 border_gap=117 visible=37720 exact_key=0 key_fringe_hue=400 purple_halo_hue=645 key_hue_ratio=0.027704
- component_7: bbox=[421, 559, 183, 214] area=22115 border_gap=168 visible=22115 exact_key=0 key_fringe_hue=536 purple_halo_hue=740 key_hue_ratio=0.057698
- component_8: bbox=[976, 566, 245, 217] area=41191 border_gap=158 visible=41191 exact_key=0 key_fringe_hue=392 purple_halo_hue=755 key_hue_ratio=0.027846
- component_9: bbox=[703, 571, 204, 205] area=32633 border_gap=165 visible=32633 exact_key=0 key_fringe_hue=323 purple_halo_hue=545 key_hue_ratio=0.026599
- component_10: bbox=[1285, 587, 128, 197] area=19738 border_gap=157 visible=19738 exact_key=1993 key_fringe_hue=1398 purple_halo_hue=1607 key_hue_ratio=0.253217
- component_11: bbox=[1447, 589, 132, 193] area=16269 border_gap=93 visible=16269 exact_key=0 key_fringe_hue=337 purple_halo_hue=623 key_hue_ratio=0.059008

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=2 hue_ratio=6e-06 score=2.000551
- #00ff00: exact=0 exact_ratio=0.0 hue_band=132 hue_ratio=0.000364 score=132.036383
- #00ffff: exact=0 exact_ratio=0.0 hue_band=1718 hue_ratio=0.004735 score=1718.473531
- #ffff00: exact=3 exact_ratio=8e-06 hue_band=25933 hue_ratio=0.071479 score=28940.147897
- #ff0000: exact=2 exact_ratio=6e-06 hue_band=41116 hue_ratio=0.113328 score=43127.332778
- #ff00ff: exact=1730 exact_ratio=0.004768 hue_band=12237 hue_ratio=0.033729 score=1742240.372877
