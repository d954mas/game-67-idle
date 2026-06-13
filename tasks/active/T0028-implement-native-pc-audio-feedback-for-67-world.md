---
id: T0028
title: Implement native PC audio feedback for 67 World
status: review
epic: ""
priority: P1
tags: [audio, native, release, ux, validation]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add minimal release-track native PC sound feedback for 67 World without editing
the engine submodule. The audio path should be game-local, deterministic enough
for automation to verify, and tied to core child-test actions: spawn, merge,
upgrade, recycle/stuck recovery, and blocked/error feedback.

## Done when

- [x] Native PC builds link a game-local audio implementation; non-Windows/web
      builds stay safe through no-op stubs.
- [x] Spawn, merge/new 67, speed/better-crate upgrades, recycle, and blocked
      feedback trigger distinct SFX respecting `settings.master_volume` and
      `settings.sfx_volume`.
- [x] DevAPI exposes audio status/evidence so automated review can verify audio
      is implemented without listening manually.
- [x] Child-test readiness report no longer lists missing audio as a release
      blocker and records audio implementation/play counts.
- [x] Native release package is rebuilt after the audio change.
- [x] Task/status files point to the new evidence and remaining blockers.

## Open questions

None. Use a game-local native PC audio shim first; keep broader engine audio as
future debt only if needed.

## Log

- 2026-06-13: Started from `T0027` release blocker. Scope: add Windows/native
  game-local SFX with no engine submodule edits and keep other platforms safe
  with no-op stubs.
- 2026-06-13: Added game-local native Windows SFX in `src/game_audio.*`, wired
  spawn/merge/upgrade/recycle/blocked cues from UI and DevAPI actions, and
  exposed `game.audio.status`. DevAPI runs keep the physical device disabled to
  avoid headless hangs, while release/non-DevAPI runtime enables the WinMM
  device path.
- 2026-06-13: Evidence passed: `py -3.12 -m py_compile tools/devapi/scenarios/child_test_readiness.py`;
  `cmake --build --preset native-debug`; `cmake --build --preset native-release`;
  `node tools/package_native_release.mjs` produced
  `build/release/67-world-pc/67-world/67-world.exe` (713216 bytes);
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9272 build/reports/child_test_readiness_v7_audio.json build/captures/scenarios/child_test_readiness_v7_audio`
  passed with audio counts `spawn=3`, `merge=1`, `upgrade=2`, `recycle=1`,
  `blocked=1` in desktop review and no missing-audio release blocker.
- 2026-06-13: Pixel health passed for all five v7 child-test screenshots:
  desktop first loop, desktop upgrade, desktop stuck, portrait first loop, and
  portrait stuck.
