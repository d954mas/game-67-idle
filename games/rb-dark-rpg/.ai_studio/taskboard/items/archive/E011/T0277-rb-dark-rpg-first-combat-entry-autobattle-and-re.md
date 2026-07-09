---
id: T0277
title: "rb-dark-rpg: first combat entry, autobattle, and result flow"
status: done
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, onboarding, combat, uiux, state]
created: 2026-07-04
updated: 2026-07-05
---

## What

Implement the first combat flow for `rb-dark-rpg`:

`place/local threat -> pre-fight card -> focused pure autobattle -> result panel -> quest advancement`.

Scope is only the first gate check encounter `gate_scavenger` in the first-five-minutes slice. The UX contract is fixed in `games/rb-dark-rpg/design/combat_entry_ux_v1.md`, backed by `games/rb-dark-rpg/design/knowledge/sources/legend_combat_entry_reference_2026-07-04.md`.

Design constraints:

- combat starts from the current place or visible local threat, not from the bottom nav;
- bottom nav may open `Место`, but it must not directly start combat;
- pre-fight must show enemy, player readiness, threat label, reason line, rewards, `В бой`, and `Назад`;
- combat v1 is pure autobattle: no active skills, no manual block, no hit zones, no spells, no consumable timing;
- result panel must grant rewards and advance `clear_gate_scavenger`.

## Done when

- [x] `gate_scavenger` can be started from the gate/place context after starter gear is available.
- [x] Pre-fight UI shows enemy stats, player HP/damage, threat label, reason line, rewards, `В бой`, and `Назад`.
- [x] Pressing `В бой` opens a focused autobattle screen and runs the fight automatically.
- [x] Combat view shows two HP bars, simple attack progress, damage feedback, and at most three log lines.
- [x] Victory grants configured XP/gold/items once and advances the quest objective.
- [x] Loss returns the player with the configured recovery behavior and a clear reason line.
- [x] Repeat-start/reward duplication is prevented after the encounter is resolved.
- [x] Unit or integration coverage proves start, deterministic win, reward grant, and quest advancement.
- [x] Native build succeeds and one visual smoke pass verifies the first combat flow on the main 16:9 layout.

## Open questions

- Should the first entry surface be a scene hotspot, the `Место` sheet, or both? Default implementation can support both but should keep the visible first route simple.
- Should result action copy be `Вернуться к стражу` or generic `Продолжить`? Use `Вернуться к стражу` when the guard dialogue is the next required step.

## Log

- 2026-07-04: Task created from accepted art/design lead decision. Legend reference was used for combat-entry ritual only; rb-dark-rpg keeps pure autobattle and avoids Legend active combat controls.
- 2026-07-04: 2026-07-05: Implementation started. First increment: inspect current combat/reward/content/UI state, then add tested battle and reward mechanics before visual polish.
- 2026-07-05: Completed first combat slice. Evidence: `cmake --build games/rb-dark-rpg/build/native-debug --target first_scene_tests game_combat_test game_dialogue_test scene_interactions_test game`; `cmake --build games/rb-dark-rpg/build/native-debug --target quality_responsive`; combat prefight screenshots in `tmp/quality/rb_dark_rpg_combat_prefight/`; combat result screenshots in `tmp/quality/rb_dark_rpg_combat_result/`; `git diff --check`; `py -3.12 games/rb-dark-rpg/tools/validate_content_compatibility.py --game-dir games/rb-dark-rpg --warnings`.
