---
id: T0001
title: First native playable slice for Backrooms Liminal
status: review
epic: E001
priority: P1
tags: [prototype, backrooms-liminal, native-first]
created: 2026-06-18
updated: 2026-06-18
---

## What

Build the first native playable slice for `Backrooms Liminal` after the Stage 0 startup gate is ready.

## Done when

- [x] `gamedesign/projects/backrooms-liminal/gdd.md` names the first playable loop and player-readable goal.
- [x] `gamedesign/projects/backrooms-liminal/data/core_loop.json` describes the
      player verbs, rules, feedback, risk, goals, replay reason, and reference
      grounding without assuming hands-off progression, away-time rewards, or
      reset-meta loops.
- [x] `gamedesign/projects/backrooms-liminal/visual/live_state_acceptance_matrix.json`
      is reviewed for this game's HUD, primary CTA, feedback, modal,
      blocked/affordable, and transient stress states.
- [x] A fake shot or visual target exists before runtime polish starts.
- [x] A 5-line visual session contract exists: goal, non-goal, proof, stop
      condition, likely files.
- [x] Current native screenshot or capture plan is compared against the fake
      shot/target in a mismatch list before visual code expands.
- [x] Native PC build/run command is identified and captured in the task log.
- [x] First native screenshot/product-read proof is captured before expanding content.

## Open questions

- No user clarification needed for iteration 0. The named target is Backrooms
  Level-0-like liminal horror, interpreted through the local quick reference
  digest in `gamedesign/projects/backrooms-liminal/gdd.md`.

## Log

- 2026-06-18: Stage 0 kickoff created `backrooms-liminal` and selected native
  PC harness only. Profiling scope set with `node tools/ai.mjs start
  backrooms-liminal 0`.
- 2026-06-18: Filled first-slice GDD, quick Backrooms reference digest,
  visual target, and `data/core_loop.json`. Runtime scope is one corridor,
  fuse pickup, return-to-exit, fear/battery pressure, screenshots before
  content expansion.
- 2026-06-18: product gate PASS (desktop); review: gamedesign/projects/backrooms-liminal/reviews/product_read_gate_2026-06-18T15-47-04-499Z_desktop.md; screenshot: build/captures/backrooms_first_screen.png; next: continue to the next narrow slice
- 2026-06-18: Implemented native 3D first-person slice in `src/clean_seed_main.c`:
  textured yellow corridor, fluorescent lighting/flicker, flashlight, fog,
  fear/battery pressure, fuse pickup, return-to-exit objective, and silhouette
  stress state. Validation: `cmake --build --preset native-debug --target
  game_seed`, `py -3.12 tools/devapi/smoke.py`, `py -3.12
  tools/devapi/ui_readability.py build/captures/backrooms_first_screen.png`,
  strict product gate PASS, and slice hygiene WARN only for unusable profiler
  review evidence. Evidence: `build/captures/backrooms_first_screen.png`,
  `build/captures/backrooms_after_fuse.png`,
  `build/captures/backrooms_first_screen_uizoom.png`,
  `gamedesign/projects/backrooms-liminal/reviews/product_read_gate_latest.json`.
