# jam_optimized_ui_01 — derived UI art (size optimization pass)

All files in this folder are **derived art**: mechanical transforms (resize /
quantize / slice-9 crop) applied to already-committed source assets elsewhere
in this repo. License and provenance for the underlying pixels are unchanged
and still tracked at the original source path; this folder adds no new
license obligations.

Goal: shrink the shared `ui` atlas without visibly breaking anything
("легко" constraint) via (a) 1/2 Lanczos downscale + light palette
quantization for flat UI chrome, and (b) slice-9 middle-cropping for the
dialogue panel set. See `games/rb-dark-rpg/design/ui_ux/ui_asset_slicing_rules.md`
for the repo's slicing conventions.

## Part A — 1/2 downscale (+ quantize for flat chrome)

Recipe: Lanczos resize to exactly 1/2 (dimensions rounded to the nearest even
number), then for flat UI chrome (not the painterly portrait) requantize the
RGB channels only via `Image.quantize(colors=256, method=Image.FASTOCTREE)`
and re-attach the exact downscaled alpha channel unchanged. No blur/median
filters — the Lanczos averaging during resize is the denoise step.

| File | Source | Before (px / KB) | After (px / KB) | Quantized |
|---|---|---|---|---|
| nav_v11_map.png | `assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_map.png` | 301x365 / 192.5KB | 150x182 / 30.8KB | yes |
| nav_v11_journal.png | `assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_journal.png` | 299x365 / 187.8KB | 150x182 / 29.7KB | yes |
| nav_v11_equipment.png | `assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_equipment.png` | 296x365 / 186.8KB | 148x182 / 31.7KB | yes |
| nav_v11_place.png | `assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_place.png` | 296x365 / 180.7KB | 148x182 / 28.3KB | yes |
| nav_v11_more.png | `assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_more.png` | 299x365 / 167.6KB | 150x182 / 23.9KB | yes |
| gate_guard_portrait.png | `assets/ui/gate_guard_portrait.png` | 384x432 / 173.7KB | 192x216 / 43.9KB | no (painterly gradients — downscale only) |
| top_hud_portrait_frame.png | `assets/ui/generated/top_hud_tokens_02/slices/top_hud_portrait_frame.png` | 363x377 / 223.2KB | 182x188 / 20.6KB | yes |
| top_hud_resource_coin_chip.png | `assets/ui/generated/top_hud_tokens_02/slices/top_hud_resource_coin_chip.png` | 385x135 / 87.7KB | 192x68 / 10.9KB | yes |
| top_hud_resource_supplies_chip.png | `assets/ui/generated/top_hud_tokens_02/slices/top_hud_resource_supplies_chip.png` | 383x139 / 85.3KB | 192x70 / 10.4KB | yes |
| top_hud_status_plaque.png | `assets/ui/generated/top_hud_tokens_02/slices/top_hud_status_plaque.png` | 651x129 / 126.5KB | 326x64 / 10.7KB | yes |
| top_hud_settings_button.png | `assets/ui/generated/top_hud_tokens_02/slices/top_hud_settings_button.png` | 181x183 / 64.5KB | 90x92 / 9.4KB | yes |
| top_hud_level_badge.png | `assets/ui/generated/top_hud_tokens_02/slices/top_hud_level_badge.png` | 185x225 / 71.8KB | 92x112 / 9.6KB | yes |

`src/build_packs.c` region/sprite names are unchanged (`nav_v11_map`,
`top_hud_portrait_frame`, `gate_guard_portrait`, etc.) — only the source file
path was repointed to this folder.

## Part B — slice-9 middle-crop (dialogue panel set)

Border band widths were measured by visually inspecting each source's
top-left corner (ornament/rivet extent) and cross-checked against the
existing `DIALOGUE_*_BORDER*` constants in `src/build_packs.c`, which already
matched the real ornament extent (e.g. outer_frame's gold corner wedge ends
at x=33 of 34px border). The crop keeps the border bands byte-for-byte
(copied verbatim from source, corners untouched) and removes columns/rows
from the *center* of the stretchable middle only, down to an 80px middle tile
(inside the requested 64-96px range).

| File | Source | Before (px / KB) | After (px / KB) | Border kept (px) | Slice axes |
|---|---|---|---|---|---|
| dialogue_outer_frame.png | `assets/ui/generated/dialogue_panel_03/slices/dialogue_outer_frame.png` | 663x371 / 32.5KB | 148x148 / 10.3KB | 34 all sides | X+Y |
| dialogue_body_panel.png | `assets/ui/generated/dialogue_panel_03/slices/dialogue_body_panel.png` | 703x312 / 260.0KB | 124x124 / 18.0KB | 22 all sides | X+Y |
| dialogue_answer_normal.png | `assets/ui/generated/dialogue_panel_03/slices/dialogue_answer_normal.png` | 701x90 / 71.7KB | 168x90 / 18.2KB | 44 left/right | X only (top/bottom = 0, full height kept) |
| dialogue_answer_primary.png | `assets/ui/generated/dialogue_panel_03/slices/dialogue_answer_primary.png` | 701x90 / 75.3KB | 168x90 / 19.2KB | 44 left/right | X only (top/bottom = 0, full height kept) |

`src/build_packs.c` was updated: path repointed to this folder and
`slice9_left/right/top/bottom` set to the border widths above (measured on
the CROPPED image, which is identical to the source border width since only
the middle was removed).

### Skipped

- **dialogue_objective_panel.png** (`assets/ui/generated/dialogue_panel_03/slices/dialogue_objective_panel.png`,
  464x210) — NOT cropped. The panel has a large circular medallion/icon
  socket inset on the right side of the stretchable middle band. That socket
  is unique, non-repeating art (not a tileable leather/plaque surface); a
  middle-crop would either slice through the circle or delete it outright,
  visibly breaking the asset. Left untouched at the original source path and
  original `src/build_packs.c` slice9 values.
- **top_hud_hp_frame.png / top_hud_xp_frame.png** — explicitly out of scope
  per instructions; left untouched.
