---
id: T0117
title: Add Blockside Heat red compact repo driving objective
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, vehicle, mission, story]
created: 2026-06-23
updated: 2026-06-23
---

## What

Make the repo intro's immediate driving objective playable: after talking to
Rita, entering the red compact should advance the repo job to a simple scout
route target.

## Done when

- [x] After `repo_intro`, entering the car advances state to a named repo
      driving objective.
- [x] HUD/state tells the player where to drive next without adding a broad
      mission system.
- [x] Native capture/probe evidence covers Rita contact, car entry, and the new
      route objective.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0116 Rita contact pass. Scope is only the first
  repo driving objective after car entry; no chase, vehicle theft system,
  garage economy, district expansion, or mission menu in this slice.
- 2026-06-23: Added `repo_drive` state, `repo_drive_active`,
  `game.action.enter_repo_car`, HUD `JOB: CURB SCOUT`, orange curb marker, and
  capture assertions. Evidence: `tmp/blockside-heat/capture-states-report.json`,
  `tmp/blockside-heat/repo-drive-latest.png`,
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-33-37-587Z_desktop.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-33-37-587Z_desktop.json; screenshot: tmp/blockside-heat/repo-drive-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement arrival at the curb scout marker and a small repo-route completion response.
