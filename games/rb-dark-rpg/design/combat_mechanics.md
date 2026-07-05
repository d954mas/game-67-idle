---
type: Game Design Specification
title: RB Dark RPG Combat Mechanics
description: Full first-slice combat and character-stat model for RB Dark RPG.
tags: [gdd, combat, stats, balance]
game_id: rb-dark-rpg
status: draft
---

# Combat Mechanics: Дракон не вернулся

Status: draft

## Design Intent

Combat is automatic, but not mindless. The player wins by preparing: accepting
the right quest, equipping better gear, reading the threat label, healing before
danger, and choosing when to take a risky route.

The fixed reference direction is `Legend: Legacy of the Dragons`: character
stats and gear archetypes matter more than a visible armour-damage formula. Act
I keeps that model small and readable.

## Player Decision Model

The player makes decisions before combat:

1. Take or postpone a quest.
2. Equip a weapon and armour item.
3. Heal or risk fighting wounded.
4. Compare the enemy's threat label.
5. Start the fight, return to hub, or upgrade first.

During combat the player watches the result. No manual dodging, targeting,
active skills, mana, stamina, or consumable timing in v1.

## Visible Stats

| Stat | UI Name | Meaning | Why It Exists |
|---|---|---|---|
| `vitality` | Живучесть | Max HP. 1 Vitality = 1 HP. | Survival is clear and item upgrades matter immediately. |
| `strength` | Сила | Physical damage growth. 10 Strength = +1 average damage. | Character growth separate from weapon quality. |
| `protection` | Защита | Block chance against normal physical hits. | Armour feels defensive without a hidden mitigation curve. |
| `intuition` | Интуиция | Physical crit chance. Crit cannot be blocked. | Lets aggressive gear feel different. |
| `weapon_damage` | Урон оружия | Base weapon contribution to physical hits. | Weapon upgrades are obvious. |
| `attack_interval` | Скорость | Seconds between automatic attacks. Lower is faster. | Adds enemy variety without buttons. |

Main UI shows `HP`, `Сила`, `Защита`, `Интуиция`, and final `Урон`. Item cards
show direct deltas, for example `Живучесть +6`, `Защита +15`, `Урон оружия +4`.

## Derived Combat Values

Implementation can calculate these, but the player should see simple labels and
reason lines instead of formulas.

```text
max_hp = vitality
attack_power = weapon_damage + floor(strength / 10) + bonus_attack_power
crit_chance = min(0.30, intuition / 100)
block_chance = min(0.35, protection / 100)
normal_hit = attack_power
blocked_hit = max(1, ceil(normal_hit * 0.5))
crit_hit = normal_hit * 2
```

Resolution order:

1. Attacker's attack timer reaches `attack_interval`.
2. Calculate `attack_power`.
3. Roll crit from attacker's `intuition`.
4. If crit succeeds, deal `crit_hit`; this hit cannot be blocked.
5. If crit fails, roll defender block from `protection`.
6. If block succeeds, deal `blocked_hit`; otherwise deal `normal_hit`.
7. Subtract damage from current HP.

No random damage range in v1. Randomness comes only from crit and block.
Opening attacks use short timer offsets so the first visible hit lands before a
full attack interval. If both combatants would resolve on the same timestamp,
the player event resolves first and the next event is delayed by a short combat
beat instead of being logged as a simultaneous exchange.

## Why No Armour Curve

`Legend: Legacy of the Dragons` exposes `Protection` as block chance, not as a
flat damage reduction or PoE-style armour formula. For this game, armour items
are the source of `Protection` and `Vitality`; they do not own a separate hidden
mitigation curve.

This gives three clear early identities:

| Archetype | Stat Pair | Meaning |
|---|---|---|
| Balanced seeker | Strength + Vitality | reliable starter growth |
| Heavy guard | Protection + Vitality | survives through HP and blocks |
| Bonebreaker | Strength + Intuition | crit-heavy attacker |

Dodger-style `Agility` exists in the reference, but is deferred because dodge
adds another avoidance roll and makes short autobattles harder to read.

## First-Slice Character Model

Base seeker before starter gear:

| Stat | Value |
|---|---:|
| Vitality / HP | 24 |
| Strength | 20 |
| Protection | 0 |
| Intuition | 3 |
| Weapon damage | 0 |
| Attack power | 2 |
| Attack interval | 2.1 s |

Starter equipped seeker:

| Source | Vitality | Strength | Protection | Intuition | Weapon Damage | Attack Power |
|---|---:|---:|---:|---:|---:|---:|
| Base seeker | 24 | 20 | 0 | 3 | 0 | 2 |
| Old sword | 0 | 0 | 0 | 0 | +4 | +4 |
| Padded jacket | +6 | 0 | +15 | 0 | 0 | 0 |
| Total | 30 | 20 | 15 | 3 | 4 | 6 |

