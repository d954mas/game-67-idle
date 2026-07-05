---
id: T0286
title: "rb-dark-rpg: combat screen animated clash stage"
status: done
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, combat, visual, uiux]
created: 2026-07-04
updated: 2026-07-05
---

## What

Refactor the running combat view so the first autobattle reads as an animated
clash, not as a text-heavy result panel. This starts after T0285 proves victory
rewards and handoff correctness.

Iteration 1 scope: use the existing deterministic `game_combat_event_t`
timeline as the source of truth and add a central stage/presenter on current
placeholder silhouettes. The UI may animate sprite transforms, silhouettes,
hit flashes, recoil, shadows, and floating damage numbers, but must not
recalculate damage, crit/block, HP, outcome, or rewards.

Scope:

- central player/enemy staging with both actors visible at once;
- event-driven animation state: idle -> windup -> lunge -> impact -> recoil -> recover;
- HP bars and simple swing/timer feedback tied to actor lanes;
- floating damage numbers;
- crit/block markers only when they happen;
- last-three-events combat log in a separate zone;
- compact stats in a separate zone;
- 16:9 and phone portrait layout.

Out of scope:

- active skills, mana, consumables during combat, dodge/agility, status stacks;
- new balance curve beyond fixes needed for readability;
- generated or final asset-backed combat sprites; if placeholders are not
  enough after review, create a separate asset pass.

Implementation shape:

1. Split `running_ui` into explicit presenter zones:
   `combat_stage_ui`, `combat_stats_ui`, and `combat_log_ui`.
2. Add small helper functions for event lookup and per-actor animation pose;
   keep `game_combat_event_t` as the contract boundary.
3. Replace the central `УДАР` text block as the main focus with an actor clash:
   attacker lunges toward center, defender recoils, impact flash/damage appears,
   then both actors return to lanes.
4. Keep the current HP/log/result semantics intact; this is a visual presenter
   refactor, not a combat-system rewrite.
5. After first implementation, run a visual/code review, apply fixes, then
   capture final evidence.

## Done when

- [x] Combat view follows the locked v1 direction in
      `games/rb-dark-rpg/design/combat_reward_loop_v1.md`.
- [x] The running view has a central animated clash stage where player and enemy
      are visible simultaneously and the current hit reads without relying on
      the log.
- [x] Animation is driven from existing combat timeline events; UI does not
      recalculate damage, HP, crit/block, outcome, or rewards.
- [x] First gate fight and mill fight are visually readable at 960x540 and on
      phone portrait captures.
- [x] HP bars, damage numbers, crit/block markers, stats, and the
      last-three-event log live in separate zones and do not overlap.
- [x] Mid-fight evidence proves two actors, a nonblank impact/damage moment,
      updated HP after an event, and a readable last-three-event log.
- [x] Visual review evidence is captured under `tmp/quality/`.
- [x] Existing combat/unit scenario checks still pass.
- [x] Review pass covers gameplay readability and mobile/desktop layout; fixes
      from that review are either applied or logged as follow-up tasks.

## Open questions

- Resolved for this iteration: use current placeholder silhouettes/forms first.
  Generated or final asset-backed sprites are a later pass only if the review
  shows placeholders cannot sell the clash.

## Log

- 2026-07-05: Created as the next combat slice after T0285 victory reward
  correctness.
- 2026-07-05: Refined after plan review: T0286 is now the first narrow
  iteration for an event-driven animated clash stage on placeholders, with
  review/fix before acceptance. Later responsive polish or asset-backed sprites
  should be separate follow-up tasks instead of being bundled into this pass.
- 2026-07-05: 2026-07-05: Started implementation after lead direction: first pass is event-driven animated clash stage on placeholders; no combat math or reward changes.
- 2026-07-05: 2026-07-05: Implemented first animated clash-stage slice in games/rb-dark-rpg/src/ui/combat_flow.c. Running combat now uses event-driven actor poses over the existing game_combat_event_t timeline, with separate meters, central stage, stats summary, and last-three-event log; no combat math/reward changes. Added DevAPI prepare_combat_running scenario and fake test. Verification: py -3.12 games/rb-dark-rpg/devapi/scenarios_test.py; cmake --build games/rb-dark-rpg/build/native-debug --target game_combat_test first_scene_tests game; responsive_viewports prepare_combat_running desktop=960x540 phone=390x844 --audit. Evidence: tmp/quality/rb_dark_rpg_combat_clash_stage/contact_sheet.png and summary.json.
- 2026-07-05: 2026-07-05: Added second running-combat evidence for mill encounter. Added prepare_mill_combat_running DevAPI scenario and fake test. Verification rerun: py -3.12 games/rb-dark-rpg/devapi/scenarios_test.py (7 tests), cmake --build games/rb-dark-rpg/build/native-debug --target game_combat_test first_scene_tests game, and responsive_viewports --audit for tmp/quality/rb_dark_rpg_combat_mill_clash_stage at desktop=960x540 and phone=390x844. Both gate and mill running captures now show combat/stage, combat/actor_sprite, combat/impact, combat/stats_summary, and combat/log.
