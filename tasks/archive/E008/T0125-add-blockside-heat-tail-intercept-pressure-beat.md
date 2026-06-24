---
id: T0125
title: Add Blockside Heat tail intercept pressure beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle, npc]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `tail_turn`, add one small intercept/pressure beat that makes tailing feel
riskier. Keep it to one readable marker or failure-pressure cue, state/HUD
response, and native capture evidence.

## Done when

- [x] After `tail_turn`, reaching or mishandling the next tail point advances a
      named intercept/pressure state.
- [x] HUD/state tells the player what changed and what the next action is.
- [x] Native capture/probe evidence covers `tail_turn` and the new pressure beat.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0124 turn-watch pass. Scope is one pressure beat
  only; no broad chase AI, full traffic system, inventory/economy expansion,
  district expansion, or mission menu in this slice.
- 2026-06-23: Implemented `tail_pressure` story state, DevAPI action
  `game.action.tail_pressure`, HUD/state `JOB: TAIL PRESSURE`, WANTED 2
  pressure escalation, and native screenshot
  `tmp/blockside-heat/tail-pressure-latest.png`. Strict product-read gate
  passes in `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T15-27-46-969Z_desktop.md; screenshot: tmp/blockside-heat/tail-pressure-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement a small stop/intercept resolution beat after tail_pressure as the next narrow playable slice.
