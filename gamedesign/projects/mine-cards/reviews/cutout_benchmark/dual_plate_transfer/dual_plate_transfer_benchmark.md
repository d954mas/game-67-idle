# Dual Plate Transfer Benchmark

AI plate sheet: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer/ai_plate_sheet_6row_raw.png`
Overview: `gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer/dual_plate_transfer_benchmark.png`

RGB is always taken from the selected source crop. AI white/black plates are used only as alpha candidates.

| case | raw pair iou | raw source agreement | aligned pair iou | aligned source agreement | verdict | row |
|---|---:|---:|---:|---:|---|---|
| green_plain_ring_bad | 0.518 | 0.144 | 0.522 | 0.749 | candidate mask | `gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer/green_plain_ring_bad_dual_transfer.png` |
| green_skull_scroll_bad | 0.607 | 0.169 | 0.616 | 0.77 | candidate mask | `gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer/green_skull_scroll_bad_dual_transfer.png` |
| white_shadow_armor_bad | 0.568 | 0.303 | 0.593 | 0.856 | candidate mask | `gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer/white_shadow_armor_bad_dual_transfer.png` |
| white_sign_semishadow_bad | 0.834 | 0.494 | 0.886 | 0.789 | candidate mask | `gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer/white_sign_semishadow_bad_dual_transfer.png` |
| white_floor_shadow_bad | 0.937 | 0.316 | 0.937 | 0.67 | transfer mismatch | `gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer/white_floor_shadow_bad_dual_transfer.png` |
| good_resheet_plain_ring_control | 0.505 | 0.09 | 0.513 | 0.641 | transfer mismatch | `gamedesign/projects/mine-cards/reviews/cutout_benchmark/dual_plate_transfer/good_resheet_plain_ring_control_dual_transfer.png` |
