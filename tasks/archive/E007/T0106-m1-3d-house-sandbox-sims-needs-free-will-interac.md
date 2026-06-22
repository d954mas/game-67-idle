---
id: T0106
title: "M1: 3D house sandbox - sims, needs, free will, interactions, social, day-night, work, build-lite"
status: done
epic: E007
priority: P1
tags: [little-lives, native-first, 3d, milestone-1]
created: 2026-06-22
updated: 2026-06-22
---

## What

First playable native 3D slice of the full Sims-like: one house lot, several
Sims with decaying needs, free will + player commands, furniture interactions,
Sim-to-Sim socializing, day/night clock, going to work for money, and a
lightweight Build/Buy mode. Shape-renderer art is acknowledged debug debt
(real meshes -> T0108; readable font text -> T0107).

## Done when

- [x] 3D house lot renders (floor, lawn, cutaway walls) with distinct furniture
      (bed, fridge, shower, toilet, sofa, computer, work door).
- [x] 3 Sims with 6 needs (energy, hunger, hygiene, bladder, fun, social) that
      decay over time (DevAPI: hunger 65.6 -> 63.7 while idle).
- [x] Free will: idle Sims autonomously seek the object for their lowest need
      (observed Alex -> sleep, Bella -> social with no command).
- [x] Player command: select a Sim (TAB/click/portrait) and command a need
      (keys 1-6 / bottom buttons / click object); need refills while using
      (DevAPI: hunger 63.7 -> 93.1 after eat command).
- [x] Sim-to-Sim social interaction raises social/fun.
- [x] Day/night clock advances; scene daylight + sky shift with time.
- [x] Go to work: Sim leaves the lot (at_work=true) and earns simoleons.
- [x] Build/Buy-lite: B toggles build, palette + grid cursor, place/remove
      furniture spending/refunding simoleons (mode toggle verified).
- [x] Orbit camera (right-drag / arrows), pan (WASD), zoom (wheel).
- [x] Readable color-coded HUD: per-need bars, mood billboards, portraits,
      money pips, day bar, mode/work buttons.
- [x] Native build + run + DevAPI smoke + screenshot proof captured.

## Open questions

- Sims read small/clustered toward the back wall (furniture is wall-aligned).
  Spread furniture + tune default camera framing in a polish pass? (deferred)

## Log

- Schema redefined (`state/game_state.schema.json`): GameMode/SimAction enums +
  global scalars (mode, selected_sim, clock, wallet, camera, settings); rich
  per-Sim/object data lives in C and is exposed via custom DevAPI. Migration
  `state/migrations/v0_to_v1.c` reduced to a no-op (fresh game, no legacy saves).
  `tools/state_codegen/generate_state.py` REQUIRED_FIELDS trimmed to the
  template-mandated infra fields.
- Implemented the whole slice in `src/clean_seed_main.c` (engine is code-first;
  single-file matches the engine examples). Added a durable
  `game.capture.framebuffer` DevAPI endpoint (glReadPixels -> PPM) for
  repeatable screenshots (prior games' capture was removed by the reset).
- Build: `cmake --build build/_cmake/native-debug --target game_seed` (clean).
- Run: `build/game_seed/native-debug/game_seed.exe --devapi 9123 --window-size 1280x720`.
- Smoke: `python tmp/ll_smoke.py` — all 4 acceptance checks pass (decay, eat,
  work, build). Hero shot: `python tmp/ll_hero.py`.
- Evidence: `gamedesign/projects/little-lives/reviews/first_slice_native.png`
  (live mode: 3 Sims at bed/fridge/shower, need-bar HUD, money/day, portraits).
- DevAPI surface: game.state (sims[]/objects[]/clock/mode), game.reset_playtest,
  game.action.command {sim,need|work}, game.action.select {sim},
  game.capture.framebuffer {output}.
