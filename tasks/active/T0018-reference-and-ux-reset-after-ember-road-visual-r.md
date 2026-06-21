---
id: T0018
title: Reference and UX reset after Ember Road visual rejection
status: review
epic: E003
priority: P0
tags: [prototype, ember-road, visual, ux, references, lead-rejection]
created: 2026-06-20
updated: 2026-06-20
---

## What

Record the lead rejection that the current Ember Road visual and UX are not the
desired game. Refresh the reference digest with closer RPG refs, current
screenshot mismatch, and a Y-up layout contract before any new gameplay/content
slice continues.

Out of scope: implementing the first mine encounter, adding economy, adding
new enemies/rewards, or changing runtime art before the new visual/UX target is
accepted.

## Done when

- [x] A strict visual rejection gate records the current screenshot as failed
      lead feedback.
- [x] A refreshed reference digest names closer references, current mismatch,
      borrow/avoid/copy-risk, and next proof.
- [x] The digest explicitly preserves Y-up game/world/UI layout and limits
      Y-down conversion to renderer/input/screenshot/DevAPI boundaries.
- [x] A new direction board or fake shot for the rejected screen is created
      and reviewed.
- [ ] Lead accepts the direction target or provides the next mismatch.
- [x] The next native screenshot/gate proves the revised UX before content
      expansion resumes.

## Open questions

- Does the next accepted target lean closer to old-browser dense RPG chrome
  (Legend/Dragon Eternity) or cleaner scene-first RPG readability
  (DragonFable/AdventureQuest/KingsRoad)?

## Log

- 2026-06-20: Lead feedback rejected current visual and UX as not matching the
  desired game. Added strict fail gate:
  `gamedesign/projects/ember-road/reviews/T0018_visual_ux_rejection_lock.md`.
- 2026-06-20: Added rejection digest:
  `gamedesign/projects/ember-road/references/visual_ux_rejection_reference_digest.md`.
  The next implementation pass is locked behind an accepted direction/fake-shot
  target and Y-up layout audit.
- 2026-06-20: Generated and saved direction fake shot:
  `gamedesign/projects/ember-road/art/ember-road-old-mine-scout-result-direction-v001.png`;
  prompt and generation record live under `gamedesign/projects/ember-road/art/`.
  Review gate:
  `gamedesign/projects/ember-road/reviews/T0018_old_mine_direction_target_review.md`.
  Runtime remains locked until lead acceptance or next mismatch.
- 2026-06-20: product gate FAIL (desktop); review: gamedesign/projects/ember-road/reviews/T0018_visual_ux_rejection_lock.md; screenshot: build/captures/ember-road/state_modal_or_choice_open.png; next: Stop gameplay expansion, refresh the Reference Digest with closer RPG refs and current-screenshot mismatch, then build a new accepted direction/fake-shot target that preserves Y-up layout before runtime UI/art changes.
- 2026-06-21: Added first native scout/result proof:
  `build/captures/ember-road/state_old_mine_scout_result.png`.
  Runtime now has `game.action.scout_old_mine`, `old_mine.scouted`,
  `old_mine.depth`, `old_mine.ember_shards`, and UI node
  `ember.mine.scout_result`. Gate:
  `gamedesign/projects/ember-road/reviews/T0018_old_mine_scout_result_runtime_gate.md`.
  Validation passed: native build, DevAPI smoke 34/34, capture states with
  `old_mine_scout_result`, visual invariant guard. Lead acceptance remains open.
- 2026-06-21: Improved native route/log UX for the scout result. Route plaques
  now expose `TOWN`, `CLEAR`, and `DEPTH 1`; bottom feedback is a framed report
  log instead of loose text. Gate:
  `gamedesign/projects/ember-road/reviews/T0018_old_mine_route_log_runtime_gate.md`.
  Validation passed: native build, capture states, visual invariant guard.
