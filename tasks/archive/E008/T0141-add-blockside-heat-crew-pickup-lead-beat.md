---
id: T0141
title: Add Blockside Heat crew-pickup lead beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, npc]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_next_score_lead`, add the next smallest crew-pickup beat: the
player reaches one crew marker, story records a named crew-pickup state, and
HUD/state expose the next playable hook without adding follower AI or crew
systems yet.

## Done when

- [x] After `repo_next_score_lead`, reaching one marker advances a named
      crew-pickup state.
- [x] HUD/state tells the player the crew pickup is done and what is next.
- [x] Native capture/probe evidence covers next-score lead and crew-pickup
      states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0140 next-score lead pass. Scope is one
  marker/story hook after `repo_next_score_lead`; no follower AI, crew roster,
  traffic, economy, weapon inventory, new district, or mission menu in this
  slice.
- 2026-06-23: Implemented `repo_crew_pickup` after the next-score lead with
  `JOB: CREW PICKUP`, `next_job=repo_tool_cache`, cash 520, wanted 0, DevAPI
  action, capture assertions, and screenshot evidence. Evidence:
  `cmake --build --preset native-debug` PASS;
  `python tools/blockside-heat/capture_states.py --port 9165` PASS; strict
  product gate PASS at
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0141_repo_crew_pickup.json`.
  Screenshot: `tmp/blockside-heat/repo-crew-pickup-latest.png`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0141_repo_crew_pickup.md; screenshot: tmp/blockside-heat/repo-crew-pickup-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Capture PASS: python tools/blockside-heat/capture_states.py --port 9165. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0141_repo_crew_pickup.json. Screenshot: tmp/blockside-heat/repo-crew-pickup-latest.png.; next: (none)
