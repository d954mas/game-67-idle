---
type: Game Knowledge
title: V0 Concept Idea
description: First durable concept capture distilled from the user-provided draft.
tags: [game-knowledge, concept, idea, v0]
game_id: rb-dark-rpg
status: draft
source: sources/v0_concept_gdd_draft.md
---

# V0 Concept Idea

Source: [v0_concept_gdd_draft.md](sources/v0_concept_gdd_draft.md)

Status: draft capture. This page records the current idea direction; it is not a
final implementation handoff.

## Core Pitch

`Дракон не вернулся` is a single-player FITGAME-like browser RPG about a world
whose ancient protector has disappeared. The player is not the lost protector or
chosen hero, but an ordinary mercenary/searcher who takes practical contracts
and accidentally becomes a witness to a larger conspiracy.

The jam version is `Акт I: Лишний свидетель`: a complete first act, not the full
dragon mystery.

## Theme

- Jam theme: `Мир без тебя`.
- Interpretation: the missing `you` is the Great Dragon, not the player.
- Main pillar: a world without its great protector is becoming dangerous again.
- Tone: dark fantasy, readable and stylized rather than bleak realism.

## Format

- 2D illustrated browser RPG, not a free-roaming 3D world.
- Screen flow: hub -> map -> location -> autobattle -> reward -> location -> hub.
- Hub is an illustrated interface location with clickable objects.
- Map is a fixed network of zones with visible locked future areas.
- Journal is the main navigation system for quests and clues.

## First Playable Direction

The first playable should start in `Последний Пост` and teach systems through
quests, not tutorial overlays.

Initial flow:

1. `Допуск за ворота`: get equipment, equip sword/armor, pass a simple gate
   fight, receive seeker token.
2. `Хлеб для Поста`: open map, go to `Старая мельница`, clear enemies, recover
   grain, receive reward.
3. Return with the first strange clue from the mill basement, opening the clue
   journal and the larger question `Где Дракон?`.

## Combat V0

- Pure autobattle for v1.
- No active skills, mana, or complex combat UI at the start.
- Minimal stats: HP, Damage, Armor, Attack Speed, Crit Chance.
- Basic damage rule: damage is attacker damage minus defender armor, floored at
  1; crit doubles damage.
- Player choice lives around preparation, gear, route, quest, and risk, not
  moment-to-moment combat input.

## Required Jam Priorities

1. Node map.
2. One hub.
3. First mill contract.
4. Pure autobattle.
5. Loot and basic progression.
6. Quest journal.
7. Clue journal.
8. Final clue in cult hideout.
9. Hook for continuation.

## Deferred / Avoid

- PvP, clans, MMO/social systems, rankings, and monetization currencies.
- Active skills, mana, complex factions, large crafting, open world, long
  cutscenes, many endings.
- Too many systems in the first minutes.
- Long grind before the first story hook.

## Current Open Decisions

- Whether the repo/game folder keeps the working id `rb-dark-rpg` while the
  player-facing title becomes `Дракон не вернулся`.
- Exact first screen composition for `Последний Пост`.
- Minimal data schema for quests, contracts, combat encounters, loot, and clues.
- Whether first task implementation starts with hub screen, map screen, combat
  simulator, or journal scaffold.
