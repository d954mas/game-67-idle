---
id: T0126
title: Add Blockside Heat tail stop resolution beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle, npc]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `tail_pressure`, add one small stop/intercept resolution beat so the
courier tail has a readable payoff. Keep it to one stop marker or intercept cue,
state/HUD response, and native capture evidence.

## Done when

- [x] After `tail_pressure`, reaching the stop/intercept point advances a named
      resolution state.
- [x] HUD/state tells the player what happened and what the next story action is.
- [x] Native capture/probe evidence covers `tail_pressure` and the resolution beat.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0125 tail-pressure pass. Scope is one resolution
  beat only; no full chase AI, traffic system, arrest loop, inventory/economy
  expansion, district expansion, or mission menu in this slice.
- 2026-06-23: Implemented `tail_stop` story state, DevAPI action
  `game.action.tail_stop`, HUD/state `JOB: COURIER STOPPED`, cash payoff,
  wanted reduction, and native screenshot `tmp/blockside-heat/tail-stop-latest.png`.
  Strict product-read gate passes in
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T15-34-28-563Z_desktop.md; screenshot: tmp/blockside-heat/tail-stop-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement a small repo target handoff interaction after tail_stop as the next narrow playable slice.
