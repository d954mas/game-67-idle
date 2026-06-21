---
id: T0022
title: Old Mine scene-integrated cache and depth anchors
status: review
epic: E003
priority: P1
tags: [prototype, ember-road, native-first, old-mine, visual-direction, ux, lead-rejection, y-up]
created: 2026-06-20
updated: 2026-06-20
---

## What

### Visual Session Contract

- goal: make the Old Mine scene itself communicate cache/depth/threat state,
  not only the right rail.
- non-goal: no new dungeon mechanics, new economy, new generated art kit, or
  copied reference layout.
- proof: updated native screenshot
  `build/captures/ember-road/state_old_mine_next_delve_choice.png` plus strict
  product gate.
- stop condition: if the screenshot still reads as rail-only or visually
  confusing, do not add more mine systems.
- likely files: `src/clean_seed_main.c`, capture/gate docs, task/status docs.

### Iteration Goal

Improve the T0021 Old Mine next-delve proof by adding scene-integrated anchors:
the cache, the Depth 2 lock, and the cleared threat should be visible on the
mine backdrop before the player reads the rail.

### Scope

- Remove the misleading wolf/ring reward overlay from Old Mine states.
- Reuse existing legal project-local atlas pieces for cache, lock, and cleared
  markers on the mine scene.
- Keep `DELVE/CACHE` as the active next action; this is a visual/UX pass, not a
  state or progression expansion.

### Out Of Scope

- No schema changes, repeatable dungeon loop, Depth 2 combat, inventory, shop,
  final UI kit, or asset generation.
- No Y-down layout semantics. All scene anchor placement remains Y-up.

### Proof

- Native screenshot:
  `build/captures/ember-road/state_old_mine_next_delve_choice.png`.
- Strict product gate:
  `gamedesign/projects/ember-road/reviews/T0022_old_mine_scene_anchors_gate.md`.
- Native build, DevAPI smoke, capture states, visual invariant guard,
  taskboard validate, and `node tools/ai.mjs validate`.

## Done when

- [x] Old Mine no longer displays the old wolf/ring reward overlay as the
      cache signifier.
- [x] Scene overlay shows cache/depth/lock state using existing runtime assets.
- [x] Screenshot evidence shows the next action is readable from the scene and
      rail together.
- [x] Required validation passes and the product gate records PASS/REVIEW with
      remaining visual debt.

## Open questions

- If this is still visually off, should the next pass generate a dedicated
  mine object/source sheet rather than reusing existing icons?

## Log

- 2026-06-21: Started after T0021 proved the DELVE/CACHE action but still kept
  most meaning in the right rail and bottom log.
- 2026-06-21: Added scene-integrated mine anchors: ember cache marker, D2 lock,
  cache-taken marker, and disabled the misleading wolf/ring reward overlay on
  Old Mine states. Evidence:
  `build/captures/ember-road/state_old_mine_next_delve_choice.png` and
  `build/captures/ember-road/state_old_mine_delve_reward.png`. Gate:
  `gamedesign/projects/ember-road/reviews/T0022_old_mine_scene_anchors_gate.md`
  recorded REVIEW with minor readability/art-source debt.
- 2026-06-20: product gate REVIEW (desktop); review: gamedesign/projects/ember-road/reviews/T0022_old_mine_scene_anchors_gate.md; screenshot: build/captures/ember-road/state_old_mine_next_delve_choice.png; next: If accepted, decide between a dedicated mine object art sheet and the next gameplay slice; if rejected, revise the mine target/fake shot before more systems.
- 2026-06-20: Scene-integrated Old Mine anchors implemented: cache/D2 lock/cache-taken markers visible in scene, misleading ring overlay removed from mine states, capture states and gate REVIEW recorded.
