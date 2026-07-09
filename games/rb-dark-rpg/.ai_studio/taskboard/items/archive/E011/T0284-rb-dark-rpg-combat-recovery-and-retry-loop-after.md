---
id: T0284
title: "rb-dark-rpg: combat recovery and retry loop after defeat"
status: done
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, combat, recovery, uiux]
created: 2026-07-04
updated: 2026-07-04
---

## What

Add the first playable recovery loop after combat defeat. After losing a fight
the player already returns to `hub_last_post` with 1 HP; this slice makes the
next useful action real: recover HP through the Last Post healer/service path
and retry the same combat without broken rewards or quest progress.

Scope is intentionally v1:

- no paid healing economy yet;
- no inventory consumables yet;
- one clear recovery action from the hub/place UI;
- state tests prove defeat can be followed by recovery and a later win.

## Done when

- [x] A state/action test proves healing from 1 HP restores the hero to a
      valid combat-ready HP value without changing quest/reward state.
- [x] A regression test proves loss -> heal -> retry can then win the first
      encounter and grants the win rewards once.
- [x] The hub/place UI exposes the healer interaction as a clear recover action.
- [x] A DevAPI scenario captures the recovery/retry path or the recovered hub
      state after defeat.
- [x] Existing combat, first-scene, scenario, and content checks pass.
- [x] Subagent review covers recovery mechanics and UX.

## Open questions

- Resolved for v1: healer restores to current equipped max HP.
- Resolved for v1: healer is free until the economy/cost slice is implemented.

## Log

- 2026-07-04: Created after T0282 closed the defeat result/no-reward path.
- 2026-07-04: Planning locked: add minimal free recovery/retry loop after combat defeat.
- 2026-07-04: Added `game_actions_restore_hp`, restoring hero HP to max HP
  derived from current equipped combat stats.
- 2026-07-04: Added combat tests for heal-without-rewards and
  loss -> heal -> retry -> win with reward idempotency.
- 2026-07-04: Hooked `healer` world-place interaction to HP restore and
  changed world-place action labels from raw ids to player-readable labels.
- 2026-07-04: Fixed top HUD to show real `hero_hp`, equipped max HP, and
  `wallet_gold` instead of hardcoded placeholder values.
- 2026-07-04: Added `prepare_combat_loss_recovered_hub` DevAPI scenario and
  captured evidence at `tmp/quality/rb_dark_rpg_combat_recovery/contact_sheet.png`
  and `summary.json`.
- 2026-07-04: Verification passed:
  `cmake --build games/rb-dark-rpg/build/native-debug --target game_combat_test first_scene_tests game`;
  `py -3.12 games/rb-dark-rpg/tools/validate_content_compatibility.py --game-dir games/rb-dark-rpg --warnings`;
  `py -3.12 games/rb-dark-rpg/devapi/scenarios_test.py`;
  `git diff --check` with only existing CRLF warnings.
- 2026-07-04: Subagent review PASS: no blocking or warning findings for
  recovery mechanics, retry, healer UI, HUD, or evidence.
- 2026-07-04: Done: free healer recovery, retry win regression, HUD real HP/gold, DevAPI recovery evidence, checks, and subagent review completed.
