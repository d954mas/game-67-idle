---
id: T0025
title: Replace temporary WASM release workaround with optimized web release
status: dropped
epic: E001
priority: P2
tags: [wasm, release, build]
created: 2026-06-11
updated: 2026-06-12
---

## What

Replace the current low-optimization `EMCC_DEBUG=1` workaround with a stable optimized web release build once the local Emscripten cleanup/lock hang is understood.

## Done when

- [ ] `game-wasm-release` builds without `EMCC_DEBUG=1`
- [ ] release optimization is restored to an approved release level
- [ ] build does not timeout in this Windows workspace
- [ ] web/mobile visual QA still passes

## Open questions

## Log

- 2026-06-12: Captured from release-readiness code review. `wasm-release` now produces shippable artifacts, but uses the same debug-save workaround and `-O0` flags as QA to avoid Emscripten timeouts in this environment.
- 2026-06-12: Started restoring `wasm-release` to an optimized release preset while leaving `wasm-qa` as the fast low-optimization harness.
- 2026-06-12: Review finding: `-O2` without `EMCC_DEBUG=1` still hangs in this Windows workspace; killed the leftover Python/Emscripten process after timeout. Testing `-O1` as the stable optimized release level instead of returning to `-O0`.
- 2026-06-12: Review finding: packed/smaller embedded asset arrays fixed optimized C compile pressure, but optimized Emscripten link still hangs. Testing compile `-O1` with link `-O0` to keep C optimization while avoiding the Windows post-link hang.
- 2026-06-12: Local Emscripten audit found `EMCC_DEBUG_SAVE=1`, a narrower temp-preservation switch that avoids cleanup without enabling `EMCC_DEBUG`. Testing release with `EMCC_DEBUG_SAVE=1`, compile-only `-O1`, and link `-O0`.
- 2026-06-12: Review finding: `EMCC_DEBUG_SAVE=1` plus compile-only `-O1` still timed out after 240s and left a stale `.ninja_lock`. Removed only the stale lock, restored `wasm-release` to the last known stable `EMCC_DEBUG=1`/`-O0` workaround so release artifacts remain buildable, and kept the split generated asset source/header optimization for lower compile pressure. T0025 remains open.
- 2026-06-12: Stabilization evidence after restore: `cmake --preset wasm-release`, `cmake --build --preset game-wasm-release` completed in this workspace, no stale `.ninja_lock` or build child process remained, and web/mobile visual QA passed at `build/captures/web_mobile_wasm_release_ruins_t0025_restore.png`.
- 2026-06-12: Director iteration 1 regression check: Developer tried removing the workaround and moving `wasm-release` to conservative `-O1`; `game-wasm-release` reached the final step then returned exit 1 without useful stdout diagnostics. Director changed release flags back to `-O0 -DNDEBUG` and reconfigured successfully, but `game-wasm-release` still timed out after 180s on the final link and left a stale `.ninja_lock`; the stale lock was removed. Current reliable web test target is `game-wasm-qa`; `game-wasm-release` is not RC-ready.
- 2026-06-12: Director iteration 2 restored `EMCC_DEBUG=1` on `wasm-release`/`game-wasm-release` with `-O0 -DNDEBUG` so the Poki test release path is reproducible again. Evidence: `cmake --preset wasm-release`, `cmake --build --preset game-wasm-release`, no stale `.ninja_lock`, fresh `build/game_67_idle/wasm-release/index.html/js/wasm`, and `node tools/devapi/scenarios/web_visual_qa_audit.mjs` passed against `wasm-release`. This task returns to P2 backlog as the remaining optimized-without-workaround follow-up.
- 2026-06-12: Iteration 2 RC check: `cmake --preset wasm-release` passed with `EMCC_DEBUG=1`, `-O0 -DNDEBUG`, and link `-O0`. Plain `cmake --build --preset game-wasm-release` failed quickly at the final link because Emscripten could not open Ninja's transient `@CMakeFiles\game_67_idle.rsp` response file; no `.ninja_lock`, `ninja`, or `emcc` process remained. Retried with Ninja `-d keeprsp`, which completed the release build, then added that option to the wasm release build presets. After cleaning generated wasm-release outputs, the exact `cmake --build --preset game-wasm-release` command passed. Optimized wasm remains open.
- 2026-06-12: Dropped from active pipeline context: fantasy RPG web RC is complete and the repo is now focused on reusable AI pipeline hardening. Keep as historical build/release testbed debt only.
