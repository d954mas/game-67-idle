---
id: T0281
title: "rb-dark-rpg: later encounter rewards compatibility and multi-reward tests"
status: done
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, combat, rewards, compatibility, tests]
created: 2026-07-04
updated: 2026-07-05
---

## What

Close the follow-up risks from T0280 mechanics review before later encounters
are exposed to players: stable persistent IDs for q002 authored steps and tests
for multi-item encounter rewards.

This slice does not make q002 combat-complete. It keeps late encounter quest
advancement deferred until q002 steps are authored.

## Done when

- [x] q002 authored step IDs are locked in content compatibility so save data can
      survive future content updates.
- [x] Multi-item encounter rewards are covered by a test: all reward items grant,
      XP/gold grant, claimed reward ID is recorded, and repeat resolution does
      not duplicate rewards.
- [x] Existing first combat/reward tests still pass.
- [x] Content compatibility validation passes.
- [x] Subagent review checks the compatibility/reward coverage.

## Open questions

- Later q002 combat objectives are not authored yet. Do not invent them in this
  task; create a later task when q002 flow is designed.

## Log

- 2026-07-05: Started after T0280 mechanics review. Scope is stable q002 step
  IDs and multi-reward encounter test coverage, not new q002 quest design.
- 2026-07-05: Added `visit_old_mill` and `inspect_old_mill` to stable
  `quest_steps`.
- 2026-07-05: Added combat regression coverage for `mill_scavenger`: q002
  visit/inspect path completes through game actions, multi-item reward grants
  stack + gear, XP/gold, claimed reward ID, and repeat resolution grants nothing.
- 2026-07-05: Updated combat result reward text to list all encounter reward
  items instead of only the first item.
- 2026-07-05: Captured multi-reward result evidence:
  `tmp/quality/rb_dark_rpg_mill_combat_result/contact_sheet.png` and
  `summary.json` for 640x360 and 390x844.
- 2026-07-05: Subagent review accepted the slice; low follow-up about real q002
  path was addressed by adding `game_actions_move_location` and
  `game_actions_inspect_object` coverage before mill combat resolution.
- 2026-07-05: Verification passed:
  `cmake --build games/rb-dark-rpg/build/native-debug --target game_combat_test first_scene_tests game`,
  `py -3.12 games/rb-dark-rpg/tools/validate_content_compatibility.py --game-dir games/rb-dark-rpg --warnings`,
  `git diff --check` (only existing CRLF normalization warnings).
