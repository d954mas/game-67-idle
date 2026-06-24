---
id: T0112
title: First native playable slice for Blockside Heat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, native-first]
created: 2026-06-23
updated: 2026-06-23
---

## What

Build the first native playable slice for `Blockside Heat` after the Stage 0 startup gate is ready.

## Done when

- [x] `gamedesign/projects/blockside-heat/gdd.md` names the first playable loop and player-readable goal.
- [x] `gamedesign/projects/blockside-heat/data/core_loop.json` describes the
      player verbs, rules, feedback, risk, goals, replay reason, and reference
      grounding without assuming hands-off progression, away-time rewards, or
      reset-meta loops.
- [x] `gamedesign/projects/blockside-heat/visual/live_state_acceptance_matrix.json`
      is reviewed for this game's HUD, primary CTA, feedback, modal,
      blocked/affordable, and transient stress states.
- [x] A fake shot or visual target exists before runtime polish starts.
- [x] A 5-line visual session contract exists: goal, non-goal, proof, stop
      condition, likely files.
- [x] Current native screenshot or capture plan is compared against the fake
      shot/target in a mismatch list before visual code expands.
- [x] Native PC build/run command is identified and captured in the task log.
- [x] First native screenshot/product-read proof is captured before expanding content.
- [ ] Strict product gate is pass or lead explicitly accepts the remaining
      visual/action-direction debt.

## Open questions

- No named reference deconstruction is active. The first visual target is the
  original fake shot at
  `gamedesign/projects/blockside-heat/visual/targets/blockside-heat-first-slice-target.png`.

## Log

- 2026-06-23: Created Stage 0 design startup for `Blockside Heat`: concept,
  GDD first slice, core loop, UI flow, combat contract, asset manifest, visual
  target, visual gate, and native build/run capture plan. Build command:
  `cmake --build --preset native-debug` after `cmake --preset native-debug` if
  configure is missing. Runtime capture plan:
  `build/_cmake/native-debug/game_seed.exe --devapi <port> --window-size 1280x720`
  with screenshot output under `tmp/blockside-heat/`.
- 2026-06-23: Implemented first native runtime slice plumbing: reusable GLB
  assets pulled into project-local `assets/`, `blockside_heat.ntpack` builder,
  real mesh renderer path, packed engine font HUD text, player/car/package/job
  state, pursuer pressure, starter stun action, and DevAPI endpoints. Build
  passes with `cmake --build --preset native-debug`.
- 2026-06-23: DevAPI smoke passed in one shell session: `game.state`,
  `game.action.pickup_package`, `game.action.complete_job`, and `ui.tree`.
  Evidence saved in `tmp/blockside-heat/*.json` and summarized in
  `gamedesign/projects/blockside-heat/reviews/iteration_001_runtime_smoke.md`.
  Screenshot/product-read proof remains open because both native capture tools
  failed with invalid-handle/BitBlt errors in this environment.
- 2026-06-23: Added dev-only `game.capture.framebuffer` endpoint and captured
  native screenshots through `tools/devapi/devapi_client.py`. First product
  gate failed on floating assets/no ground; after grouped multi-primitive
  rendering and authored `blockside_city_base` mesh, the latest gate is
  `review`, not pass. Evidence:
  `tmp/blockside-heat/first-native-screenshot-iter4.png`,
  `tmp/blockside-heat/pickup-stress-screenshot.png`,
  `tmp/blockside-heat/job-complete-screenshot.png`,
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: Added `tools/blockside-heat/capture_states.py`; it captures
  first screen, pickup/stress, reward/lock, state JSON, and `ui.tree` into
  `tmp/blockside-heat/capture-states-report.json`. Latest gate remains
  `review`; next fix is a visible package/route affordance.
- 2026-06-23: Split framebuffer capture, CLI config, and asset helper code out
  of `src/clean_seed_main.c` so the active runtime is 898 lines. Validation
  passed: `cmake --build --preset
  native-debug`, `python tools/blockside-heat/capture_states.py --port 9153`,
  `node tools/visual_invariant_guard.mjs`, the task metadata check, and
  `node tools/ai.mjs validate --review`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T13-56-24-312Z_desktop.md; screenshot: tmp/blockside-heat/first-native-screenshot-latest.png; evidence: Strict product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json; native capture report: tmp/blockside-heat/capture-states-report.json; build: cmake --build --preset native-debug.; next: Expand the playable loop with better vehicle handling or a second street job; keep strict screenshots for each new system.
