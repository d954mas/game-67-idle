---
id: T0139
title: Add Blockside Heat final-call beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, wanted]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `repo_safehouse_drop`, add the next smallest final-call beat: the player
reaches one call marker, the story records a named final-call state, and
HUD/state expose the next playable hook.

## Done when

- [x] After `repo_safehouse_drop`, reaching one call marker advances a named
      final-call state.
- [x] HUD/state tells the player Rita's call landed and what is next.
- [x] Native capture/probe evidence covers safehouse-drop and final-call states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0138 safehouse-drop pass. Scope is one final-call
  marker/story hook only; no broad chase AI, traffic, factions, economy, new
  district, weapon inventory, or mission menu in this slice.
- 2026-06-23: Implemented `repo_final_call` after safehouse-drop with HUD/state
  `JOB: RITA CALL`, `next_job=repo_next_score`, cash 500, wanted 0, DevAPI
  action, and final-call marker. Evidence: `cmake --build --preset native-debug`
  PASS; `python tools/blockside-heat/capture_states.py --port 9165` PASS;
  strict product gate PASS at
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0139_repo_final_call.json`.
  Screenshot: `tmp/blockside-heat/repo-final-call-latest.png`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0139_repo_final_call.md; screenshot: tmp/blockside-heat/repo-final-call-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Capture PASS: python tools/blockside-heat/capture_states.py --port 9165. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0139_repo_final_call.json. Screenshot: tmp/blockside-heat/repo-final-call-latest.png.; next: (none)
