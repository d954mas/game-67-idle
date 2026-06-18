---
id: T0005
title: Risky route choice anomalies for Backrooms Liminal
status: review
epic: E001
priority: P1
tags: []
created: 2026-06-18
updated: 2026-06-18
---

## What
Add a narrow native route-choice pressure slice to Backrooms Liminal: after the
fuse pickup, the return path presents lane anomalies where the player must pick
left or right under fear/stalker pressure. This should make the existing
shifting corridor play like a risky route decision, not only a visual effect.

## Done when

- [x] The native runtime exposes three return-path anomalies with deterministic
      safe lanes and clear wrong-choice consequences.
- [x] The HUD and corridor visuals show the active turn pressure without
      making the first screen noisy.
- [x] DevAPI state exposes anomaly stage, active safe side, correct choices, and
      wrong choices for automation.
- [x] Native build, smoke, route-choice scenario, readability zoom, product
      gate, slice hygiene, and taskboard validation pass or record explicit
      non-gameplay debt.

## Open questions

- Should later work turn these anomalies into branching geometry, or keep this
  first pass as readable lane pressure inside the current corridor?

## Log

- 2026-06-18: Started as the next narrow native slice after win/fail/replay
  polish.
- 2026-06-18: product gate PASS (desktop); review: gamedesign\projects\backrooms-liminal\reviews\product_read_gate_t0005_desktop.md; screenshot: build/captures/backrooms_t0005_route_choice.png; next: continue to the next narrow slice
- 2026-06-18: Implemented three deterministic return-path lane anomalies.
  Evidence: native build PASS, DevAPI smoke PASS,
  `build/captures/backrooms_t0005_route_choice_status.json` checks all true,
  `build/captures/backrooms_t0005_route_choice.png`,
  `build/captures/backrooms_t0005_wrong_turn.png`, readability PASS, product
  gate PASS, slice hygiene WARN only for advisory profiler failed-record debt.
