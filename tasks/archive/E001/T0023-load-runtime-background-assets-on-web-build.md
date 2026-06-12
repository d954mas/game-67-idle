---
id: T0023
title: Load runtime background assets on web build
status: done
epic: E001
priority: P1
tags: [fantasy-pocket-rpg, web, visual, assets]
created: 2026-06-11
updated: 2026-06-12
---

## What

Web/mobile builds should render the same GDD-derived runtime backgrounds as native builds, or deliberately ship an equivalent web-safe visual asset path.

## Done when

- [x] `load_runtime_texture` has a web path or web-specific equivalent
- [x] web/mobile screenshot shows the ruins/camp background or an approved equivalent
- [x] `cmake --build --preset game-wasm-release` still completes
- [x] web visual QA captures evidence under `build/captures/`

## Open questions

## Log

- 2026-06-12: Captured from release-readiness review. Native renders generated backgrounds; web currently falls back to flat/stamp visuals because `load_runtime_texture` returns unloaded on web.
- 2026-06-12: Started web-safe background integration using compact embedded RGBA variants generated from the source PNGs.
- 2026-06-12: Completed. Web release now embeds downsampled RGBA background variants and keeps them alive across the browser frame loop; validated with `cmake --build --preset game-native-debug`, `cmake --build --preset game-wasm-release`, and `build/captures/web_mobile_wasm_release_ruins.png`.
