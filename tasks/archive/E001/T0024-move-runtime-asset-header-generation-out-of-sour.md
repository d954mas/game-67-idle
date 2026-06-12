---
id: T0024
title: Move runtime asset header generation out of source tree
status: done
epic: E001
priority: P2
tags: [build, assets, cleanup]
created: 2026-06-11
updated: 2026-06-12
---

## What

Avoid preset-specific generated runtime asset headers in `src/generated/`; generated paths should live in the CMake build tree or be stable relative runtime paths.

## Done when

- [x] switching between native/debug/qa/release/wasm presets does not rewrite source-tree generated asset headers
- [x] runtime background loading still works on native
- [x] CMake include paths point to the build-tree generated header
- [x] native and wasm QA builds pass

## Open questions

## Log

- 2026-06-12: Captured from release-readiness code review. `src/generated/game_runtime_assets.h` contains preset-specific absolute paths and is rewritten by whichever preset ran last.
- 2026-06-12: Started moving `game_runtime_assets.h` to the CMake build tree while keeping state codegen in `src/generated/`.
- 2026-06-12: Completed. `game_runtime_assets.h` now generates under each preset's CMake build tree, `src/main.c` includes the build-tree header directly, and `src/generated/game_runtime_assets.h` was removed. Evidence: `cmake --build --preset game-native-debug`, `cmake --build --preset game-wasm-release`, `cmake --build --preset game-native-qa`, `cmake --build --preset game-wasm-qa`, and `build/captures/fantasy_slice_runtime_bg_ruins_t0024.png`.
