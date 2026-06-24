---
id: T0122
title: Add Blockside Heat courier spotting follow-up
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, npc]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `market_watch`, let the player spot the courier as one small NPC/story
follow-up. Keep it focused on a visible courier cue, state/HUD response, and
native capture evidence.

## Done when

- [x] After `market_watch`, reaching or interacting with the courier cue advances
      a named courier-spotting state.
- [x] HUD/state clearly says what changed and what the next repo action is.
- [x] Native capture/probe evidence covers `market_watch` and the courier
      spotting follow-up.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0121 market-watch pass. Scope is only courier
  spotting; no broad chase system, district expansion, weapon inventory,
  economy system, or mission menu in this slice.
- 2026-06-23: Implemented `courier_spotted` story state, DevAPI action
  `game.action.spot_courier`, HUD/state `JOB: TAIL ROUTE`, and native capture
  `tmp/blockside-heat/courier-spotted-latest.png`; product-read gate passed in
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T15-04-37-470Z_desktop.md; screenshot: tmp/blockside-heat/courier-spotted-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement the first tail-route movement beat as the next narrow playable slice.
