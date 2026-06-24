---
id: T0118
title: Add Blockside Heat curb scout arrival response
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, vehicle, mission, story]
created: 2026-06-23
updated: 2026-06-23
---

## What

Make the first repo driving objective respond when the player reaches the orange
curb scout marker: the car route should complete a small beat and point to the
next repo action.

## Done when

- [x] Driving to the curb scout marker after `repo_drive` advances state to a
      named completion/next-action stage.
- [x] HUD/state communicates the arrival response without adding a broad mission
      system.
- [x] Native capture/probe evidence covers repo drive and curb arrival.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0117 repo-drive pass. Scope is only arrival at the
  first curb marker and a small response; no chase, garage, economy, district
  expansion, or mission menu in this slice.
- 2026-06-23: Added `repo_scout_complete`, `game.action.complete_repo_scout`,
  HUD `JOB: STASH SPOTTED`, cash +25, purple-block next marker, and capture
  assertions. Evidence: `tmp/blockside-heat/capture-states-report.json`,
  `tmp/blockside-heat/repo-scout-complete-latest.png`,
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-38-00-800Z_desktop.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-38-00-800Z_desktop.json; screenshot: tmp/blockside-heat/repo-scout-complete-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement the purple-block stash lead interaction as the next narrow playable beat.
