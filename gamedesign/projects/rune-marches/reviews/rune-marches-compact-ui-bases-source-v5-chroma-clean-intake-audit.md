# Source Sheet Intake Audit: rune-marches-compact-ui-bases-source-v5-chroma-clean.png

status: pass
analysis_engine: numpy
size: 1536x1024
component_count: 11
closest_gap_px: 40
max_exact_key_conflict_px: 0
max_key_hue_conflict_ratio: 0.05
suggested_key_color: #ff00ff
key_color_action: keep_current_key_color
next_prompt_key_color: #00ff00
recommended_next_step: slice_ready

## Problems
- none

## Problem Summary
- components_with_border_gap: 0
- components_with_exact_key_conflict: 0
- components_with_key_hue_conflict: 0
- total_exact_key_conflict_px: 0
- total_key_fringe_hue_px: 0
- total_purple_halo_hue_px: 0
- gutter_below_min: false
- worst_key_hue_component: {'id': 'component_1', 'ratio': 0.0, 'bbox': [1149, 131, 338, 714]}

## Recommended Next Step
- action: slice_ready
- reason: source sheet passed intake checks
- key_color: #00ff00

## Components
- component_1: bbox=[1149, 131, 338, 714] area=239210 border_gap=49 visible=239210 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_2: bbox=[311, 203, 290, 111] area=31563 border_gap=203 visible=31563 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_3: bbox=[642, 203, 436, 111] area=47403 border_gap=203 visible=47403 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_4: bbox=[63, 204, 203, 110] area=22034 border_gap=63 visible=22034 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_5: bbox=[63, 389, 203, 111] area=22110 border_gap=63 visible=22110 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_6: bbox=[311, 389, 290, 111] area=31641 border_gap=311 visible=31641 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_7: bbox=[641, 389, 437, 111] area=47592 border_gap=389 visible=47592 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_8: bbox=[259, 589, 141, 148] area=16430 border_gap=259 visible=16430 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_9: bbox=[561, 590, 151, 144] area=21191 border_gap=290 visible=21191 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_10: bbox=[57, 807, 451, 88] area=31083 border_gap=57 visible=31083 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0
- component_11: bbox=[567, 807, 497, 88] area=34260 border_gap=129 visible=34260 exact_key=0 key_fringe_hue=0 purple_halo_hue=0 key_hue_ratio=0.0

## Candidate Key Colors
- #ff00ff: exact=0 exact_ratio=0.0 hue_band=16 hue_ratio=2.9e-05 score=16.002938
- #0000ff: exact=0 exact_ratio=0.0 hue_band=17 hue_ratio=3.1e-05 score=17.003122
- #00ffff: exact=0 exact_ratio=0.0 hue_band=3095 hue_ratio=0.005684 score=3095.568394
- #00ff00: exact=0 exact_ratio=0.0 hue_band=5606 hue_ratio=0.010295 score=5607.029536
- #ffff00: exact=0 exact_ratio=0.0 hue_band=9052 hue_ratio=0.016624 score=9053.662391
- #ff0000: exact=0 exact_ratio=0.0 hue_band=44168 hue_ratio=0.081114 score=44176.111409

## Timing
- load_image: 21.066 ms
- find_components: 28.78 ms
- merge_fragments: 0.04 ms
- key_conflicts: 49.475 ms
- candidate_key_scores: 59.58 ms
- component_rules: 0.021 ms
- gutter_scan: 0.357 ms
- total: 159.369 ms
