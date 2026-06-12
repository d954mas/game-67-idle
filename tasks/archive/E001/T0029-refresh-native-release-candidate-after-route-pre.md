---
id: T0029
title: Refresh native release candidate after route-preview updates
status: done
epic: E001
priority: P1
tags: [release, qa, native]
created: 2026-06-12
updated: 2026-06-12
---

## What

Refresh the native release-candidate evidence after the Rusty Blade equip beat and Hunter's Ford route preview changed the first playable slice.

Out of scope: solving the known optimized WASM timeout tracked by T0025.

## Done when

- [x] native release build completes
- [x] native release asset pack builds and validates as non-empty `NPAK`
- [x] release-like DevAPI full-loop playtest passes against the native QA executable
- [x] screenshot evidence includes the Hunter's Ford route-preview state
- [x] taskboard validates after evidence is recorded

## Open questions

## Log

- 2026-06-12: Started after T0027/T0028 changed release behavior after the older T0022 pass.
- 2026-06-12: Done. Evidence: `cmake --build --preset game-native-release`, `cmake --build --preset pack-native-release`, pack `build/game_67_idle/game_67_idle.ntpack` is 12,587,324 bytes with `NPAK` magic, `cmake --build --preset game-native-qa`, `py -3.12 tools/devapi/agent_playtest.py 9150 --full-loop --exe build/game_67_idle/native-qa/game_67_idle.exe --out-dir build/captures/t0029_native_release_candidate`; route-preview screenshot `build/captures/t0029_native_release_candidate/screenshots/agent_hunter_ford_20260612_025804.png`.
