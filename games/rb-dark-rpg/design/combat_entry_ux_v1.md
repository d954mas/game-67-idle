---
type: Game Design Specification
title: RB Dark RPG Combat Entry UX V1
description: First-slice UX contract for entering, watching, and resolving pure autobattle.
tags: [gdd, combat, uiux, onboarding, autobattle]
game_id: rb-dark-rpg
status: locked
---

# Combat Entry UX V1

Status: locked on 2026-07-05.

This document fixes the first combat flow for `rb-dark-rpg`.

Reference basis:

- Source note:
  `knowledge/sources/legend_combat_entry_reference_2026-07-04.md`.
- Combat mechanics:
  `combat_mechanics.md`.
- Structured combat data:
  `data/combat.json`.

## Accepted Direction

`rb-dark-rpg` uses pure autobattle in v1, but the player must knowingly start a
specific encounter. The game borrows the old-browser-RPG ritual from
`Legend: Legacy of the Dragons`: local place -> target/enemy -> fight screen.

The bottom navigation never starts a fight directly. Bottom navigation opens
sections such as equipment, journal, map, and current place. Combat starts from
the current place or a visible local threat.

## Locked Combat Entry Rules

- The player reaches combat through world context: scene hotspot, current place,
  map location, or quest-linked threat.
- The bottom `Место`/place nav button is a discovery route, not a battle button.
  It may show local objects and threats; selecting a threat opens pre-fight.
- Every combat start has a pre-fight confirmation. No accidental combat from a
  single bottom-nav tap.
- The pre-fight card is the only place with the `В бой` action.
- After `В бой`, the fight is hands-off autobattle until result.
- During pre-fight, running combat, and result, hub hotspots and bottom nav must
  not process click-through input.
- First fight may route back to the guard with specific copy. Later fights use
  neutral continuation copy unless authored quest data provides a safe next step.

## First Fight

Encounter: `gate_scavenger`.

Context: after the player talks to the gate guard and equips starter gear, the
quest objective points to the gate threat.

Player-facing framing:

- location: `Последний Пост`, gate area;
- target: `Падальщик у ворот`;
- purpose: prove the seeker can handle himself before the guard allows passage;
- mode: automatic fight;
- expected result: short safe tutorial fight with visible reward.

## Flow

1. The quest objective changes to `Разобраться с падальщиком у ворот`.
2. The `Место` section or scene hotspot exposes `Падальщик у ворот`.
3. Selecting the threat opens the pre-fight card.
4. The pre-fight card shows readiness and a single primary action: `В бой`.
5. Pressing `В бой` opens the focused autobattle screen.
6. The fight runs automatically.
7. The result panel grants rewards and advances the quest.
8. The player returns to the guard / gate flow.

## Pre-Fight Card

Required fields:

- enemy portrait or silhouette;
- enemy name;
- enemy HP and damage;
- player HP and final damage;
- threat label: `Легко`, `Ровно`, `Риск`, or `Смертельно`;
- one reason line;
- rewards: XP, gold, item/clue when relevant;
- primary action: `В бой`;
- secondary action: `Назад`;
- optional action: `Лечиться` only when player HP makes the fight risky;
- optional action: `Снаряжение` only when required gear is missing or clearly
  weak.

Do not show exact win percentage in v1 outside debug.

First tutorial copy:

- title: `Падальщик у ворот`;
- threat: `Легко`;
- reason: `Снаряжение стража достаточно для этой проверки.`;
- mode line: `Бой пройдет автоматически.`;
- rewards: `8 XP`, `5 золота`;
- primary: `В бой`;
- secondary: `Назад`.

## Battle Screen

Required fields:

- player side with HP bar;
- enemy side with HP bar;
- small attack timers or strike pips;
- floating damage numbers;
- crit marker only when crit happens;
- block marker only when block happens;
- last three combat events maximum;
- no manual skills;
- no hit-zone buttons;
- no spell bar;
- no bottom-nav interaction while the result is unresolved.

The first gate fight target duration is 4-10 seconds.

## Result Panel

Win:

- title: `Победа`;
- show gained XP and gold;
- show any quest item/clue;
- show player remaining HP;
- action: `Вернуться к стражу` or `Продолжить`.

Loss:

- title: `Поражение`;
- no gold or item loss in v1;
- player returns with 1 HP;
- show one reason line: low HP, weak weapon, low protection, enemy speed, or
  enemy crit;
- actions: `Лечиться`, `Снаряжение`, `Назад`.

## Implementation Plan

1. Data contract:
   ensure `locations.json`, `quests.json`, and `combat.json` all point to
   `gate_scavenger` and expose the pre-fight copy/rewards.
2. State/actions:
   add or verify encounter start, combat tick/resolve, win/loss reward grant,
   and quest advancement.
3. UI:
   implement `prefight -> autobattle -> result` as one focused flow.
4. Integration:
   wire the gate objective and `Место` / hotspot selection to pre-fight.
5. Validation:
   add unit coverage for start, deterministic win, rewards, quest advancement,
   and repeat-prevention.
6. Visual smoke:
   run the native game and capture the first fight flow on desktop layout.

## Deferred

- active skills;
- manual block or hit zones;
- spells;
- consumable timing;
- PvP/battlefield queue;
- auto-repeat farming;
- multi-enemy groups;
- exact Legend hunt-screen modal parity.
