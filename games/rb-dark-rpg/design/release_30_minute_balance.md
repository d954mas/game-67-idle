---
type: Game Design Specification
title: RB Dark RPG 30 Minute Release Balance
description: Release-scope pacing, levels, quests, gear tiers, enemies, and visual proof target for a 30-minute Act I build.
tags: [gdd, balance, release, content, visual-proof]
game_id: rb-dark-rpg
status: draft
---

# 30 Minute Release Balance

Status: draft

## Definition Of Done

This balance pass is done when the release target has:

- 30 minutes of directed play without mandatory grinding;
- 10 character levels, with level 10 reached at the Act I finale;
- 10+ authored quests, split into critical path and optional support quests;
- 5 readable gear and weapon tiers;
- 10+ distinct enemies with clear stat lessons;
- a visual proof target showing how the player reads level, quest, enemy, gear tier, reward, and next action in one gameplay screen.

Runtime implementation, final art production, and full expansion of
`quests.json`, `combat.json`, `items.json`, `locations.json`, and
`dialogues.json` are the next gate. This pass creates the accepted balance
contract first.

## Release Shape

The 30-minute build is Act I: the player becomes an unwanted witness while doing
small survival contracts for the Last Post. The session should feel like compact
browser/mobile RPG play: short hub actions, short fights, frequent quest state
updates, and visible gear upgrades.

Core pacing rule:

```text
quest beat -> one clear player action -> short autobattle or inspection -> reward -> gear/level change -> next location
```

Mandatory story content should carry the player to level 10. Optional quests can
replace a missed fight, pay for alternate gear, or make a risky encounter safer,
but they must not be required.

## 30 Minute Pacing

| Time | Target level | Quest | Primary action | Main reward | Gear tier | Enemy lesson |
|---:|---:|---|---|---|---|---|
| 0-2 min | 1 -> 2 | `q001_gate_pass` | Talk, receive gear, equip, win gate check | 20 XP, 5 gold, seeker token | T1 | first safe fight |
| 2-5 min | 2 -> 3 | `q002_bread_for_post` | Open map, reach Old Mill, clear yard | 44 XP, 16 gold, first drops | T1/T2 | fair fight and protection |
| 5-8 min | 3 -> 4 | `q003_sign_in_cellar` | Inspect hidden mark, collect clue | 42 XP, clue, relic unlock | T2 | fast fragile enemy |
| 8-11 min | 4 -> 5 | `q004_do_not_show_anyone` | Decide who sees the clue, report carefully | 45 XP, shop refresh | T2/T3 | no fight or one light fight |
| 11-14 min | 5 | `q005_night_visitors` | Defend the witness at night | 52 XP, order scrap, weapon drop | T3 | human crit threat |
| 14-17 min | 5 -> 6 | `q006_forest_road` | Follow the cargo trail | 55 XP, road location | T3 | speed pressure |
| 17-20 min | 6 -> 7 | `q007_ash_convoy` | Recover moved grain and chain evidence | 58 XP, tier 4 shop seed | T3/T4 | protected brute |
| 20-23 min | 7 -> 8 | `q008_scribes_seal` | Convert evidence into official permission | 60 XP, council pass | T4 | social/report beat |
| 23-27 min | 8 -> 9 | `q009_black_sun_hideout` | Enter hideout, win two short fights | 70 XP, finale unlock | T4 | mixed enemies |
| 27-30 min | 9 -> 10 | `q010_extra_witness_finale` | Survive captain and expose Act I proof | 94 XP, tier 5 proof item | T5 | boss-style readable threat |

Optional quests:

| Quest | Unlock | Target | Reward role |
|---|---|---|---|
| `q011_healer_debt` | after `q002` | 3-6 min side beat | Free heal token and 20 XP substitute. |
| `q012_blacksmith_chain_test` | after `q003` | 7-12 min side beat | Discount on one T2/T3 gear piece and 25 XP. |
| `q013_missing_cart` | after `q006` | 16-22 min side beat | Gold burst, one T4 component, and 30 XP. |

## Level Curve

Total XP is cumulative. Mandatory critical-path rewards add up to 540 XP, so the
player reaches level 10 at the finale without grinding.

| Level | Total XP | Base Vitality | Base Strength | Base Protection | Base Intuition | Purpose |
|---:|---:|---:|---:|---:|---:|---|
| 1 | 0 | 24 | 20 | 0 | 3 | fragile seeker |
| 2 | 20 | 29 | 30 | 0 | 3 | first weapon scaling visible |
| 3 | 50 | 34 | 30 | 5 | 3 | first survival bump |
| 4 | 90 | 39 | 40 | 5 | 4 | damage improves before T3 |
| 5 | 140 | 45 | 45 | 8 | 5 | night attack baseline |
| 6 | 200 | 51 | 55 | 10 | 6 | road fights become fair |
| 7 | 270 | 58 | 60 | 13 | 8 | T4 gear unlock band |
| 8 | 350 | 65 | 70 | 15 | 10 | hideout entry baseline |
| 9 | 440 | 72 | 75 | 18 | 12 | finale prep |
| 10 | 540 | 80 | 85 | 20 | 14 | Act I cap |

Tuning rule: level stats alone should not trivialize the next mandatory fight.
Gear remains the stronger visible upgrade, while levels confirm progress.

## Gear And Weapon Tiers

Each tier should contain at least one weapon, one armour item, one leg item, and
one utility/relic item once that slot is unlocked. Tier names are player-facing
themes; ids should stay ASCII.

