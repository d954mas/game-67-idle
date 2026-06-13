---
id: T0014
title: Add faster art-pack iteration command for agents
status: done
epic: ""
priority: P2
tags: [art, pipeline, pack]
created: 2026-06-12
updated: 2026-06-12
---

## What

Capture the discussion point that pack building may be fast enough due to
cache, so agents should not assume a direct PNG/runtime shortcut is better, and
add a concrete one-command pack iteration path once the reusable UI/character
assets are sliced.

## Done when

- [x] The point is captured in the art pipeline research.
- [x] The asset pipeline skill requires checking and measuring the pack/material
  path before bypassing it.
- [x] A concrete 67 World art pack command is added or documented after slicing
  the reusable UI/character assets.

## Open questions

None.

## Log

- 2026-06-12: Captured as part of T0015. The new rule is in
  `.codex/skills/game-asset-pipeline/SKILL.md` and
  `gamedesign/knowledge/ai_art_iteration_pipeline.md`; the concrete pack
  command remains to be added during the asset integration pass.
- 2026-06-12: Added explicit native pack path:
  `py -3.12 tools/assets/build_67_world_art.py`,
  `cmake --build --preset native-debug --target build_67_world_packs`, then
  `build/game_seed/native-debug/build_67_world_packs.exe build/game_seed/67-world-packs`.
  Evidence: `world67_art.ntpack` built at
  `build/game_seed/67-world-packs/world67_art.ntpack`; cached rerun reported
  `Cache: 7 hit / 0 miss`.
