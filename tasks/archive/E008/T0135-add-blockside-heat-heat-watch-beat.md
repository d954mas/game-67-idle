---
id: T0135
title: Add Blockside Heat heat-watch beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, wanted]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_next_lead`, add the next smallest heat-watch beat: the player moves
to one watch marker, the story records a named heat-watch state, and HUD/state
expose the next playable hook.

## Done when

- [x] After `repo_next_lead`, reaching one watch marker advances a named
      heat-watch state.
- [x] HUD/state tells the player the watch started and what is next.
- [x] Native capture/probe evidence covers new-lead and heat-watch states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0134 post-payout lead pass. Scope is one
  heat-watch marker/story hook only; no broad chase AI, traffic, factions,
  economy, new district, weapon inventory, or mission menu in this slice.
- 2026-06-23: Implemented `repo_heat_watch` as the next named story state after
  `repo_next_lead`; HUD/state now report the watch beat and next hook
  `repo_meet_intercept`.
- 2026-06-23: Evidence: build PASS `cmake --build --preset native-debug`;
  native capture PASS `python tools/blockside-heat/capture_states.py --port
  9165`; report `tmp/blockside-heat/capture-states-report.json`; screenshots
  `tmp/blockside-heat/repo-next-lead-latest.png` and
  `tmp/blockside-heat/repo-heat-watch-latest.png`.
- 2026-06-23: Product/readability gate PASS:
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0135_repo_heat_watch.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0135_repo_heat_watch.md; screenshot: tmp/blockside-heat/repo-heat-watch-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Native capture PASS: python tools/blockside-heat/capture_states.py --port 9165; report tmp/blockside-heat/capture-states-report.json covers repo_next_lead and repo_heat_watch. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0135_repo_heat_watch.json. Screenshot: tmp/blockside-heat/repo-heat-watch-latest.png.; next: (none)
