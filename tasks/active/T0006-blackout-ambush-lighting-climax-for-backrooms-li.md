---
id: T0006
title: Blackout ambush lighting climax for Backrooms Liminal
status: review
epic: E001
priority: P1
tags: []
created: 2026-06-18
updated: 2026-06-18
---

## What
Add a narrow horror/lighting climax to Backrooms Liminal: when the player makes
a wrong route-choice on the return path, the corridor briefly blacks out, the
stalker surges closer, emergency color bleeds into the scene, and the HUD gives
a clear run warning. This should make wrong choices feel scary and physical,
not only a stat penalty.

## Done when

- [x] Wrong route-choice triggers a timed blackout ambush with stronger
      lighting/shadow change and closer stalker pressure.
- [x] Correct route-choice produces a readable relief pulse so the player
      feels the safe lane worked.
- [x] DevAPI exposes blackout/ambush/relief state for automation.
- [x] Native build, smoke, blackout scenario, screenshot proof, readability,
      product gate, slice hygiene, and taskboard validation pass or record
      explicit non-gameplay debt.

## Open questions

- Should later work turn blackout ambushes into a timed chase segment with
  actual sprint/stamina, or keep this pass as a short pressure spike?

## Log

- 2026-06-18: Started after T0005 route-choice anomalies to make wrong choices
  more frightening through light/shadow/audio pressure.
- 2026-06-18: product gate PASS (desktop); review: gamedesign\projects\backrooms-liminal\reviews\product_read_gate_t0006_desktop.md; screenshot: build/captures/backrooms_t0006_blackout_ambush.png; next: continue to the next narrow slice
- 2026-06-18: Implemented blackout ambush and safe-turn relief. Evidence:
  native build PASS, DevAPI smoke PASS,
  `build/captures/backrooms_t0006_blackout_status.json` checks all true,
  `build/captures/backrooms_t0006_blackout_ambush.png`,
  `build/captures/backrooms_t0006_safe_turn_relief.png`, readability PASS,
  product gate PASS, slice hygiene WARN only for advisory profiler/global
  historical debt.
