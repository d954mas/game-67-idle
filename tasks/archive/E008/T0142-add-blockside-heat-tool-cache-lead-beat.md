---
id: T0142
title: Add Blockside Heat tool-cache lead beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, weapon]
created: 2026-06-23
updated: 2026-06-24
---

## What

After `repo_crew_pickup`, add the next smallest tool-cache beat: the player
reaches one cache marker, story records a named tool-cache state, and HUD/state
expose the next playable hook without adding weapon inventory or combat systems
yet.

## Done when

- [x] After `repo_crew_pickup`, reaching one marker advances a named tool-cache
      state.
- [x] HUD/state tells the player the cache is found and what is next.
- [x] Native capture/probe evidence covers crew-pickup and tool-cache states.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0141 crew-pickup pass. Scope is one marker/story
  hook after `repo_crew_pickup`; no weapon inventory, combat redesign, traffic,
  economy, new district, or mission menu in this slice.
- 2026-06-24: Added `repo_tool_cache`, HUD/state/DevAPI coverage, capture
  assertions, and screenshot proof at
  `tmp/blockside-heat/repo-tool-cache-latest.png`. Lead called out the visual
  as too square/debug-like, so this slice also repaired the immediate blocker
  by adding denser reused mesh placement and submesh tint detail. Build PASS:
  `cmake --build --preset native-debug`. Capture PASS:
  `python tools/blockside-heat/capture_states.py --port 9165`. Product gate
  PASS:
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0142_repo_tool_cache.json`.
- 2026-06-24: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0142_repo_tool_cache.md; screenshot: tmp/blockside-heat/repo-tool-cache-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Capture PASS: python tools/blockside-heat/capture_states.py --port 9165. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0142_repo_tool_cache.json. Screenshot: tmp/blockside-heat/repo-tool-cache-latest.png. Visual feedback addressed in-slice: added denser reused mesh placement and submesh tint detail after lead reported the scene looked like square roads and colored cubes.; next: (none)
