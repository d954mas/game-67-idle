---
type: Game Design Lock
title: RB Dark RPG Combat And Reward Loop V1
description: Fixed first-slice decisions for combat resolution, reward feedback, and the next implementation slices.
tags: [gdd, combat, rewards, uiux]
game_id: rb-dark-rpg
status: locked
---

# Combat And Reward Loop V1

Status: locked on 2026-07-05.

This document fixes the current production direction for the first playable
combat and reward loop. The detailed stat model stays in
`combat_mechanics.md`; this page is the short implementation-facing contract.

## Fixed Decisions

- Combat is pure autobattle in v1. The player prepares before combat; they do
  not press skills, dodge, target, or time consumables during combat.
- Combat starts from a concrete place/threat in the world: a location object,
  encounter row, or quest-linked threat. It is not started from the bottom nav.
- Pre-fight UI explains readiness: enemy, player HP, key stats, threat label,
  reason line, expected rewards, and primary action `В бой`.
- Victory grants rewards immediately and deterministically in the combat
  resolution action.
- Defeat grants nothing, does not advance quests, returns the player to
  `hub_last_post`, and leaves the hero alive at 1 HP.
- Healing is a recovery action before retrying combat. The first recovery path
  can be free; later healing can cost gold or a ration.
- Reward ids, item ids, quest ids, flags, and unlock ids are compatibility
  contracts once player saves can contain them. Deleting or renaming them is a
  migration task, not a casual content edit.

## Victory Result Contract

The result panel must answer three questions without making the player inspect
debug state:

1. Did I win or lose?
2. What changed in my state: XP, gold, item/gear/clue/progress?
3. What should I do next?

For the first gate check, the next action can be specific: `К стражу`.
For later location fights, use neutral/context text such as `Продолжить` unless
the next quest objective is already known and safe to show.

## Reward Rules

- XP and gold are shown as first-class reward cells.
- Every item reward in a multi-reward encounter must be visible, not collapsed
  into a generic `loot` label.
- Gear rewards and stackable quest items both need runtime assertions, because
  they write to different inventory paths.
- Repeating a claimed encounter reward must not duplicate XP, gold, item stacks,
  gear instances, flags, or quest progress.
- Top HUD must reflect updated HP and gold after closing the result.

## Visual Direction For Combat V1

- Keep the combat screen readable before it is decorative.
- Use large enemy/player silhouettes, HP bars, simple swing/timer feedback,
  floating damage numbers, and explicit crit/block markers only when they occur.
- Keep the combat log to the last three events.
- Avoid noisy background detail under combat UI. The fight should read at
  960x540 and phone portrait without needing small text.
- The visual style should stay Roblox-like blocky dark fantasy, but rendered as
  2D browser RPG UI, not a painted illustration with dense detail.

## Current Implementation Plan

1. Finish `T0285`: victory reward handoff, runtime reward assertions, first and
   later win evidence. This includes proving that closing a result panel does
   not click through into hub objects on desktop or phone.
2. Then do `T0286`: combat screen visual readability pass. Keep it focused on
   large forms, readable HP/timeline feedback, blocky dark-fantasy style, and
   less visual noise.
3. Then do `T0287`: reward presentation and loot feedback pass. Make XP, gold,
   stack items, and gear visually distinct without changing reward ids.
4. After those pass, plan the next content slice: the first full contract loop
   at the old mill, including authored enemies, item rewards, quest progress,
   and return-to-hub handoff.

Out of scope for this loop: active skills, mana, stamina, dodge/agility,
elemental damage, status stacks, injuries, PvP, pets, party formation, and
idle/offline rewards.
