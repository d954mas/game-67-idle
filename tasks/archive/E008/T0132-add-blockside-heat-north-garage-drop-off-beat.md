---
id: T0132
title: Add Blockside Heat north garage drop-off beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_dropoff_call`, add the next smallest drop-off beat: the player drives
the claimed green coupe to a north-garage marker, the repo job records a garage
arrival/payoff, and the HUD/state expose the next story hook.

## Done when

- [x] After `repo_dropoff_call`, reaching the north garage advances a named
      drop-off/garage state.
- [x] HUD/state tells the player the car reached the garage and what is next.
- [x] Native capture/probe evidence covers call and garage drop-off states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0131 drop-off call pass. Scope is one garage marker
  and arrival/payoff only; no broad delivery loop, chase AI, traffic, economy,
  districts, weapon inventory, or mission menu in this slice.
- 2026-06-23: Implemented `repo_dropoff_garage`: after Rita's drop-off call,
  the claimed green coupe can be delivered to the north garage marker, cash
  advances to $340, HUD shows `JOB: GARAGE DONE`, and `next_job` becomes
  `repo_payout_meet`. Evidence: `cmake --build --preset native-debug`;
  `python tools/blockside-heat/capture_states.py --port 9165`;
  `tmp/blockside-heat/repo-dropoff-call-latest.png`;
  `tmp/blockside-heat/repo-dropoff-garage-latest.png`;
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0132_repo_dropoff_garage.json`
  PASS.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0132_repo_dropoff_garage.md; screenshot: tmp/blockside-heat/repo-dropoff-garage-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Native capture PASS: python tools/blockside-heat/capture_states.py --port 9165; report tmp/blockside-heat/capture-states-report.json covers repo_dropoff_call and repo_dropoff_garage. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0132_repo_dropoff_garage.json. Screenshot: tmp/blockside-heat/repo-dropoff-garage-latest.png.; next: (none)
