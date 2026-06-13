---
id: T0026
title: Validate native mobile portrait layout
status: review
epic: ""
priority: P1
tags: [mobile, native, layout, readability, validation]
created: 2026-06-12
updated: 2026-06-12
---

## What

Validate and improve the native 67 World layout at a mobile portrait viewport
without using a web build. The gate should prove that the primary HUD, board,
crate CTA, collection drawer, FTUE prompt, and stuck-board recovery remain
readable when the native PC harness is launched with a phone-like window size.

## Done when

- [x] DevAPI native launcher supports an explicit `--window-size` argument for
      scenario captures.
- [x] A native mobile portrait scenario captures first-loop and full-board
      screenshots at a phone-like viewport.
- [x] HUD, board, crate CTA, FTUE prompt, and collection drawer do not overlap
      incoherently in the captured portrait screenshots.
- [x] Pixel health passes for the portrait screenshots.
- [x] The normal 960x540 native first-loop/full-board scenarios still pass.

## Open questions

None.

## Log

- 2026-06-12: Started from release gate: mobile layout validation is still
  missing. Per project rule, this will use native PC `--window-size` capture,
  not a web/mobile build detour.
- 2026-06-12: Added native DevAPI `window_size` support and
  `mobile_portrait_layout.py`. Improved portrait layout in `src/main.c`:
  two-row HUD, compact bottom drawer, lower FTUE plaque, portrait CTA sign, and
  final art/text flush for readable overlay. Evidence passed:
  `cmake --build --preset native-debug`;
  `py -3.12 -m py_compile tools/devapi/scenarios/mobile_portrait_layout.py tools/devapi/devapi_client.py`;
  `py -3.12 tools/devapi/scenarios/mobile_portrait_layout.py 9253 build/captures/scenarios/mobile_portrait_v11.png 390x844`;
  `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/mobile_portrait_v11_first_loop.png`;
  `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/mobile_portrait_v11_stuck.png`;
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9254 build/captures/scenarios/first_67_loop_mobile_regression_v1.png`;
  `py -3.12 tools/devapi/scenarios/full_board_density.py 9255 build/captures/scenarios/full_board_density_mobile_regression_v1.png`;
  pixel health passed for both desktop regression screenshots;
  `cmake --build --preset native-release`;
  `py -3.12 tools/balance/simulate_67_world.py`;
  `node tools/package_native_release.mjs`.
