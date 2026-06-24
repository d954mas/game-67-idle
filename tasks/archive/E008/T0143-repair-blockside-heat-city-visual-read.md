---
id: T0143
title: Repair Blockside Heat city visual read
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, visual, assets, lead-rejection]
created: 2026-06-24
updated: 2026-06-24
---

## What

Lead/player feedback: the current Blockside Heat screen still reads like flat
square roads plus colored cubes. Stop feature/content expansion and make one
small visual repair pass on the native city block using project-local reused
assets first.

Scope: improve the existing playable scene's visual read with denser low-poly
city dressing: sidewalks/curbs or street-edge framing, more believable parked
cars/props/pedestrians, better marker presentation, and screenshot proof. Do
not add a new district, mission, economy system, weapon inventory, or UI menu in
this slice.

## Done when

- [x] A fresh native screenshot no longer reads primarily as square roads and
      colored cubes.
- [x] Existing reused GLB assets remain project-local and provenance remains in
      `gamedesign/projects/blockside-heat/data/asset_manifest.json`.
- [x] Native capture/probe still passes through `repo_tool_cache`.
- [x] Product/readability gate records the lead-rejection visual fix and either
      passes strict visual gate or leaves the exact next visual blocker.

## Open questions

## Log

- 2026-06-24: Created immediately after lead feedback: "I see just squares,
  two roads, and colored cubes." T0142 made the first repair by adding denser
  mesh placement and submesh tint; this task owns the next visual pass before
  more story expansion.
- 2026-06-24: Resolved the specific roads-and-cubes read with street-edge
  framing, crosswalk stripes, a physical cache prop, another NPC, and existing
  project-local GLB families only. Build PASS: `cmake --build --preset
  native-debug`. Capture PASS: `python tools/blockside-heat/capture_states.py
  --port 9165`. Product gate PASS:
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_T0143_city_visual_repair.json`.
  Screenshot: `tmp/blockside-heat/repo-tool-cache-latest.png`.
- 2026-06-24: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0143_city_visual_repair.md; screenshot: tmp/blockside-heat/repo-tool-cache-latest.png; evidence: Build PASS: cmake --build --preset native-debug. Capture PASS: python tools/blockside-heat/capture_states.py --port 9165. Product gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_T0143_city_visual_repair.json. Screenshot: tmp/blockside-heat/repo-tool-cache-latest.png.; resolved rejection: Lead rejected the scene as just square roads and colored cubes; proof at tmp/blockside-heat/repo-tool-cache-latest.png now shows sidewalks/curbs, crosswalk detail, physical cache prop, more cars/NPCs, and denser low-poly buildings using project-local GLB assets.; next: (none)
