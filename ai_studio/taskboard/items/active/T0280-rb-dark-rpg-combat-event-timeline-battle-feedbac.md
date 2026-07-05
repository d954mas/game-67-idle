---
id: T0280
title: "rb-dark-rpg: combat event timeline, battle feedback, rewards polish"
status: done
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, combat, rewards, uiux, onboarding]
created: 2026-07-04
updated: 2026-07-05
---

## What

Build the first production-ready combat slice for rb-dark-rpg after the player
accepts the guard's first quest: deterministic autobattle, readable battle
feedback, reward resolution, and a visual pass that fits the accepted Roblox-like
dark fantasy direction.

Scope is intentionally narrow: one first-quest encounter flow, not the full
future combat system.

## Done when

- [x] Combat simulation exposes a real event timeline: actor, time, damage, hp
      after hit, crit/block flags, and deterministic result.
- [x] Running combat UI is driven by real events, not fake progress text: visible
      HP changes, current hit/crit/block feedback, compact readable log.
- [x] Rewards are granted exactly once, update quest progress, and are validated
      against content compatibility rules.
- [x] First-quest encounter can be started from the in-game flow and returns to a
      usable result/continue state.
- [x] Desktop 16:9 and phone portrait runtime captures pass visual review for
      readability, no overlap, and no heavy panel clutter.
- [x] At least two subagent reviews are run: combat mechanics/state review and
      battle UI/UX visual review; findings are either fixed or explicitly logged.
- [x] Tests/validation pass for combat, content compatibility, and affected
      onboarding UI.

## Open questions

- Resolved for this slice: first battle is pure autobattle. Active skills/boosts
  remain future scope.
- Resolved for this slice: result stays compact and lists XP, gold, and first item
  reward in one row.
- Resolved for this slice: no generated combat art yet; first battle uses current
  scene plus event-driven UI feedback until accepted sprites exist.

## Log

- 2026-07-05: Lead fixed the next slice: continue from accepted first hub,
  guard/quest/equipment direction into combat mechanics, battle visual feedback,
  and reward resolution. Created planning card before implementation.
- 2026-07-05: Implemented `game_combat_event_t` timeline in combat result and
  added tests proving ordered events, actor/damage/HP-after fields, and no events
  on zero-HP loss.
- 2026-07-05: Updated running combat UI to use real timeline events for HP, attack
  pips, central hit/crit/block marker, active damage badge, and compact log.
- 2026-07-05: Updated result UI to list item reward alongside XP/gold; rewards
  remain once-only through `encounter.<id>.win` compatibility IDs.
- 2026-07-05: Runtime evidence captured:
  `tmp/quality/rb_dark_rpg_combat_prefight/contact_sheet.png`,
  `tmp/quality/rb_dark_rpg_combat_running/contact_sheet.png`,
  `tmp/quality/rb_dark_rpg_combat_result/contact_sheet.png`, each with
  `summary.json` for 640x360 and 390x844.
- 2026-07-05: Subagent mechanics review found no must-fix blocker for first
  combat/reward integration; logged follow-ups for later encounters/q002
  compatibility/multi-reward tests.
- 2026-07-05: Subagent UI review found running modal heaviness and ambiguous
  damage badges as must-fix; fixed by phase-specific compact panel heights and
  single labeled active hit badge.
- 2026-07-05: Verification passed:
  `cmake --build games/rb-dark-rpg/build/native-debug --target game_combat_test first_scene_tests game`,
  `py -3.12 games/rb-dark-rpg/tools/validate_content_compatibility.py --game-dir games/rb-dark-rpg --warnings`,
  `git diff --check` (only existing CRLF normalization warnings).
