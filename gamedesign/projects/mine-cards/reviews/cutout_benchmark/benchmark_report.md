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
| source | 43710 | 13474 | 14128 | 14455 | 0 | 0.009 |
| current | 35732 | 6034 | 6301 | 6477 | 0 | 129.06 |
| aggressive | 35732 | 6034 | 6301 | 6477 | 0 | 145.889 |
| holes | 30209 | 881 | 886 | 954 | 0 | 145.488 |
| soft matte | 31150 | 203 | 203 | 203 | 0 | 104.073 |
| pymatting | 35959 | 333 | 3710 | 3769 | 0 | 1202.126 |

### green_skull_scroll_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_skull_scroll_bad.png`
Source: `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-v001-candidate-b-normalized.png`
Notes: Candidate B normalized component_5; key-color conflicts remain after background normalization.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 58786 | 21917 | 22538 | 22793 | 0 | 0.012 |
| current | 36574 | 444 | 544 | 581 | 0 | 174.805 |
| aggressive | 36574 | 444 | 544 | 581 | 0 | 180.689 |
| holes | 36009 | 0 | 0 | 16 | 0 | 173.813 |
| soft matte | 37046 | 1 | 1 | 1 | 0 | 134.518 |
| pymatting | 37827 | 0 | 34 | 34 | 0 | 185.261 |

### green_shadow_armor_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_shadow_armor_bad.png`
Source: `gamedesign/projects/mine-cards/art/candidates/mine-cards-equipment-source-sheet-shadow-problem-v001.png`
Notes: Armor crop flattened onto green-screen source; tests whether cutout preserves highlights and dark material shading.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 99468 | 26332 | 26380 | 26457 | 0 | 0.055 |
| current | 73011 | 0 | 0 | 0 | 0 | 21.531 |
| aggressive | 73011 | 0 | 0 | 0 | 0 | 15.231 |
| holes | 73011 | 0 | 0 | 0 | 0 | 17.552 |
| soft matte | 73136 | 0 | 0 | 0 | 0 | 241.735 |
| pymatting | 74233 | 0 | 0 | 0 | 0 | 299.642 |

### green_sign_semishadow_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_sign_semishadow_bad.png`
Source: `procedural:wooden_sign`
Notes: Controlled sign fixture with semitransparent cast shadow flattened onto green; shows how single-background extraction handles shadow softness.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 168360 | 123761 | 138067 | 138705 | 0 | 0.038 |
| current | 44577 | 172 | 14284 | 14922 | 0 | 38.611 |
| aggressive | 44577 | 172 | 14284 | 14922 | 0 | 38.79 |
| holes | 44577 | 172 | 14284 | 14922 | 0 | 36.799 |
| soft matte | 47243 | 2573 | 14671 | 14671 | 0 | 493.362 |
| pymatting | 47805 | 0 | 14418 | 14424 | 0 | 616.278 |

### green_floor_shadow_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_floor_shadow_bad.png`
Source: `assets/runtime/mine-cards-stage-ui-family-v001/stage/mine_floor_shadow.png`
Notes: Existing runtime shadow sprite flattened onto green; tests whether single-background extraction preserves soft alpha edges without green halo.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 36072 | 11692 | 18551 | 19040 | 0 | 0.006 |
| current | 17867 | 0 | 348 | 835 | 0 | 97.482 |
| aggressive | 17867 | 0 | 348 | 835 | 0 | 96.776 |
| holes | 17867 | 0 | 348 | 835 | 0 | 116.176 |
| soft matte | 24755 | 339 | 3875 | 3878 | 0 | 77.514 |
| pymatting | 24755 | 0 | 3875 | 3878 | 0 | 126.878 |

### green_angel_wings_glow_bad

Image: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/green_angel_wings_glow_bad.png`
Source: `gamedesign/projects/mine-cards/art/candidates/mine-cards-angel-wings-glow-black-v001.png`
Notes: Hard transparency case converted from black source to green-screen source: bright feathers, soft golden aura, sparkles, and semitransparent particles.

| mode | visible | key px | green spill | source-key spill | hidden rgb | time ms |
|---|---:|---:|---:|---:|---:|---:|
| source | 1572516 | 1108366 | 1108366 | 1108366 | 0 | 1.407 |
| current | 464150 | 0 | 0 | 0 | 0 | 1405.492 |
| aggressive | 464150 | 0 | 0 | 0 | 0 | 1412.324 |
| holes | 464150 | 0 | 0 | 0 | 0 | 1336.333 |
| soft matte | 464150 | 0 | 0 | 0 | 0 | 4758.143 |
| pymatting | 494126 | 0 | 2425 | 2646 | 0 | 5656.173 |
