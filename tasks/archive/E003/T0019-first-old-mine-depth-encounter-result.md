---
id: T0019
title: First Old Mine depth encounter result
status: dropped
epic: E003
priority: P1
tags: [prototype, ember-road, native-first, old-mine, state, encounter]
created: 2026-06-20
updated: 2026-06-21
---

## What

Add one narrow gameplay result after the Old Mine scout: resolve the first
Depth 1 Cave Bat encounter as an automated outcome, show threat/result/reward
feedback, and keep the route/log UX readable.

### Iteration Goal

After scouting the Old Mine, the player can resolve one first-depth encounter
and see a clear result: Cave Bat defeated, depth progress, ember shards/XP/gold
reward, and a return path.

### Scope

- Add schema-first persistent fields for the first mine encounter result.
- Add one semantic action: `game.action.resolve_old_mine_depth`.
- Update Old Mine UI and DevAPI tree to show encounter ready/resolved states.
- Add native capture/smoke/product-gate evidence for the new state.

### Out Of Scope

- No repeatable dungeon loop, procedural floors, enemy tables, inventory
  expansion, crafting, shops, town systems, or final sliced UI kit.
- No Y-down layout semantics. Game/world/UI stays Y-up; only boundary adapters
  may convert to Y-down.

### Proof

- Native screenshot:
  `build/captures/ember-road/state_old_mine_depth_encounter.png`.
- Strict product gate:
  `gamedesign/projects/ember-road/reviews/T0019_old_mine_depth_encounter_gate.md`.
- Native build, DevAPI smoke, capture states, taskboard validate, visual
  invariant guard, and `node tools/ai.mjs validate`.

## Done when

- [x] `old_mine.depth_resolved` and first encounter reward/progress fields are
      schema-generated and visible through `game.state`.
- [x] `game.action.resolve_old_mine_depth` is available after scouting and
      deterministic for this slice.
- [x] Native UI shows a clear Cave Bat/depth result and no `NEXT SLICE`
      player-facing scaffold.
- [x] `old_mine_depth_encounter` capture is covered in the live-state matrix.
- [x] Required validation passes and the product gate records PASS/REVIEW with
      exact remaining debt.

## Open questions

- Should the next slice turn depth 1 into a repeatable dungeon loop or return
  to town/equipment polish first?

## Log

- 2026-06-21: Lead closed the Ember Road prototype. This task is dropped and
  archived as historical evidence only; do not continue it unless explicitly
  reopened.
- 2026-06-21: Started after T0018 produced a readable scout/result screen.
  Keep this to one deterministic encounter result, not a broad dungeon system.
- 2026-06-21: Implemented deterministic Depth 1 Cave Bat result:
  `game.action.resolve_old_mine_depth`, `old_mine.depth_resolved`,
  `old_mine.bat_defeated`, `old_mine.bat_damage`, `old_mine.depth_gold`, and
  capture `build/captures/ember-road/state_old_mine_depth_encounter.png`.
  Product gate:
  `gamedesign/projects/ember-road/reviews/T0019_old_mine_depth_encounter_gate.md`.
  Validation passed: native build, DevAPI smoke 41/41, capture states, visual
  invariant guard.
