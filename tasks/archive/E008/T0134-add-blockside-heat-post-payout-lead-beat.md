---
id: T0134
title: Add Blockside Heat post-payout lead beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_payout_meet`, add the next smallest new-lead beat: Rita points the
player to one new street lead marker, the story records a named lead state, and
HUD/state expose the next playable hook.

## Done when

- [x] After `repo_payout_meet`, reaching one new lead marker advances a named
      post-payout lead state.
- [x] HUD/state tells the player the new lead was found and what is next.
- [x] Native capture/probe evidence covers payout done and post-payout lead
      states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0133 payout-meet pass. Scope is one post-payout
  lead marker and story hook only; no broad mission menu, economy, factions,
  traffic, chase AI, new district, or weapon inventory in this slice.
- 2026-06-23: Implemented `repo_next_lead`: after `repo_payout_meet`, the player
  can follow one street lead marker, HUD shows `JOB: NEW LEAD`, wanted rises to
  1, and `next_job` becomes `repo_heat_watch`. Evidence:
  `cmake --build --preset native-debug`; `python tools/blockside-heat/capture_states.py --port 9165`;
  `tmp/blockside-heat/repo-payout-meet-latest.png`;
  `tmp/blockside-heat/repo-next-lead-latest.png`;
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0134_repo_next_lead.json`
  PASS.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0134_repo_next_lead.md; screenshot: tmp/blockside-heat/repo-next-lead-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Native capture PASS: python tools/blockside-heat/capture_states.py --port 9165; report tmp/blockside-heat/capture-states-report.json covers repo_payout_meet and repo_next_lead. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0134_repo_next_lead.json. Screenshot: tmp/blockside-heat/repo-next-lead-latest.png.; next: (none)
