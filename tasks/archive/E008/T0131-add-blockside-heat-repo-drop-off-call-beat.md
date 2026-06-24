---
id: T0131
title: Add Blockside Heat repo drop-off call beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `green_coupe_escape`, add the next smallest story beat: Rita calls with the
drop-off instruction, the HUD/state advance to a named drop-off lead, and the
next action is visible. Keep this to one call/lead response only.

## Done when

- [x] After `green_coupe_escape`, triggering the call advances a named drop-off
      lead state.
- [x] HUD/state tells the player where to take the car next.
- [x] Native capture/probe evidence covers escape and drop-off call states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0130 green-coupe escape pass. Scope is one Rita
  call/drop-off lead only; no full delivery loop, broad chase AI, traffic,
  weapon inventory, economy, districts, or mission menu in this slice.
- 2026-06-23: product gate PASS (desktop); review: gamedesign\projects\blockside-heat\reviews\product_read_gate_T0131_repo_dropoff_call.md; screenshot: tmp/blockside-heat/repo-dropoff-call-latest.png; next: continue to the next narrow slice
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign\projects\blockside-heat\reviews\product_read_gate_T0131_repo_dropoff_call.md; screenshot: tmp/blockside-heat/repo-dropoff-call-latest.png; evidence: cmake --build --preset native-debug PASS; python tools/blockside-heat/capture_states.py --port 9165 PASS; product gate PASS gamedesign/projects/blockside-heat/reviews/product_read_gate_T0131_repo_dropoff_call.json; screenshot tmp/blockside-heat/repo-dropoff-call-latest.png; next: (none)
