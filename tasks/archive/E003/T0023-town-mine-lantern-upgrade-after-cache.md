---
id: T0023
title: Town mine lantern upgrade after cache
status: dropped
epic: E003
priority: P1
tags: [prototype, ember-road, native-first, town, equipment, old-mine, state, ux, y-up]
created: 2026-06-20
updated: 2026-06-21
---

## What

### Iteration Goal

After recovering the first Old Mine cache, returning to Old Gate should reveal
a concrete town/equipment progression step: forge a Mine Lantern from ember
shards to unlock the next depth promise.

### Scope

- Add schema-first state for `gear.mine_lantern` and
  `old_mine.depth2_unlocked`.
- Add one semantic action: `game.action.forge_mine_lantern`.
- Update town UI, route plaque, bottom log, DevAPI, smoke, and captures so the
  cache reward visibly becomes a town upgrade.

### Out Of Scope

- No Depth 2 combat, procedural floors, shop economy, inventory expansion, or
  final art kit.
- No Y-down layout semantics. Game/world/UI remains Y-up; boundary conversions
  only.

### Proof

- Native screenshot:
  `build/captures/ember-road/state_town_lantern_upgrade.png`.
- Strict product gate:
  `gamedesign/projects/ember-road/reviews/T0023_town_lantern_upgrade_gate.md`.
- Native build, DevAPI smoke, capture states, taskboard validate, visual
  invariant guard, and `node tools/ai.mjs validate`.

## Done when

- [x] Lantern/depth2 fields are schema-generated.
- [x] `game.action.forge_mine_lantern` is available after cache return.
- [x] Town UI shows the Mine Lantern upgrade as the next action.
- [x] DevAPI/capture evidence includes `town_lantern_upgrade`.
- [x] Required validation passes and product gate records PASS/REVIEW.

## Open questions

- Should the next slice use the lantern to enter Depth 2 or improve equipment
  presentation in town first?

## Log

- 2026-06-21: Lead closed the Ember Road prototype. This task is dropped and
  archived as historical evidence only; do not continue it unless explicitly
  reopened.
- 2026-06-21: Started after T0022 put cache/depth anchors into the Old Mine
  scene. Next step is linking cache reward back to town/equipment progression.
- 2026-06-21: Added Mine Lantern town upgrade: `gear.mine_lantern`,
  `old_mine.depth2_unlocked`, `game.action.forge_mine_lantern`, town forge UI,
  DevAPI/smoke coverage, and captures
  `build/captures/ember-road/state_town_lantern_upgrade.png` plus
  `build/captures/ember-road/state_town_lantern_forged.png`. Gate:
  `gamedesign/projects/ember-road/reviews/T0023_town_lantern_upgrade_gate.md`
  recorded REVIEW.
- 2026-06-20: product gate REVIEW (desktop); review: gamedesign/projects/ember-road/reviews/T0023_town_lantern_upgrade_gate.md; screenshot: build/captures/ember-road/state_town_lantern_upgrade.png; next: If accepted, use the lantern to add a narrow Depth 2 entry/push slice; if rejected, create dedicated forge/mine object art before more gameplay.
- 2026-06-20: Mine Lantern town upgrade implemented: state/action/UI/DevAPI/captures added, smoke 56/56, product gate REVIEW recorded.
