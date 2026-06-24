---
id: T0120
title: Add Blockside Heat east van rumor follow-up
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `stash_lead`, let the player follow the east van rumor into one small
playable follow-up beat. Keep it as a direct story/vehicle continuation: marker,
interaction, state/HUD response, and capture evidence.

## Done when

- [x] After `stash_lead`, reaching or following the east marker advances a named
      van-rumor state.
- [x] HUD/state clearly says what changed and what the next action is.
- [x] Native capture/probe evidence covers `stash_lead` and the van-rumor
      follow-up.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0119 stash-lead pass. Scope is only the east
  van-rumor follow-up; no chase system, economy, district expansion, weapon
  inventory, or mission menu in this slice.
- 2026-06-23: Implemented `van_rumor` story state, DevAPI action
  `game.action.follow_van_rumor`, HUD/state `JOB: MARKET WATCH`, visible east
  marker coverage, and native capture `tmp/blockside-heat/van-rumor-latest.png`;
  product-read gate passed in
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-51-31-408Z_desktop.md; screenshot: tmp/blockside-heat/van-rumor-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement the market-watch stakeout as the next narrow playable beat.
