---
id: T0124
title: Add Blockside Heat tail-route turn-watch beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `tail_route`, add a second small tailing beat where the player follows the
first turn marker. Keep it to one visible route marker, state/HUD response, and
native capture evidence.

## Done when

- [x] After `tail_route`, reaching the turn-watch marker advances a named
      tail-turn state.
- [x] HUD/state clearly says what changed and what the next tail action is.
- [x] Native capture/probe evidence covers `tail_route` and the turn-watch beat.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0123 first tail-route pass. Scope is only one
  turn-watch tail marker; no broad chase AI, district expansion, weapon
  inventory, economy system, or mission menu in this slice.
- 2026-06-23: Implemented `tail_turn` story state, DevAPI action
  `game.action.watch_tail_turn`, HUD/state `JOB: TURN WATCH`, and native
  screenshot `tmp/blockside-heat/tail-turn-latest.png`. Strict product-read
  gate passes in `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T15-20-48-652Z_desktop.md; screenshot: tmp/blockside-heat/tail-turn-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement a small intercept or spacing-failure beat after tail_turn as the next narrow playable slice.
