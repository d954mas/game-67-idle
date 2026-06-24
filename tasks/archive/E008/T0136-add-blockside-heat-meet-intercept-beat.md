---
id: T0136
title: Add Blockside Heat meet-intercept beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, wanted]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_heat_watch`, add the next smallest meet-intercept beat: the player
reaches one intercept marker, the story records a named meet-intercept state,
and HUD/state expose the next playable hook.

## Done when

- [x] After `repo_heat_watch`, reaching one intercept marker advances a named
      meet-intercept state.
- [x] HUD/state tells the player the meet is intercepted and what is next.
- [x] Native capture/probe evidence covers heat-watch and meet-intercept states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0135 heat-watch pass. Scope is one intercept
  marker/story hook only; no broad chase AI, traffic, factions, economy, new
  district, weapon inventory, or mission menu in this slice.
- 2026-06-23: Implemented `repo_meet_intercept` after `repo_heat_watch`; HUD/UI
  now show `JOB: MEET HIT`, cash rises to `$430`, wanted pressure stays at `2`,
  and `next_job` becomes `repo_getaway_route`.
- 2026-06-23: Evidence: build PASS `cmake --build --preset native-debug`;
  native capture PASS `python tools/blockside-heat/capture_states.py --port
  9165`; report `tmp/blockside-heat/capture-states-report.json`; screenshots
  `tmp/blockside-heat/repo-heat-watch-latest.png` and
  `tmp/blockside-heat/repo-meet-intercept-latest.png`.
- 2026-06-23: Product/readability gate PASS:
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0136_repo_meet_intercept.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0136_repo_meet_intercept.md; screenshot: tmp/blockside-heat/repo-meet-intercept-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Native capture PASS: python tools/blockside-heat/capture_states.py --port 9165; report tmp/blockside-heat/capture-states-report.json covers repo_heat_watch and repo_meet_intercept. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0136_repo_meet_intercept.json. Screenshot: tmp/blockside-heat/repo-meet-intercept-latest.png.; next: (none)
