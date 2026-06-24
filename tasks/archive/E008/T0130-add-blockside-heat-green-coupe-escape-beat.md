---
id: T0130
title: Add Blockside Heat green coupe escape beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `green_coupe_claimed`, add the next smallest escape beat: the player drives
the claimed green coupe to a nearby lose-heat point, wanted pressure resolves,
and the HUD/state expose the next story hook. Keep this to one escape marker and
one payoff response.

## Done when

- [x] After `green_coupe_claimed`, reaching an escape point advances a named
      lose-heat/escape state.
- [x] HUD/state says heat is lost or reduced and exposes the next story action.
- [x] Native capture/probe evidence covers claimed and escape states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0129 green-coupe entry pass. Scope is one escape
  marker and payoff only; no broad chase AI, traffic, weapon inventory, economy,
  mission menu, district expansion, or full repo delivery loop in this slice.
- 2026-06-23: product gate PASS (desktop); review: gamedesign\projects\blockside-heat\reviews\product_read_gate_T0130_green_coupe_escape.md; screenshot: tmp/blockside-heat/green-coupe-escape-latest.png; next: continue to the next narrow slice
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign\projects\blockside-heat\reviews\product_read_gate_T0130_green_coupe_escape.md; screenshot: tmp/blockside-heat/green-coupe-escape-latest.png; evidence: cmake --build --preset native-debug PASS; python tools/blockside-heat/capture_states.py --port 9165 PASS; product gate PASS gamedesign/projects/blockside-heat/reviews/product_read_gate_T0130_green_coupe_escape.json; screenshot tmp/blockside-heat/green-coupe-escape-latest.png; next: (none)
