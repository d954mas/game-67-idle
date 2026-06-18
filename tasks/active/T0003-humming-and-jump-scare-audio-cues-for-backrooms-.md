---
id: T0003
title: Humming and jump-scare audio cues for Backrooms Liminal
status: review
epic: E001
priority: P1
tags: [prototype, backrooms-liminal, native-first, horror, audio]
created: 2026-06-18
updated: 2026-06-18
---

## What

Add a native audio layer to the Backrooms slice using the existing generated
PCM `game_audio` module: fuse hum/notify cues, flashlight clicks, stalker
pressure stingers, caught/win cues, and DevAPI-visible audio status. Keep this
as cue-based horror feedback, not a broad music system or asset pipeline.

## Done when

- [x] `src/clean_seed_main.c` initializes, updates, and shuts down
      `game_audio` in the native runtime.
- [x] Core player actions and horror state changes play distinct cues:
      flashlight toggle, fuse pickup, stalker pressure threshold, caught, and
      escape.
- [x] DevAPI exposes audio status/play counts so smoke or a focused capture can
      prove cues fired without relying on the agent hearing local audio.
- [x] Native build and DevAPI smoke pass.
- [x] Fresh native screenshots/readability/product gate still pass after the
      HUD/audio status changes.

## Open questions

- None for this iteration. Use existing generated PCM cue infrastructure; do
  not add external audio assets or engine changes.

## Log

- 2026-06-18: Started as the next narrow slice after T0002 review. Runtime
  harness remains native PC (`game_seed.exe --devapi`); web/mobile and external
  audio assets are out of scope.
- 2026-06-18: product gate PASS (desktop); review: gamedesign/projects/backrooms-liminal/reviews/product_read_gate_2026-06-18T16-06-56-999Z_desktop.md; screenshot: build/captures/backrooms_t0003_audio_threat.png; next: continue to the next narrow slice
- 2026-06-18: Integrated generated PCM horror cues through `game_audio`:
  flashlight click, fuse hum loop, fuse pickup, stalker stinger, caught, and
  escape. Added `game.audio.status` DevAPI endpoint and smoke coverage for cue
  count increments. Focused audio scenario evidence:
  `build/captures/backrooms_t0003_audio_status.json` shows cue counts
  `flashlight=2`, `fuse_hum=6`, `fuse_pickup=1`, `stalker=3`, `caught=1`,
  `escape=1`. Validation: `cmake --build --preset native-debug --target
  game_seed`, `py -3.12 tools/devapi/smoke.py`, readability PASS for
  `build/captures/backrooms_t0003_first_screen.png` and
  `build/captures/backrooms_t0003_audio_threat.png`, strict product gate PASS,
  and slice hygiene WARN only for advisory profiler parsing/global old failure.
