---
id: T0138
title: Add Blockside Heat safehouse-drop beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, wanted]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_getaway_route`, add the next smallest safehouse-drop beat: the
player reaches one safehouse marker, the story records a named safehouse-drop
state, and HUD/state expose the next playable hook.

## Done when

- [x] After `repo_getaway_route`, reaching one safehouse marker advances a named
      safehouse-drop state.
- [x] HUD/state tells the player the safehouse drop is done and what is next.
- [x] Native capture/probe evidence covers getaway-route and safehouse-drop
      states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0137 getaway-route pass. Scope is one safehouse
  marker/story hook only; no broad chase AI, traffic, factions, economy, new
  district, weapon inventory, or mission menu in this slice.
- 2026-06-23: Implemented `repo_safehouse_drop` after `repo_getaway_route`;
  HUD/UI now show `JOB: SAFEHOUSE`, cash rises to `$485`, wanted clears to `0`,
  and `next_job` becomes `repo_final_call`.
- 2026-06-23: Evidence: build PASS `cmake --build --preset native-debug`;
  native capture PASS `python tools/blockside-heat/capture_states.py --port
  9165`; report `tmp/blockside-heat/capture-states-report.json`; screenshots
  `tmp/blockside-heat/repo-getaway-route-latest.png` and
  `tmp/blockside-heat/repo-safehouse-drop-latest.png`.
- 2026-06-23: Product/readability gate PASS:
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0138_repo_safehouse_drop.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0138_repo_safehouse_drop.md; screenshot: tmp/blockside-heat/repo-safehouse-drop-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Native capture PASS: python tools/blockside-heat/capture_states.py --port 9165; report tmp/blockside-heat/capture-states-report.json covers repo_getaway_route and repo_safehouse_drop. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0138_repo_safehouse_drop.json. Screenshot: tmp/blockside-heat/repo-safehouse-drop-latest.png.; next: (none)
