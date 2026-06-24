---
id: T0119
title: Add Blockside Heat purple stash lead interaction
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, npc]
created: 2026-06-23
updated: 2026-06-23
---

## What

Make the purple-block stash lead interactable after the curb scout response, so
the repo chain has one more explicit story beat instead of stopping at a marker.

## Done when

- [x] After `repo_scout_complete`, reaching the purple-block marker triggers a
      named stash-lead interaction.
- [x] HUD/state communicates the next repo beat without adding a broad mission
      system.
- [x] Native capture/probe evidence covers curb completion and stash-lead
      interaction.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0118 curb-arrival pass. Scope is only the
  purple-block lead interaction; no chase, combat escalation, economy, district
  expansion, or mission menu in this slice.
- 2026-06-23: Implemented `stash_lead` story state, DevAPI action
  `game.action.open_stash_lead`, HUD/state `JOB: VAN RUMOR`, and native capture
  `tmp/blockside-heat/stash-lead-latest.png`; strict product-read gate passed
  in `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-45-04-862Z_desktop.md; screenshot: tmp/blockside-heat/stash-lead-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement the east van-rumor follow-up as the next narrow playable beat.
