---
id: T0123
title: Add Blockside Heat first tail-route movement beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `courier_spotted`, add the first small tail-route movement beat. Keep it
to one visible route marker, vehicle movement/state response, and native capture
evidence.

## Done when

- [x] After `courier_spotted`, reaching the first tail marker advances a named
      tail-route state.
- [x] HUD/state clearly says what changed and what the next tail action is.
- [x] Native capture/probe evidence covers `courier_spotted` and the first tail
      route beat.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0122 courier-spotting pass. Scope is only the first
  tail-route movement beat; no broad chase AI, district expansion, weapon
  inventory, economy system, or mission menu in this slice.
- 2026-06-23: Implemented `tail_route` story state, DevAPI action
  `game.action.start_tail_route`, HUD/state `JOB: SAFE DISTANCE`, and native
  capture `tmp/blockside-heat/tail-route-latest.png`; product-read gate passed
  in `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T15-12-10-720Z_desktop.md; screenshot: tmp/blockside-heat/tail-route-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement a second tail-route turn-watch beat as the next narrow playable slice.
