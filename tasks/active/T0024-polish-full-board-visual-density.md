---
id: T0024
title: Polish full-board visual density
status: review
epic: ""
priority: P1
tags: [visual, ui, native, readability]
created: 2026-06-12
updated: 2026-06-12
---

## What

Improve the native PC full-board and stuck-board visual density so 12 visible
67 variants, the crate, and the CTA remain readable at 960x540. This is a
layout/rendering polish pass only; balance, state, and progression rules are
out of scope.

## Done when

- [x] Full-board/stuck screenshot shows less character/crate overlap while
      keeping the field-first Cow Evolution-style screen.
- [x] Better Crate HUD and FREE SLOT CTA remain readable.
- [x] Native PC Better Crate scenario captures both progression and stuck-board
      screenshots after the layout pass.
- [x] First-loop native scenario and pixel health still pass.

## Open questions

None.

## Log

- 2026-06-12: Started after HUD-polish review showed the stuck-board screenshot
  was mechanically correct but visually too dense around characters and crate.
- 2026-06-12: Improved full-board spacing and replaced the crate CTA with a
  compact gold in-field sign (`TAP/BOX` or `FREE/SLOT`) that stays readable in
  960x540 native screenshots.
- 2026-06-12: Evidence passed: `cmake --build --preset native-debug`;
  `py -3.12 tools/devapi/scenarios/full_board_density.py 9238 build/captures/scenarios/full_board_density_v10.png`;
  `py -3.12 tools/devapi/scenarios/better_crate_progression.py 9239 build/captures/scenarios/better_crate_density_v10.png`
  producing `build/captures/scenarios/better_crate_density_v10_stuck.png`;
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9240 build/captures/scenarios/first_67_loop_density_v10.png`;
  `py -3.12 tools/balance/simulate_67_world.py` passed at 53.92 minutes to
  Cosmic 67; pixel health passed for all four v10 screenshots.
