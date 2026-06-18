---
id: T0007
title: Sprint escape during Backrooms blackout ambush
status: review
epic: E001
priority: P1
tags: []
created: 2026-06-18
updated: 2026-06-18
---

## What
Make the blackout ambush playable, not only scary: add a focused sprint escape
action that lets the player hold Shift to outrun the stalker during blackout at
the cost of battery/pressure. The HUD must make the action visible, and DevAPI
must prove sprint changes chase pressure.

## Done when

- [x] Holding Shift increases movement speed during active play and is exposed
      as `sprinting` in DevAPI.
- [x] Sprinting during blackout reduces or stabilizes stalker pressure compared
      to not sprinting, while draining battery enough to create a tradeoff.
- [x] HUD teaches `SHIFT SPRINT` and blackout prompt tells the player to sprint.
- [x] Native build, smoke, sprint-vs-no-sprint scenario, screenshot proof,
      readability, product gate, slice hygiene, and taskboard validation pass
      or record explicit non-gameplay debt.

## Open questions

- Should a later slice add full stamina and footstep audio, or keep sprint tied
  to battery pressure for this prototype?

## Log

- 2026-06-18: Started after T0006 blackout ambush because the on-screen `RUN`
  warning needs a real player action.
- 2026-06-18: product gate PASS (desktop); review: gamedesign\projects\backrooms-liminal\reviews\product_read_gate_t0007_desktop.md; screenshot: build/captures/backrooms_t0007_sprint_escape.png; next: continue to the next narrow slice
- 2026-06-18: Implemented held Shift sprint during blackout. Evidence: native
  build PASS, DevAPI smoke PASS,
  `build/captures/backrooms_t0007_sprint_status.json` checks all true,
  `build/captures/backrooms_t0007_sprint_escape.png`, readability PASS,
  product gate PASS, slice hygiene WARN only for advisory global profiler
  historical debt.
- 2026-06-18: product gate PASS (desktop); review: gamedesign\projects\backrooms-liminal\reviews\product_read_gate_t0007_desktop.md; screenshot: build/captures/backrooms_t0007_sprint_escape.png; next: continue to the next narrow slice
