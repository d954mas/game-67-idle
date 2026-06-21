---
type: Visual Runtime Contract
title: T0018 Old Mine Scout Runtime Contract
tags: [project, ember-road, visual, ux, runtime, y-up, lead-rejection]
updated: 2026-06-21
---

# T0018 Old Mine Scout Runtime Contract

## 5-Line Contract

- goal: turn the rejected Old Mine modal into a playable-looking scout/result
  state with visible route, threat, resource, reward, and log feedback.
- non-goal: no full dungeon system, repeatable mining economy, new enemy
  combat loop, new inventory categories, or final sliced UI kit.
- proof: native screenshot `state_old_mine_scout_result.png`, strict visual/UX
  gate against `visual_ux_rejection_reference_digest.md`, DevAPI smoke, capture
  matrix, taskboard validate, visual invariant guard, and `node tools/ai.mjs
  validate`.
- stop condition: any strict product fail, lead rejection, or Y-up invariant
  violation blocks feature/content expansion until fixed.
- likely files: `state/game_state.schema.json`, generated `src/generated/*`,
  `src/game_actions.*`, `src/clean_seed_main.c`,
  `tools/ember-road/capture_states.py`, `tools/devapi/smoke.py`, T0018 docs.

## Accepted Working Target

Working target for this runtime pass:

`gamedesign/projects/ember-road/art/ember-road-old-mine-scout-result-direction-v001.png`

This target is not final UI art. It is accepted only as an implementation
direction for a native proof pass: scene-first mine entrance, functional route
strip, scout result, threat/resource/reward rail, and bottom log.

## Runtime Proof Requirements

- The player-facing scout action must not say `NEXT SLICE`.
- After scouting, the screen must show a new state: Old Mine depth, cave threat,
  ember shard/resource gain, and reward/progression feedback.
- The primary control hierarchy must be clear: scout result first, return/back
  secondary.
- The route strip must communicate current destination rather than act as pure
  decoration.
- UI/world/game layout remains Y-up. Boundary conversions must stay localized
  to renderer, input, screenshot, or DevAPI rectangle adapters.

## Product Gate

Use a strict review gate against:

- current rejection lock:
  `gamedesign/projects/ember-road/reviews/T0018_visual_ux_rejection_lock.md`
- reference digest:
  `gamedesign/projects/ember-road/references/visual_ux_rejection_reference_digest.md`
- target image:
  `gamedesign/projects/ember-road/art/ember-road-old-mine-scout-result-direction-v001.png`
- native proof screenshot:
  `build/captures/ember-road/state_old_mine_scout_result.png`

## Runtime Proof Result

2026-06-21 native proof:

- screenshot:
  `build/captures/ember-road/state_old_mine_scout_result.png`
- gate:
  `gamedesign/projects/ember-road/reviews/T0018_old_mine_scout_result_runtime_gate.md`
- result: REVIEW, because the rejected `NEXT SLICE` scaffold is removed and
  scout/result state is playable, but lead acceptance and final sliced UI art
  remain open.

2026-06-21 route/log improvement:

- screenshot:
  `build/captures/ember-road/state_old_mine_scout_result.png`
- gate:
  `gamedesign/projects/ember-road/reviews/T0018_old_mine_route_log_runtime_gate.md`
- result: REVIEW; route plaques now carry state and bottom feedback is a framed
  report log, but lead acceptance remains open.
