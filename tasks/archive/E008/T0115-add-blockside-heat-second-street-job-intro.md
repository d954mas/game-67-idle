---
id: T0115
title: Add Blockside Heat second street job intro
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, story, mission]
created: 2026-06-23
updated: 2026-06-23
---

## What

Add a second narrow street-job intro after the package route so Blockside Heat
starts to feel like an open-world crime game with story progression, not a
single isolated task.

## Done when

- [x] Completing the package route unlocks or previews a second named job beat.
- [x] HUD/state communicates the story progression without a modal-heavy UI.
- [x] Native capture/probe evidence covers first job completion and second-job
      readiness.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0114 pursuit pass. Scope is one story/mission
  progression beat, not a broad quest system.
- 2026-06-23: Added `second_ready` state, `next_job=rita_repo_tip`, HUD `JOB:
  RITA REPO`, blue-jacket marker reuse, and capture assertions. Evidence:
  `tmp/blockside-heat/capture-states-report.json`,
  `tmp/blockside-heat/job-complete-latest.png`,
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-20-17-833Z_desktop.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-17-58-759Z_desktop.json; screenshot: tmp/blockside-heat/job-complete-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement the Rita repo interaction as the next narrow playable mission beat.
