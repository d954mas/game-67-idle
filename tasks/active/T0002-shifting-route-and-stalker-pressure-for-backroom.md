---
id: T0002
title: Shifting route and stalker pressure for Backrooms Liminal
status: review
epic: E001
priority: P1
tags: [prototype, backrooms-liminal, native-first, horror]
created: 2026-06-18
updated: 2026-06-18
---

## What

Make the current Backrooms slice more interesting and scary by adding one narrow
return-path pressure layer: after the fuse, the corridor visually shifts, false
side exits/route cues appear, and the stalker threat becomes a readable gameplay
pressure state. Keep it native PC only and do not expand into full procedural
maze generation.

## Done when

- [x] Runtime exposes a player-readable route instability/threat state after
      fuse pickup without requiring a full maze or new engine APIs.
- [x] HUD tells the player what changed and what to do next without hiding fear,
      battery, fuse, or exit status.
- [x] DevAPI state/smoke can observe the new pressure state or progress marker.
- [x] Native build and DevAPI smoke pass.
- [x] Fresh native screenshots show the first screen and the post-fuse shifting
      threat state.
- [x] UI readability zoom and strict product gate pass or record any debt
      explicitly.

## Open questions

- None for this iteration. Scope is the smallest next step named by
  `tasks/STATUS.md`: route uncertainty plus enemy pressure, not broad content.

## Log

- 2026-06-18: Started as the next narrow slice after T0001 review. Runtime
  harness remains native PC (`game_seed.exe --devapi`); web/mobile is out of
  scope.
- 2026-06-18: product gate PASS (desktop); review: gamedesign/projects/backrooms-liminal/reviews/product_read_gate_2026-06-18T15-56-40-961Z_desktop.md; screenshot: build/captures/backrooms_t0002_shift_threat.png; next: continue to the next narrow slice
- 2026-06-18: Implemented route instability and stalker pressure in
  `src/clean_seed_main.c`: post-fuse `route_shift`, `stalker_pressure`,
  `threat_visible`, false green exits, screen warping, humanoid stalker
  silhouette, ROUTE/THREAT HUD line, and DevAPI state/debug support.
  Validation: `cmake --build --preset native-debug --target game_seed`,
  `py -3.12 tools/devapi/smoke.py`, readability PASS for
  `build/captures/backrooms_t0002_first_screen.png` and
  `build/captures/backrooms_t0002_shift_threat.png`, strict product gate PASS,
  and slice hygiene WARN only for advisory profiler parsing/global old failures.
  Evidence: `build/captures/backrooms_t0002_first_screen.png`,
  `build/captures/backrooms_t0002_shift_threat.png`,
  `build/captures/backrooms_t0002_shift_threat_uizoom.png`,
  `gamedesign/projects/backrooms-liminal/reviews/product_read_gate_latest.json`,
  `build/captures/backrooms_t0002_slice_hygiene.md`.