| Tier | Level band | Player meaning | Weapon target | Armour target | Acquisition |
|---|---:|---|---|---|---|
| T1 Guard Castoff | 1 | permission to leave the hub | 4 weapon damage | +9 Vitality, +15 Protection across armour/legs | `q001` mandatory grant |
| T2 Post Militia | 2-3 | first bought upgrade | 7 damage or 6 damage + 10 Strength | +15-19 Vitality, +20-28 Protection across two pieces | Last Post shops after `q001` |
| T3 Mill Salvage | 4-5 | found gear with clue flavor | 9 damage or 7 damage + Intuition | protection-heavy old chain pieces | Old Mill drops and `q012` |
| T4 Roadwarden | 6-7 | reliable travel kit | 12 damage or 10 damage + 15 Strength | +25-30 Vitality, +30-34 Protection | road/convoy rewards and shop refresh |
| T5 Black Sun Proof | 8-10 | finale proof kit | 15 damage or 13 damage + Intuition | +34-40 Vitality, +32-35 Protection | hideout/finale rewards |

Do not add more equipment slots for the release target. Use `weapon`, `armour`,
`legs`, and `relic` so the UI stays readable on phone.

## Enemy Roster

The release target uses 12 enemies. Every enemy should teach one readable
difference: more HP, more block, faster timer, higher crit, or a reward/clue
reason to fight.

| Id | Level band | Role | Vitality | Attack | Protection | Intuition | Speed | Reward role |
|---|---:|---|---:|---:|---:|---:|---:|---|
| `gate_scavenger` | 1 | tutorial check | 20 | 4 | 0 | 0 | 2.2 | seeker token gate |
| `mill_scavenger` | 2 | first contract enemy | 26 | 5 | 5 | 0 | 2.1 | contract progress |
| `mill_brute` | 3 | protection lesson | 34 | 6 | 20 | 0 | 2.5 | grain and chain drop |
| `black_sun_runner` | 4 | speed lesson | 30 | 7 | 0 | 7 | 1.6 | clue fragment |
| `cellar_knifeman` | 4 | first clue defender | 32 | 7 | 8 | 8 | 1.9 | burned chain bracket |
| `night_attacker` | 5 | first human threat | 42 | 8 | 12 | 8 | 2.0 | order scrap |
| `road_cutthroat` | 6 | travel tax | 50 | 9 | 10 | 10 | 1.9 | gold and ration |
| `ash_trail_scout` | 6 | fast ambusher | 44 | 8 | 5 | 14 | 1.5 | route clue |
| `chain_bearer` | 7 | block-heavy brute | 60 | 10 | 25 | 6 | 2.4 | T4 component |
| `black_sun_acolyte` | 8 | mixed cultist | 58 | 11 | 15 | 16 | 1.8 | hideout key |
| `relic_warden` | 8-9 | tank check | 68 | 12 | 30 | 8 | 2.5 | T5 relic |
| `black_sun_captain` | 10 | Act I finale | 86 | 15 | 24 | 20 | 2.1 | Act I proof |

Balance guardrails:

- required fights are `easy` or `fair` with the intended tier and 70%+ HP;
- `risky` is allowed only when the UI points to healing or an obvious upgrade;
- no mandatory fight should depend on a crit roll;
- normal release fights target 6-18 seconds; the finale can reach 20-26 seconds
  if the result panel carries the Act I proof.

## Reward Budget

Critical path XP:

```text
q001 20 + q002 44 + q003 42 + q004 45 + q005 52
+ q006 55 + q007 58 + q008 60 + q009 70 + q010 94 = 540 XP
```

Gold target:

- by minute 5: 20-25 total gold earned, enough to buy one T2 piece;
- by minute 10: 40-50 total gold earned, enough for a weapon plus one armour piece;
- by minute 20: 85-105 total gold earned, enough to choose one T4 path;
- by finale: 120-140 total gold earned, with T5 mostly rewarded, not bought.

Healing:

- one free tutorial heal remains in `q001`;
- basic heal cost stays 5 gold until level 5;
- after level 6, advanced heal can cost 8 gold but should restore full HP;
- every mandatory `risky` pre-fight state must show `Heal` and `Upgrade`.

## Content Expansion Order

1. Accept or adjust this balance contract.
2. Expand `data/quests.json` to the 10 critical quests plus 3 optional quests.
3. Expand `data/combat.json` with the 12 enemy encounters and the level curve.
4. Expand `data/items.json` and `data/services.json` with T3-T5 gear.
5. Expand `data/locations.json`, `characters.json`, and `dialogues.json` only
   for the accepted quest beats.
6. Update the content editor so balance fields can be edited without raw JSON.

## Visual Target

The first release visual proof is
`design/layout/release_30min_fake_shot.svg`.

It should show a real gameplay state, not a poster:

- player level and HP;
- XP progress toward level 10;
- current quest and next action;
- current enemy and threat label;
- current gear tier;
- one visible reward;
- bottom navigation and phone-safe action density;
- enough map/environment context to show that the 30-minute arc is not only a
  spreadsheet.

## Open Risks

- The current runtime and authored JSON cover the first minutes, not the full
  release target.
- Existing content files already have uncommitted changes; expand them only
  after this contract is accepted.
- The current `combat.json` level curve stops at level 3; it must be migrated to
  the release curve after acceptance.
- Visual proof is a direction target. It does not replace sliced UI assets or
  engine-rendered text.
