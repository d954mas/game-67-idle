---
id: T0133
title: Add Blockside Heat repo payout meet beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_dropoff_garage`, add the next smallest payout beat: the player meets
Rita at one contact marker, the repo payout state is recorded, and HUD/state
expose the next story hook.

## Done when

- [x] After `repo_dropoff_garage`, reaching Rita advances a named payout/meet
      state.
- [x] HUD/state tells the player the garage handoff paid out and what is next.
- [x] Native capture/probe evidence covers garage done and payout meet states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0132 garage drop-off pass. Scope is one Rita
  contact marker and payout hook only; no broad mission menu, economy, factions,
  traffic, chase AI, new district, or weapon inventory in this slice.
- 2026-06-23: Implemented `repo_payout_meet`: after `repo_dropoff_garage`, Rita
  can be met at the contact marker, cash advances to $400, HUD shows
  `JOB: PAYOUT DONE`, and `next_job` becomes `repo_next_lead`. Evidence:
  `cmake --build --preset native-debug`; `python tools/blockside-heat/capture_states.py --port 9165`;
  `tmp/blockside-heat/repo-dropoff-garage-latest.png`;
  `tmp/blockside-heat/repo-payout-meet-latest.png`;
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0133_repo_payout_meet.json`
  PASS.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0133_repo_payout_meet.md; screenshot: tmp/blockside-heat/repo-payout-meet-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Native capture PASS: python tools/blockside-heat/capture_states.py --port 9165; report tmp/blockside-heat/capture-states-report.json covers repo_dropoff_garage and repo_payout_meet. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0133_repo_payout_meet.json. Screenshot: tmp/blockside-heat/repo-payout-meet-latest.png.; next: (none)
