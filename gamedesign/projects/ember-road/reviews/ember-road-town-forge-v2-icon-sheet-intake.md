# Source Sheet Intake Audit: ember-road-town-forge-v2-icons-normalized-green.png

status: fail
analysis_engine: numpy
size: 1536x1024
component_count: 11
closest_gap_px: 50
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #0000ff
key_color_action: regenerate_with_next_prompt_key_color
next_prompt_key_color: #0000ff
recommended_next_step: regenerate_source_sheet_with_safer_key_color

## Problems
- component_1 contains 1209px of exact key-color-like art > allowed 0px
- component_10 contains 3396px of exact key-color-like art > allowed 0px
- component_10 key/halo hue conflict ratio 0.110 > allowed 0.050; choose a safer background or split/preserve this art

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 2
- components_with_key_hue_conflict: 1
- total_exact_key_conflict_px: 4605
- total_key_fringe_hue_px: 0
- total_purple_halo_hue_px: 0
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_10', 'ratio': 0.110367, 'bbox': [568, 691, 161, 238]}

## Blocking Reasons
- code: key_color_conflict, count: 3, action: regenerate_source_sheet_with_safer_key_color

## Recommended Next Step
- action: regenerate_source_sheet_with_safer_key_color
- reason: current key color conflicts with visible component art or halo colors
- key_color: #0000ff

## Components
- component_1: bbox=[176, 55, 187, 295] area=35173 border_gap=55 visible=35173 exact_key=1209 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.034373
- component_2: bbox=[508, 74, 191, 271] area=33328 border_gap=74 visible=33328 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_3: bbox=[793, 92, 263, 254] area=36251 border_gap=92 visible=36251 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_4: bbox=[1124, 94, 289, 248] area=53510 border_gap=94 visible=53510 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_5: bbox=[473, 395, 248, 241] area=46220 border_gap=388 visible=46220 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_6: bbox=[1142, 407, 223, 225] area=39210 border_gap=171 visible=39210 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_7: bbox=[132, 409, 270, 223] area=50748 border_gap=132 visible=50748 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_8: bbox=[826, 411, 202, 219] area=24867 border_gap=394 visible=24867 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_9: bbox=[846, 688, 192, 240] area=26023 border_gap=96 visible=26023 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_10: bbox=[568, 691, 161, 238] area=30770 border_gap=95 visible=30770 exact_key=3396 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.110367
- component_11: bbox=[139, 712, 247, 217] area=41494 border_gap=95 visible=41494 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0

## Candidate Key Colors
- #0000ff: exact=0 exact_ratio=0.0 hue_band=0 hue_ratio=0.0 score=0.0
- #00ffff: exact=0 exact_ratio=0.0 hue_band=1 hue_ratio=2e-06 score=1.000239
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=6 hue_ratio=1.4e-05 score=6.001437
- #ffff00: exact=2 exact_ratio=5e-06 hue_band=27080 hue_ratio=0.064848 score=29086.484768
- #ff0000: exact=0 exact_ratio=0.0 hue_band=45057 hue_ratio=0.107897 score=45067.789667
- #00ff00: exact=3658 exact_ratio=0.00876 hue_band=12460 hue_ratio=0.029838 score=3670462.983759
