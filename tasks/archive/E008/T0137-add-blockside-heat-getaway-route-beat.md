---
id: T0137
title: Add Blockside Heat getaway-route beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, wanted]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_meet_intercept`, add the next smallest getaway-route beat: the
player reaches one route marker, the story records a named getaway-route state,
and HUD/state expose the next playable hook.

## Done when

- [x] After `repo_meet_intercept`, reaching one getaway marker advances a named
      getaway-route state.
- [x] HUD/state tells the player the route is found and what is next.
- [x] Native capture/probe evidence covers meet-intercept and getaway-route
      states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0136 meet-intercept pass. Scope is one getaway
  marker/story hook only; no broad chase AI, traffic, factions, economy, new
  district, weapon inventory, or mission menu in this slice.
- 2026-06-23: Implemented `repo_getaway_route` after `repo_meet_intercept`;
  HUD/UI now show `JOB: GETAWAY`, cash rises to `$450`, wanted drops to `1`,
  and `next_job` becomes `repo_safehouse_drop`.
- 2026-06-23: Evidence: build PASS `cmake --build --preset native-debug`;
  native capture PASS `python tools/blockside-heat/capture_states.py --port
  9165`; report `tmp/blockside-heat/capture-states-report.json`; screenshots
  `tmp/blockside-heat/repo-meet-intercept-latest.png` and
  `tmp/blockside-heat/repo-getaway-route-latest.png`.
- 2026-06-23: Product/readability gate PASS:
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0137_repo_getaway_route.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0137_repo_getaway_route.md; screenshot: tmp/blockside-heat/repo-getaway-route-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Native capture PASS: python tools/blockside-heat/capture_states.py --port 9165; report tmp/blockside-heat/capture-states-report.json covers repo_meet_intercept and repo_getaway_route. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0137_repo_getaway_route.json. Screenshot: tmp/blockside-heat/repo-getaway-route-latest.png.; next: (none)
