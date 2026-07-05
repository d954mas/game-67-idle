---
type: Game Design Document
title: RB Dark RPG GDD
description: Implementation-facing game design document for RB Dark RPG.
tags: [gdd]
game_id: rb-dark-rpg
status: draft
---

# RB Dark RPG GDD: Дракон не вернулся

Status: draft

## Definition Of Done

This design pass is done when the concept, GDD, private knowledge index,
structured loop/UI/combat/quest data, combat mechanics spec, and quest system
spec agree on the same player-facing title, Act I premise, core screen flow,
first locations, first autobattle model, first quest state model, and open
narrative decisions. Runtime implementation, final art, and first-5-minute task
execution are out of scope.

## Player Fantasy

You are an ordinary seeker in the Ash Border. You take contracts because food,
gear, and safety cost money. A simple mill job turns you into the extra witness
to a secret about why the Great Dragon never returned.

## First 30 Seconds

The player lands in the illustrated hub `Последний Пост`. The screen shows a
locked gate, a gate guard, a dim Dragon memorial or beacon, and a first quest
prompt for `Допуск за ворота`. The player clicks the active quest/NPC and takes
the first concrete action without reading a lore dump.

## Core Loop

See `data/core_loop.json`.

## Quest System

See `quest_system.md` and `data/quests.json`.

Quest definitions are static authored content. Runtime save state should store
only quest ids, status, current step, completed steps, objective progress,
flags, and choices. The first quest is `Допуск за ворота`: talk to the gate
guard, receive starter gear, equip it, win the gate encounter, return for the
seeker token, then unlock the map and `Хлеб для Поста`.

## Content Authoring

See `content_model.md`, `content_editor_spec.md`, and
`data/content_manifest.json`.

The fixed direction is a local web content platform as the primary authoring
tool. The game engine should consume the same JSON configs and expose preview
hooks such as opening a location, dialogue, quest step, item, or encounter by
id. The engine should not become the main editor for quests/dialogues/content.

## Player Verbs

- Inspect hub objects and NPCs
- Accept or track a quest
- Equip basic gear
- Choose a map location or marker
- Start autobattle
- Collect reward or loot
- Record a clue
- Return, report, recover, or upgrade

## Rules And Feedback

The first playable slice should prove hub navigation, quest-driven onboarding,
basic equipment, one simple autobattle, reward feedback, and the first clue
journal update. Every failure or blocked state must explain what is missing:
gear, health, quest access, map unlock, or unresolved clue.

Combat v1 is pure autobattle. No active skills, mana, PvP, clans, rankings, or
MMO currencies in the jam act.

See `combat_mechanics.md` for the full stat/combat design and `data/combat.json`
for the structured first autobattle contract. The fixed stat direction follows
`Legend: Legacy of the Dragons`: `Сила` scales physical damage, `Живучесть` is
HP, `Защита` is block chance, and `Интуиция` is crit chance. Armour items grant
stats; they are not a separate hidden mitigation curve.

## Story And World

See
`knowledge/story_world_lore_digest.md`.

Current canon:

- Title: `Дракон не вернулся`
- Subtitle/theme: `Мир без Дракона`
- Jam act: `Акт I: Лишний свидетель`
- World area: `Пепельное Пограничье`
- Hub: `Последний Пост`
- First onboarding quest: `Допуск за ворота`
- First contract: `Хлеб для Поста`
- First hidden threat: `Культ Чёрного Солнца`

## Act I Arc

1. `Допуск за ворота`: get basic gear, equip, pass a simple gate fight, earn a
   seeker token.
2. `Хлеб для Поста`: open the map, travel to `Старая мельница`, clear enemies,
   recover grain.
3. Mill basement clue: find Black Sun signs, cut grain sacks, a dead cultist, a
   burned chain bracket, and an order scrap.
4. `Не показывай это никому`: return to Last Post; officials react with fear.
5. `Те, кто пришёл ночью`: someone tries to take the clue or remove the witness.
6. `Лесная дорога`: follow the moved cargo trail.
7. `Тайник Чёрного Солнца`: find proof that the Dragon's disappearance was
   prepared.

## Locations

- `Последний Пост`: one illustrated hub with gate guard, contract board,
  blacksmith, healer, council scribe, Dragon memorial/beacon, and map gate.
- `Старая мельница`: first contract location; grain shortage plus hidden cult
  basement.
- `Заброшенная дорога`: ambush/transport trail.
- `Лесная опушка`: route confirmation and future-map promise.
- `Тайник Чёрного Солнца`: Act I finale and continuation hook.
- Locked future zones: `Чёрная шахта`, `Погасший маяк`, `Вампирская дорога`,
  `Башня наблюдателей`, `Святилище Чёрного Солнца`.

## Character Cast

- Player seeker: practical, low-status, not chosen.
- Gate guard: first onboarding, permission boundary, proof that leaving town is
  now dangerous.
- Староста: first contract giver; wants grain before mystery.
- Писарь Совета: validates reports and makes the public Dragon search feel
  bureaucratic.
- Кузнец: equipment tutorial and first clue reader for the chain bracket.
- Лекарь: recovery and visible consequences of new dangers.
- Missing miller / dead seeker: human cost and first investigation trail.
- Night attacker: proves the clue is dangerous.
- Great Dragon: absent protector; no full answer in Act I.

## UI Flow

See `data/ui_flow.json`.

## Assets And Visual Proof

See `data/asset_manifest.json`.

## Validation

- Design docs agree on `Дракон не вернулся`, `Акт I: Лишний свидетель`, and
  hub -> map -> location -> autobattle -> reward -> clue/journal -> hub.
- `combat_mechanics.md` and `data/combat.json` keep first combat to pure
  autobattle with no active skills, no mana, no dodge/accuracy, no armour
  mitigation curve, and no status-effect stack.
- `quest_system.md` and `data/quests.json` define static quest content,
  runtime quest state shape, and the first quest `Допуск за ворота`.
- `content_model.md`, `content_editor_spec.md`, and `data/content_manifest.json`
  define the web-editor-first content platform direction and modular data files.
- Reference lessons are linked from `knowledge/index.md`.
- Later visual proof needs an accepted `Последний Пост` hub mock/fake shot.
- Later gameplay proof needs a capture of quest journal -> map -> old mill ->
  autobattle result -> clue journal update.
