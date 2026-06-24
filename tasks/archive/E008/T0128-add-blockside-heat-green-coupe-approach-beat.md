---
id: T0128
title: Add Blockside Heat green coupe approach beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `target_handoff`, add one small green-coupe approach beat so the named
repo target becomes a visible next objective. Keep it to one target marker,
state/HUD response, and native capture evidence.

## Done when

- [x] After `target_handoff`, reaching the green-coupe approach point advances a
      named approach state.
- [x] HUD/state tells the player the target car is found and what to do next.
- [x] Native capture/probe evidence covers `target_handoff` and the approach beat.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0127 target-handoff pass. Scope is one approach
  beat only; no full repo theft, traffic system, inventory/economy expansion,
  district expansion, or mission menu in this slice.
- 2026-06-23: product gate PASS (desktop); review: gamedesign\projects\blockside-heat\reviews\product_read_gate_T0128_green_coupe_approach.md; screenshot: tmp/blockside-heat/green-coupe-approach-latest.png; next: continue to the next narrow slice
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign\projects\blockside-heat\reviews\product_read_gate_T0128_green_coupe_approach.md; screenshot: tmp/blockside-heat/green-coupe-approach-latest.png; evidence: cmake --build --preset native-debug PASS; python tools/blockside-heat/capture_states.py --port 9165 PASS; product gate PASS gamedesign/projects/blockside-heat/reviews/product_read_gate_T0128_green_coupe_approach.json; screenshot tmp/blockside-heat/green-coupe-approach-latest.png; next: (none)
