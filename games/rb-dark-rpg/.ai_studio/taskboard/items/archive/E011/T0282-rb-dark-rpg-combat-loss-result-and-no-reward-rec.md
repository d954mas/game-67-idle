---
id: T0282
title: "rb-dark-rpg: combat loss result and no-reward recovery UX"
status: done
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, combat, rewards, uiux]
created: 2026-07-04
updated: 2026-07-04
---

## What

Close the combat loss path for `rb-dark-rpg` so the first combat loop has a
clear, RPG-readable failure state: the player can lose from positive HP, no
rewards or quest progress are granted, the hero returns with 1 HP, and the
result UI explains the next recovery action without turning the battle into a
manual-choice flow.

This slice keeps the accepted combat direction:

- Battle starts from the relevant world object/NPC, not from an extra hub button.
- First implementation is deterministic autobattle with readable event feedback.
- Rewards are granted only through stable reward ids and only on win.
- Loss is recoverable, not a run-ending fail screen.
- UI must work in the same responsive scene on desktop and phone.

## Done when

- [x] A positive-HP loss regression test proves that loss grants no reward,
      does not claim reward ids, does not advance quest state, and leaves the
      hero at the designed recovery HP.
- [x] Combat result UI has explicit loss copy: no reward was received, what
      happened, and the next useful action.
- [x] A DevAPI scenario can open the game directly into the combat loss result.
- [x] Visual evidence is captured for 16:9 and phone viewports.
- [x] Existing combat, first-scene, and content compatibility checks still pass.
- [x] A subagent review covers both combat-state correctness and battle UX.

## Open questions

- Resolved for v1: defeat returns to `hub_last_post` with 1 HP and points to
  restoring HP or checking equipment; no healer dependency is required yet.
- Resolved for v1: every defeat result explicitly says that no reward was
  received and quest progress did not change.

## Log

- 2026-07-04: Created as the next narrow combat/reward slice after win,
  timeline, and multi-reward paths were implemented.
- 2026-07-04: Planning locked: use T0282 as the single combat loss/no-reward recovery slice; removed duplicate T0283.
- 2026-07-04: Added positive-HP loss regression in
  `games/rb-dark-rpg/tests/test_game_combat.c`: loss records events, grants no
  XP/gold/items/reward id, does not advance the quest/flag, returns to
  `hub_last_post`, and leaves hero HP at 1.
- 2026-07-04: Updated `game_actions_resolve_encounter` defeat behavior to
  return the player to `hub_last_post` with recovery HP while keeping rewards
  and quest advancement win-only.
- 2026-07-04: Updated combat result copy and button text for defeat in
  `games/rb-dark-rpg/src/ui/combat_flow.c`.
- 2026-07-04: Added `prepare_combat_loss_result` DevAPI scenario with state
  assertions for HP, XP, gold, reward ids, completed steps, flags, current
  location, and quest step.
- 2026-07-04: Captured loss result evidence:
  `tmp/quality/rb_dark_rpg_combat_loss/contact_sheet.png` and `summary.json`.
- 2026-07-04: Fixed generated location content compatibility with current
  `game_location_object_t` interaction arrays and updated world-place UI to use
  `object->interactions[0]`.
- 2026-07-04: Verification passed:
  `cmake --build games/rb-dark-rpg/build/native-debug --target game_combat_test first_scene_tests game`;
  `py -3.12 games/rb-dark-rpg/tools/validate_content_compatibility.py --game-dir games/rb-dark-rpg --warnings`;
  `py -3.12 games/rb-dark-rpg/devapi/scenarios_test.py`;
  `git diff --check` with only existing CRLF warnings.
- 2026-07-04: Subagent review passed: combat-state/reward correctness PASS;
  battle UX/visual evidence PASS after adding stronger DevAPI no-reward
  assertions.
- 2026-07-04: Done: positive-HP loss path, no-reward recovery UX, DevAPI loss scenario, responsive evidence, checks, and subagent reviews completed.
