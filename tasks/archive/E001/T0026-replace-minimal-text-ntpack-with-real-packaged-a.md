---
id: T0026
title: Replace minimal text ntpack with real packaged asset format
status: done
epic: E001
priority: P2
tags: [release, assets, packaging]
created: 2026-06-11
updated: 2026-06-12
---

## What

Replace the current minimal text `.ntpack` release manifest with the real packaged asset format expected by the engine/runtime asset pipeline.

## Done when

- [x] pack builder writes the engine-approved binary/package format
- [x] pack includes or references the current slice assets intentionally
- [x] pack build fails on missing required assets instead of silently producing a placeholder
- [x] release packaging docs and commands name the final artifact path

## Open questions

## Log

- 2026-06-12: Captured from release-readiness code review. P0 empty-pack blocker was fixed with a non-empty minimal manifest pack, but final release packaging still needs the real pack format.
- 2026-06-12: Started replacing the hand-written text manifest with the engine builder's binary `.ntpack` output. Scope: include the two current fantasy slice background PNGs and a small manifest blob; keep runtime loading unchanged for this step.
- 2026-06-12: Completed. `src/build_packs.c` now uses `nt_builder_start_pack`, adds the two active fantasy slice background PNGs as texture assets plus `game_67_idle/manifest` as a blob, validates via `nt_builder_dump_pack`, and fails before building if required source assets are missing.
- 2026-06-12: Evidence: `cmake --build --preset pack-native-debug`, `cmake --build --preset pack-native-release`, `cmake --build --preset game-wasm-release`, missing-asset cwd check from `C:\tmp`, `node tools/taskboard/cli.mjs validate`; `build/game_67_idle/game_67_idle.ntpack` and `build/game_67_idle/wasm-release/assets/game_67_idle.ntpack` are 12,587,324 bytes, magic `NPAK`, version 2, asset_count 3.
