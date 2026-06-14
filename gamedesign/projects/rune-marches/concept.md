---
type: Concept
title: Rune Marches Concept
description: Initial concept lock for a casual open-world fantasy RPG.
tags: [concept, rpg, casual, open-world, ftue]
timestamp: 2026-06-13T00:00:00Z
---

# Rune Marches Concept

## Definition Of Done For This Gate

Done for the first design gate means:

- a named original setting exists;
- the fantasy, audience, platform, pillars, and no-go list are explicit;
- named references are recorded with borrow/avoid/copy-risk;
- the first playable slice can be implemented without inventing core numbers;
- PC, web, and mobile constraints are reflected in the slice;
- follow-up implementation tasks exist in `tasks/`.

Out of scope for this gate:

- final art;
- full main quest;
- full world map;
- monetization;
- runtime implementation before the reference gate is strong enough.

Accepted proof:

- durable docs and valid JSON contracts under this project folder;
- taskboard validation;
- later native screenshot and input automation for the first playable slice.

## One-Line Pitch

A casual first-person fantasy RPG where the player explores a static authored
frontier, resolves tiny quests, grows a spellblade, and watches settlements
change through short, readable sessions.

## Working Title

`Rune Marches`

## Audience

- Casual PC and browser/mobile players who want the fantasy of a big RPG
  without long setup, complex controls, or dense inventory management.
- Poki-style test audience: fast first action, readable UI, short sessions,
  touch-friendly controls, low frustration, and clear progress.

## Platforms

- Primary build proof: native desktop PC.
- Target follow-up: web build with desktop browser and mobile portrait support.
- Inputs: mouse, touch, keyboard shortcuts only as optional accelerators.

## Player Fantasy

The player is a new Warden of the Rune Marches, a borderland where old road
stones wake up, small towns ask for help, and magic is practical craft rather
than destiny. The fantasy is not "chosen one saves the world" on minute one;
it is "I can step into the wild, solve a local problem, return stronger, and
see the map open."

## Setting

The Rune Marches are three valleys around a broken leyline called the Glass
Road. Every settlement was built around a rune-stone that once kept weather,
spirits, and trade routes stable. The stones are failing, so roads loop,
marshlights imitate travelers, and old watchtowers wake up.

First slice region: `Miregate`, a safe border hamlet, the adjacent
`Wispfen Road`, and one locked landmark, `Old Bell Tower`.

## Core Verbs

- Scout a road.
- Fight or ward off a simple threat.
- Cast a spell.
- Collect silver, XP, and rune sparks.
- Upgrade a spell or tool.
- Turn in a quest.
- Unlock a new location.

## Design Pillars

1. Big-world fantasy in small bites.
   - Every screen implies a wider world, but the player always has one clear
     next action.
2. Casual clarity over simulation density.
   - Choices matter, but first-session UI never requires reading a manual.
3. Magic changes the map.
   - Upgrades are not only numbers; they reveal paths, calm hazards, or change
     settlement status.

## Pillar Violations

- Long character creation before the first action.
- Inventory screens with many similar items in the first five minutes.
- Mature gore, sexual content, or grim tone that would block broad web/mobile
  testing.
- Direct Elder Scrolls names, lore, dragon/shout identity, UI copying, or
  quest copying.
- Procedural world promises. The world is authored and static for this
  iteration.

## Progression Metric

Primary visible progress: `ward_rank`.

First-session supporting metrics:

- `xp`
- `silver`
- `rune_sparks`
- `spell_level`
- `locations_unlocked`
- `main_quest_step`

## First-Slice Promise

Within five minutes, the player can:

- see the hamlet and road map;
- scout Wispfen Road;
- fight a Mire Wisp with strike and spark magic;
- earn silver, XP, and one rune spark;
- upgrade Spark Ward;
- unlock Old Bell Tower as the next goal;
- save persistent progress.

