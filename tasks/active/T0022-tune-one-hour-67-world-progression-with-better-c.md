---
id: T0022
title: Tune one-hour 67 World progression with better crates
status: review
epic: ""
priority: P1
tags: [balance, progression, release, native]
created: 2026-06-12
updated: 2026-06-12
---

## What

Tune ordinary-play 67 World progression so a player can reach the 30th
release-track variant through visible gameplay systems rather than only
DevAPI-forced state edits. Add a repeatable Better Crate upgrade and a
one-hour simulator gate.

## Done when

- [x] Runtime state has persistent Better Crate progression.
- [x] Native gameplay exposes a visible upgrade path after Faster Spawn.
- [x] One-hour simulator reaches Cosmic 67 through ordinary spawn/merge/passive
      play without direct state forcing.
- [x] Existing first-loop and 30-variant DevAPI scenarios still pass.
- [x] Balance/UI docs and live status name the new release gate evidence.

## Open questions

None.

## Log

- 2026-06-12: Started from release gate gap: pure Tiny-only doubling cannot
  reach 30 variants in an hour. Added Better Crate as the first late-session
  acceleration system and a simulator target.
- 2026-06-12: Evidence passed: `cmake --build --preset native-debug`;
  `py -3.12 tools/balance/simulate_67_world.py` (Cosmic at 53.92m inside
  50-60m window); `py -3.12 tools/devapi/scenarios/first_67_loop.py 9201 build/captures/scenarios/first_67_loop_better_crate_v1.png`;
  `py -3.12 tools/devapi/scenarios/extended_67_progression.py 9202 build/captures/scenarios/extended_67_progression_better_crate_v1.png`;
  `py -3.12 tools/devapi/scenarios/better_crate_progression.py 9203 build/captures/scenarios/better_crate_progression_v2.png`;
  pixel health passed for all three screenshots.