## Gear Rules

Weapons:

- primary stat: `weapon_damage`;
- later variants can add `intuition` or faster `attack_interval`;
- a weapon upgrade must reduce hits-to-kill against at least one early enemy.

Armour:

- primary stats: `vitality` and `protection`;
- no speed penalty in v1;
- an armour upgrade must increase hits-survived or noticeably add blocked hits.

Charms:

- locked until after the mill clue;
- first use: small `intuition` or story utility;
- no charm in the first combat tutorial.

## First Enemy Set

| Enemy | Role | Vitality | Attack Power | Protection | Intuition | Speed | Expected Feeling |
|---|---|---:|---:|---:|---:|---:|---|
| Gate scavenger | tutorial check | 20 | 4 | 0 | 0 | 2.2 s | clearly safe after starter gear |
| Mill scavenger | first contract enemy | 26 | 5 | 5 | 0 | 2.1 s | fair if healed |
| Mill brute | first protection lesson | 34 | 6 | 20 | 0 | 2.5 s | risky without better weapon |
| Black Sun runner | first speed lesson | 25 | 5 | 0 | 5 | 1.6 s | fragile but scary because fast |
| Night attacker | first human threat | 35 | 6 | 10 | 8 | 2.0 s | tuned for upgraded weapon |

## Threat Labels

Pre-fight UI must show one label and one reason line.

| Label | Internal Rule | Player Meaning |
|---|---|---|
| `Легко` | predicted win chance about 80 percent or higher | Safe fight, good tutorial. |
| `Ровно` | about 60-80 percent | Winnable, gear and HP matter. |
| `Риск` | about 40-60 percent | Can win, but should heal or upgrade. |
| `Смертельно` | below about 40 percent | The player is probably underprepared. |

Reason line examples:

- `У тебя достаточно живучести для этой встречи.`
- `Его защита часто режет обычные удары.`
- `Тебе не хватает силы или оружия.`
- `Враг бьет чаще тебя.`
- `У врага высокий шанс крита.`

Do not show exact percentages in v1 unless debug mode is enabled.

## Leveling

Leveling exists, but gear does the visible work in the first slice.

```text
level 1 -> 2: requires 20 XP, grants +5 Vitality and +10 Strength
level 2 -> 3: requires 45 total XP, grants +5 Vitality and +5 Protection
```

Do not require grinding levels to clear mandatory story fights. Mandatory fights
should be passable with quest gear, healing, and one obvious purchase.

## Healing, Loss, And Recovery

Win:

- enemy is defeated;
- player keeps remaining HP;
- rewards appear immediately;
- quest or clue state updates.

Loss:

- player returns to `Последний Пост` with 1 HP;
- no gold or item loss in v1;
- current encounter progress resets;
- UI explains the mismatch: low HP, low Strength/weapon damage, low Protection,
  enemy speed, or enemy crit.

Retreat:

- available before combat starts;
- not available after combat starts in v1.

Healing:

- first tutorial heal can be free;
- later basic heal costs 3-5 gold or uses a quest-provided ration;
- healing must be available before any mandatory risky fight.

## Encounter Pacing

| Encounter Type | Target Duration |
|---|---:|
| First gate check | 4-10 seconds |
| Normal mill fight | 6-16 seconds |
| Stat lesson enemy | 8-18 seconds |
| Act I warning fight | 8-18 seconds |

Early fights should not exceed 30 seconds unless a new visual event or clue is
attached to the fight.

## UI Requirements

Pre-fight panel:

- enemy portrait/name;
- player HP, Strength, Protection, Intuition, final Damage;
- enemy HP, Damage, Protection, Intuition;
- threat label;
- one reason line;
- expected rewards;
- buttons: `В бой`, `Назад`, `Лечиться`, `Улучшить` when relevant.

Combat view:

- two HP bars;
- simple attack pips or swing timers;
- floating damage numbers;
- crit marker only when it happens;
- block marker only when it happens;
- last 3 combat events maximum;
- result panel with reward and next quest/clue update.

## Deferred Complexity

Do not add these to Act I first slice:

- dodge / Agility;
- hit chance / accuracy;
- elemental damage;
- resistances;
- mana;
- stamina;
- morale;
- stress;
- injuries;
- lifesteal;
- status-effect stacks;
- active super-blow input;
- party formation;
- pets;
- PvP;
- idle/offline reward loops.

These can be reconsidered only after the hub -> quest -> fight -> reward ->
clue loop is playable.

## Reference Basis

The model is based on the local v0 draft plus source notes in
`knowledge/sources/combat_stats_reference_packet_2026-07-04.md`, especially the
official `Legend: Legacy of the Dragons` Character Attributes reference.
