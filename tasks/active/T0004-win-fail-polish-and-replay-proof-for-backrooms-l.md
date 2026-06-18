---
id: T0004
title: Win fail polish and replay proof for Backrooms Liminal
status: review
epic: E001
priority: P1
tags: [prototype, backrooms-liminal, native-first, polish, replay]
created: 2026-06-18
updated: 2026-06-18
---

## What

Make the Backrooms slice read as a complete playable run by adding clear win
and fail end-state overlays, run stats, restart/replay input, and a focused
DevAPI scenario that proves both outcomes. Keep this as polish around the
existing corridor loop, not new content or a broader maze.

## Done when

- [x] Escape and caught outcomes display readable center overlays with outcome,
      run time, fear, battery, and the next action.
- [x] Pressing E/Enter from win or caught starts a fresh run, and DevAPI
      `game.action.use` follows the same restart path.
- [x] DevAPI state exposes outcome/run stats/restart readiness for automation.
- [x] Native build, DevAPI smoke, and a focused win/fail/replay scenario pass.
- [x] Fresh screenshots/readability/product gate cover win/fail/replay states
      or record any explicit debt.

## Open questions

- None for this iteration. This task improves completion/replay clarity for
  the existing native slice only.

## Log

- 2026-06-18: Started as the next narrow slice after T0003 review. Runtime
  harness remains native PC (`game_seed.exe --devapi`); broader route content
  and web/mobile are out of scope.
- 2026-06-18: product gate PASS (desktop); review: gamedesign/projects/backrooms-liminal/reviews/product_read_gate_2026-06-18T16-14-31-286Z_desktop.md; screenshot: build/captures/backrooms_t0004_win_overlay.png; next: continue to the next narrow slice
- 2026-06-18: Added readable win/fail overlays, run stats, and E/Enter replay
  from both escaped and caught states. DevAPI state now exposes `outcome`,
  `run_time`, `last_run_time`, `last_fear`, `last_battery`, and
  `can_restart`; `game.action.use` restarts from end states. Validation:
  `cmake --build --preset native-debug --target game_seed`, `py -3.12
  tools/devapi/smoke.py`, focused replay scenario
  `tmp/capture_backrooms_t0004_replay.py` with all restart checks true,
  readability PASS for win/fail overlays, strict product gate PASS, and slice
  hygiene WARN only for advisory profiler parsing/global old failure. Evidence:
  `build/captures/backrooms_t0004_win_overlay.png`,
  `build/captures/backrooms_t0004_fail_overlay.png`,
  `build/captures/backrooms_t0004_replay_status.json`,
  `build/captures/backrooms_t0004_slice_hygiene.md`.
