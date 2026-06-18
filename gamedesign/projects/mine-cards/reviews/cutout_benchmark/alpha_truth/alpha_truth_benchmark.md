# Ground-Truth Alpha Benchmark

Overview: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/alpha_truth/alpha_truth_overview.png`

`alpha_sad` = mean |alpha - truth| (0..255, lower better). `mask_iou` higher better.
Path 1 (single-background) is well-posed only for opaque art + flat-key holes;
fractional alpha (soft shadow / glow / glass) needs path 2 (dual-plate).

## hard_edge_or_holes — PATH 1's domain (mean alpha SAD, lower is better)

| mode | mean alpha SAD |
|---|---:|
| dual min | 0.0 |
| dual proj | 0.0 |
| key matte | 1.27 |
| soft matte | 1.29 |
| pymatting240 | 1.29 |
| holes | 1.9 |
| current | 30.82 |
| aggressive | 30.82 |

## soft_or_transparent — PATH 2's domain (mean alpha SAD, lower is better)

| mode | mean alpha SAD |
|---|---:|
| dual min | 0.0 |
| dual proj | 0.0 |
| soft matte | 55.28 |
| pymatting240 | 64.93 |
| key matte | 65.19 |
| current | 102.46 |
| aggressive | 102.46 |
| holes | 102.46 |

## overall (mixed — for reference only) (mean alpha SAD, lower is better)

| mode | mean alpha SAD |
|---|---:|
| dual min | 0.0 |
| dual proj | 0.0 |
| soft matte | 33.69 |
| pymatting240 | 39.47 |
| key matte | 39.62 |
| holes | 62.24 |
| current | 73.8 |
| aggressive | 73.8 |


### ring_hole

| mode | path | alpha SAD | alpha RMSE | grad SAD | mask IoU |
|---|---|---:|---:|---:|---:|
| current | single | 48.84 | 110.3 | 3.352 | 0.68 |
| aggressive | single | 48.84 | 110.3 | 3.352 | 0.68 |
| holes | single | 2.34 | 17.52 | 2.877 | 0.952 |
| soft matte | single | 1.43 | 10.58 | 1.76 | 0.998 |
| pymatting240 | single | 1.36 | 9.05 | 1.556 | 0.991 |
| key matte | single | 1.43 | 10.82 | 1.731 | 0.998 |
| dual min | dual | 0.0 | 0.0 | 0.0 | 1.0 |
| dual proj | dual | 0.0 | 0.0 | 0.0 | 1.0 |

### soft_shadow

| mode | path | alpha SAD | alpha RMSE | grad SAD | mask IoU |
|---|---|---:|---:|---:|---:|
| current | single | 155.7 | 172.35 | 2.767 | 0.951 |
| aggressive | single | 155.7 | 172.35 | 2.767 | 0.951 |
| holes | single | 155.7 | 172.35 | 2.767 | 0.951 |
| soft matte | single | 62.37 | 77.88 | 2.007 | 0.964 |
| pymatting240 | single | 75.8 | 93.89 | 2.407 | 0.954 |
| key matte | single | 76.8 | 95.94 | 2.513 | 0.94 |
| dual min | dual | 0.0 | 0.0 | 0.0 | 1.0 |
| dual proj | dual | 0.0 | 0.0 | 0.0 | 1.0 |

### glow

| mode | path | alpha SAD | alpha RMSE | grad SAD | mask IoU |
|---|---|---:|---:|---:|---:|
| current | single | 97.66 | 132.48 | 4.81 | 0.884 |
| aggressive | single | 97.66 | 132.48 | 4.81 | 0.884 |
| holes | single | 97.66 | 132.48 | 4.81 | 0.884 |
| soft matte | single | 49.68 | 71.1 | 2.729 | 0.969 |
| pymatting240 | single | 65.37 | 90.99 | 3.213 | 0.961 |
| key matte | single | 65.01 | 90.99 | 3.243 | 0.94 |
| dual min | dual | 0.0 | 0.0 | 0.0 | 1.0 |
| dual proj | dual | 0.0 | 0.0 | 0.0 | 1.0 |

### hard_gear

| mode | path | alpha SAD | alpha RMSE | grad SAD | mask IoU |
|---|---|---:|---:|---:|---:|
| current | single | 12.79 | 54.96 | 2.238 | 0.868 |
| aggressive | single | 12.79 | 54.96 | 2.238 | 0.868 |
| holes | single | 1.47 | 11.53 | 1.744 | 0.965 |
| soft matte | single | 1.16 | 8.88 | 1.398 | 0.995 |
| pymatting240 | single | 1.23 | 8.44 | 1.421 | 0.983 |
| key matte | single | 1.1 | 9.17 | 1.32 | 0.995 |
| dual min | dual | 0.0 | 0.0 | 0.0 | 1.0 |
| dual proj | dual | 0.0 | 0.0 | 0.0 | 1.0 |

### glass_orb

| mode | path | alpha SAD | alpha RMSE | grad SAD | mask IoU |
|---|---|---:|---:|---:|---:|
| current | single | 54.02 | 83.65 | 2.771 | 0.976 |
| aggressive | single | 54.02 | 83.65 | 2.771 | 0.976 |
| holes | single | 54.02 | 83.65 | 2.771 | 0.976 |
| soft matte | single | 53.8 | 83.34 | 2.229 | 1.0 |
| pymatting240 | single | 53.61 | 83.1 | 1.971 | 0.994 |
| key matte | single | 53.77 | 83.34 | 2.178 | 1.0 |
| dual min | dual | 0.0 | 0.0 | 0.0 | 1.0 |
| dual proj | dual | 0.0 | 0.0 | 0.0 | 1.0 |
