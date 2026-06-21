---
id: T0021
title: Old Mine next delve choice runtime proof
status: review
epic: E003
priority: P1
tags: [prototype, ember-road, native-first, old-mine, state, ux, lead-rejection, y-up]
created: 2026-06-20
updated: 2026-06-20
---

## What

### Iteration Goal

After Depth 1 is cleared, the Old Mine screen should no longer feel dead or
rail-only. The player should see one real next choice: delve the visible ember
cache, get a small deterministic reward, and still have Back to Old Gate as a
safe secondary path.

### Scope

- Add schema-first state for one Old Mine cache delve result.
- Add semantic action `game.action.delve_old_mine`.
- Replace the post-Depth-1 `CLEARED` button with an active `DELVE`/`CACHE`
  choice and a reward result state.
- Add DevAPI/smoke/capture coverage for
  `state_old_mine_next_delve_choice.png`.

### Out Of Scope

- No repeatable dungeon loop, procedural floors, enemy tables, crafting,
  inventory expansion, shop/economy, or final sliced UI kit.
- No Y-down layout semantics. Game/world/UI stays Y-up; Y-down conversion is
  boundary-only.

### Proof

- Native screenshot:
  `build/captures/ember-road/state_old_mine_next_delve_choice.png`.
- Strict product gate:
  `gamedesign/projects/ember-road/reviews/T0021_old_mine_next_delve_choice_gate.md`.
- Native build, DevAPI smoke, capture states, taskboard validate, visual
  invariant guard, and `node tools/ai.mjs validate`.

## Done when

- [x] `old_mine.delve_count` and one-cache reward fields are schema-generated.
- [x] `game.action.delve_old_mine` is available after Depth 1 is resolved.
- [x] Native UI shows an active next choice after Depth 1 instead of `CLEARED`.
- [x] DevAPI/capture evidence includes `old_mine_next_delve_choice`.
- [x] Required validation passes and the product gate records PASS/REVIEW with
      exact remaining debt.

## Open questions

- Should the next accepted slice push to Depth 2, or go back to town/equipment
  polish after this cache proof?

## Log

- 2026-06-21: Started after T0020 refreshed the reference/UX target and named
  `state_old_mine_next_delve_choice.png` as the next narrow runtime proof.
- 2026-06-21: Implemented one-cache Old Mine delve proof:
  `game.action.delve_old_mine`, `old_mine.delve_count`,
  `old_mine.cache_claimed`, last delve reward fields, active DELVE/CACHE UI,
  DevAPI node `ember.mine.next_delve`, and captures
  `build/captures/ember-road/state_old_mine_next_delve_choice.png` plus
  `build/captures/ember-road/state_old_mine_delve_reward.png`. Validation:
  native build passed, DevAPI smoke passed 49/49, capture states passed, gate
  recorded REVIEW.
- 2026-06-20: product gate REVIEW (desktop); review: gamedesign/projects/ember-road/reviews/T0021_old_mine_next_delve_choice_gate.md; screenshot: build/captures/ember-road/state_old_mine_next_delve_choice.png; next: If accepted, decide whether to push Depth 2 or return to town/equipment polish; if rejected, revise the fake shot/reference target before more runtime systems.
- 2026-06-20: Old Mine next delve proof implemented: native build passed, DevAPI smoke 49/49, capture states produced state_old_mine_next_delve_choice.png and state_old_mine_delve_reward.png, product gate REVIEW recorded.
