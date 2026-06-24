---
id: T0140
title: Add Blockside Heat next-score lead beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_final_call`, add the next smallest playable lead beat: the player
reaches one next-score marker, story records a named next-score state, and
HUD/state expose the next playable hook without adding new broad systems.

## Done when

- [x] After `repo_final_call`, reaching one marker advances a named next-score
      lead state.
- [x] HUD/state tells the player the bigger-score lead is active and what is
      next.
- [x] Native capture/probe evidence covers final-call and next-score states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0139 final-call pass. Scope is one marker/story
  hook after `repo_final_call`; no broad chase AI, traffic, factions, economy,
  weapon inventory, new district, mission menu, or multi-step mission branch in
  this slice.
- 2026-06-23: Implemented `repo_next_score_lead` after Rita's final call with
  `JOB: BIG SCORE`, `next_job=repo_crew_pickup`, cash 510, wanted 0, DevAPI
  action, capture assertions, and screenshot evidence. Evidence:
  `cmake --build --preset native-debug` PASS;
  `python tools/blockside-heat/capture_states.py --port 9165` PASS; strict
  product gate PASS at
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0140_repo_next_score_lead.json`.
  Screenshot: `tmp/blockside-heat/repo-next-score-lead-latest.png`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0140_repo_next_score_lead.md; screenshot: tmp/blockside-heat/repo-next-score-lead-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Capture PASS: python tools/blockside-heat/capture_states.py --port 9165. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0140_repo_next_score_lead.json. Screenshot: tmp/blockside-heat/repo-next-score-lead-latest.png.; next: (none)
