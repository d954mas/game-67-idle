---
id: T0116
title: Add Blockside Heat Rita repo interaction beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, story, mission, npc]
created: 2026-06-23
updated: 2026-06-23
---

## What

Make the unlocked Rita repo tip playable as a tiny NPC interaction: after the
first package route, reaching the blue-jacket contact starts or previews the
repo job with a clear next action.

## Done when

- [x] Player can approach Rita after `second_ready` and trigger the repo intro.
- [x] HUD/state names the repo job's immediate next action without adding a
      broad mission system.
- [x] Native capture/probe evidence covers second-job readiness and Rita
      interaction.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0115 second-job intro passed. Scope is the Rita
  contact interaction only; no repo vehicle chase, inventory, economy, or
  district expansion in this slice.
- 2026-06-23: Added `repo_intro` state, `repo_intro_active`,
  `game.action.talk_rita`, HUD `JOB: RED COMPACT`, UI tree labels, and capture
  assertions. Evidence: `tmp/blockside-heat/capture-states-report.json`,
  `tmp/blockside-heat/rita-contact-latest.png`,
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-28-59-930Z_desktop.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-24-08-423Z_desktop.json; screenshot: tmp/blockside-heat/rita-contact-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement the red compact repo driving objective as the next narrow playable slice.
