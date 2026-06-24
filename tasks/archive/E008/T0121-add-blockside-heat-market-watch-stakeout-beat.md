---
id: T0121
title: Add Blockside Heat market-watch stakeout beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, npc]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `van_rumor`, let the player stake out the market lead as one small
story/NPC beat. Keep it focused on a visible market contact or watch point,
state/HUD response, and capture evidence.

## Done when

- [x] After `van_rumor`, reaching or interacting with the market watch point
      advances a named stakeout state.
- [x] HUD/state clearly says what changed and what the next repo action is.
- [x] Native capture/probe evidence covers `van_rumor` and the market-watch
      follow-up.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0120 van-rumor pass. Scope is only the market-watch
  stakeout beat; no broad chase system, district expansion, weapon inventory,
  economy system, or mission menu in this slice.
- 2026-06-23: Implemented `market_watch` story state, DevAPI action
  `game.action.start_market_watch`, HUD/state `JOB: COURIER WATCH`, `WANTED 1`
  pressure, and native capture `tmp/blockside-heat/market-watch-latest.png`;
  product-read gate passed in
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-58-34-888Z_desktop.md; screenshot: tmp/blockside-heat/market-watch-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement the courier-spotting follow-up as the next narrow playable beat.
