# Cutout Mode Benchmark

Overview: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/overview.png`

## Modes
- `source`: original crop, no alpha extraction.
- `current`: current conservative border-connected key extraction.
- `aggressive`: current extraction with visible decontamination.
- `holes`: exact key holes removed as well as border background.
- `soft matte`: distance-based soft alpha matte plus despill cleanup.
- `pymatting`: optional PyMatting closed-form alpha from chroma trimap.
- `dual plate`: alpha reconstructed from pixel-aligned light and dark background plates.

## Cases

### green_plain_ring_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_plain_ring_bad.png`
Source: `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-v001-candidate-b-normalized.png`
Notes: Candidate B normalized component_8; intake reported 6034 exact key conflicts.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 43710 | 13474 | 14128 | 14455 | 0 | 0.007 |
| current | 35732 | 6034 | 6301 | 6477 | 0 | 97.937 |
| aggressive | 35732 | 6034 | 6301 | 6477 | 0 | 97.377 |
| holes | 30209 | 881 | 886 | 954 | 0 | 102.248 |
| soft matte | 31150 | 203 | 203 | 203 | 0 | 78.706 |
| pymatting | 35959 | 333 | 3710 | 3769 | 0 | 994.787 |
| key matte | 31839 | 0 | 0 | 0 | 0 | 148.681 |

### green_skull_scroll_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_skull_scroll_bad.png`
Source: `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-v001-candidate-b-normalized.png`
Notes: Candidate B normalized component_5; key-color conflicts remain after background normalization.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 58786 | 21917 | 22538 | 22793 | 0 | 0.008 |
| current | 36574 | 444 | 544 | 581 | 0 | 129.081 |
| aggressive | 36574 | 444 | 544 | 581 | 0 | 125.019 |
| holes | 36009 | 0 | 0 | 16 | 0 | 123.952 |
| soft matte | 37046 | 1 | 1 | 1 | 0 | 105.758 |
| pymatting | 37827 | 0 | 34 | 34 | 0 | 155.918 |
| key matte | 37100 | 0 | 0 | 0 | 0 | 159.185 |

### green_shadow_armor_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_shadow_armor_bad.png`
Source: `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-sheet-shadow-problem-v001.png`
Notes: Armor crop flattened onto green-screen source; tests whether cutout preserves highlights and dark material shading.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 99468 | 26332 | 26380 | 26457 | 0 | 0.019 |
| current | 73011 | 0 | 0 | 0 | 0 | 13.503 |
| aggressive | 73011 | 0 | 0 | 0 | 0 | 12.842 |
| holes | 73011 | 0 | 0 | 0 | 0 | 13.147 |
| soft matte | 73136 | 0 | 0 | 0 | 0 | 193.803 |
| pymatting | 74233 | 0 | 0 | 0 | 0 | 258.384 |
| key matte | 73136 | 0 | 0 | 0 | 0 | 257.384 |

### green_sign_semishadow_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_sign_semishadow_bad.png`
Source: `procedural:wooden_sign`
Notes: Controlled sign fixture with semitransparent cast shadow flattened onto green; shows how single-background extraction handles shadow softness.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 168360 | 123761 | 138067 | 138705 | 0 | 0.021 |
| current | 44577 | 172 | 14284 | 14922 | 0 | 29.794 |
| aggressive | 44577 | 172 | 14284 | 14922 | 0 | 31.75 |
| holes | 44577 | 172 | 14284 | 14922 | 0 | 27.149 |
| soft matte | 47243 | 2573 | 14671 | 14671 | 0 | 335.356 |
| pymatting | 47805 | 0 | 14418 | 14424 | 0 | 490.995 |
| key matte | 46671 | 0 | 0 | 0 | 0 | 1384.378 |

### green_floor_shadow_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_floor_shadow_bad.png`
Source: `assets/runtime/mine-cards-stage-ui-family-v001/stage/mine_floor_shadow.png`
Notes: Existing runtime shadow sprite flattened onto green; tests whether single-background extraction preserves soft alpha edges without green halo.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 36072 | 11692 | 18551 | 19040 | 0 | 0.005 |
| current | 17867 | 0 | 348 | 835 | 0 | 75.802 |
| aggressive | 17867 | 0 | 348 | 835 | 0 | 74.045 |
| holes | 17867 | 0 | 348 | 835 | 0 | 72.467 |
| soft matte | 24755 | 339 | 3875 | 3878 | 0 | 63.313 |
| pymatting | 24755 | 0 | 3875 | 3878 | 0 | 119.89 |
| key matte | 24755 | 0 | 0 | 0 | 0 | 120.646 |

### green_angel_wings_glow_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_angel_wings_glow_bad.png`
Source: `gamedesign/projects/mine-cards/art/candidates/mine-cards-angel-wings-legendary-white-v001.png`
Notes: Real white-plate source used directly (no green recomposite). Single-background keys on white and necessarily eats the white feathers; only dual-plate (white+black) recovers them — a true path-2 asset.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 1572516 | 789493 | 0 | 871515 | 0 | 1.119 |
| current | 914441 | 142162 | 0 | 213440 | 0 | 882.62 |
| aggressive | 914441 | 142162 | 0 | 213440 | 0 | 865.302 |
| holes | 724900 | 4744 | 0 | 23899 | 0 | 850.266 |
| soft matte | 858556 | 21333 | 0 | 21333 | 0 | 3989.093 |
| pymatting | 932661 | 0 | 0 | 0 | 0 | 4130.481 |
| key matte | 894394 | 0 | 0 | 0 | 0 | 4902.151 |
