---
id: T0129
title: Add Blockside Heat green coupe entry beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `green_coupe_approach`, add the next smallest repo beat: the player gets
close enough to the visible green coupe, enters/claims it, and the HUD/state
advance to the next named objective. Keep this to target-car entry feedback only.

## Done when

- [x] After `green_coupe_approach`, the player can trigger a named green-coupe
      entry/claimed state.
- [x] HUD/state says the target car is claimed and exposes the next repo action.
- [x] Native capture/probe evidence covers approach and entry states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0128 green-coupe approach pass. Scope is target-car
  entry/claim feedback only; no chase AI, traffic, weapon inventory, economy,
  mission menu, district expansion, or full theft loop in this slice.
- 2026-06-23: product gate PASS (desktop); review: gamedesign\projects\blockside-heat\reviews\product_read_gate_T0129_green_coupe_entry.md; screenshot: tmp/blockside-heat/green-coupe-entry-latest.png; next: continue to the next narrow slice
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign\projects\blockside-heat\reviews\product_read_gate_T0129_green_coupe_entry.md; screenshot: tmp/blockside-heat/green-coupe-entry-latest.png; evidence: cmake --build --preset native-debug PASS; python tools/blockside-heat/capture_states.py --port 9165 PASS; product gate PASS gamedesign/projects/blockside-heat/reviews/product_read_gate_T0129_green_coupe_entry.json; screenshot tmp/blockside-heat/green-coupe-entry-latest.png; next: (none)
