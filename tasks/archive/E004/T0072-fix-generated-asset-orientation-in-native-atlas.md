---
id: T0072
title: Fix generated asset orientation in native atlas
status: done
tags: []
epic: E004
priority: P0
created: 2026-06-16
updated: 2026-06-16
---

## What
Generated directional assets can look correct in source/contact sheets but
appear rotated/flipped in the native game. The visible example is the generated
`icon_pull.png`: source points up-right, but the game screenshot showed it as a
down-right arrow.

Likely cause in this game pack: `tools/critter_corral/build_packs.c` had
`atlas_opts.allow_transform = true`, allowing the atlas packer to rotate/flip
regions for packing. That is unsafe for generated UI/icons with semantic
direction unless every renderer path correctly compensates and a screenshot
test proves it.

Current fix: disable atlas transforms for the Critter Corral atlas and remove
the unverified importer-side preflip workaround. Keep the source PNGs
orientation-correct; let the pack preserve them.

## Done when

- [x] Runtime generated PNGs match source/contact-sheet orientation.
- [x] Critter Corral pack rebuilds with atlas transforms disabled.
- [x] Native screenshot shows `Lure Pull` arrow oriented like
      `gamedesign/projects/critter-corral/art/sprites/icon_pull.png`.
- [x] The fix is documented so future generated directional art does not use
      pack transforms by default.

## Open questions

## Log

- 2026-06-16: Lead reported recurring issue: art looks correct in source but
  appears flipped/rotated in game. Started orientation fix.
- 2026-06-16: Fixed in two places: disabled atlas transforms for the
  Critter Corral pack, and negated sprite Y scale in `emit_sprite` to match the
  game's Y-down projection. Verified `Lure Pull` arrow now matches source
  orientation in `build/captures/corral_portrait_upgrade.png`.
- 2026-06-16: product gate PASS (portrait-upgrade); review:
  `gamedesign/projects/critter-corral/reviews/T0072_portrait_upgrade_orientation_gate.md`.
- 2026-06-16: product gate PASS (landscape-play); review:
  `gamedesign/projects/critter-corral/reviews/T0072_landscape_play_orientation_gate.md`.
- 2026-06-16: product gate PASS (portrait-upgrade); review: gamedesign\projects\critter-corral\reviews\T0072_portrait_upgrade_orientation_gate.md; screenshot: build/captures/corral_portrait_upgrade.png; next: continue to the next narrow slice
- 2026-06-16: product gate PASS (landscape-play); review: gamedesign\projects\critter-corral\reviews\T0072_landscape_play_orientation_gate.md; screenshot: build/captures/corral_visual_ui_landscape_play.png; next: continue to the next narrow slice
