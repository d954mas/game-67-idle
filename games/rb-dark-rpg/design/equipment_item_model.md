---
type: Game Design Specification
title: RB Dark RPG Equipment And Item Model
description: MVP item kinds, equipment slots, and first quest gear progression.
tags: [gdd, content, items, equipment, inventory]
game_id: rb-dark-rpg
status: draft
---

# Equipment And Item Model

Status: draft

## Slot Model

The save schema and runtime support the full early RPG equipment grid:

| Slot | Role | First-slice use |
|---|---|---|
| `weapon` | Main damage source | Required by `q001_gate_pass` before the first fight. |
| `offhand` | Later shield/tool slot | Supported by save/runtime, no first-slice items yet. |
| `head` | Later armour slot | Supported by save/runtime, no first-slice items yet. |
| `armour` | Main survivability source | Required by `q001_gate_pass` before the first fight. |
| `hands` | Later armour/utility slot | Supported by save/runtime, no first-slice items yet. |
| `waist` | Later belt/utility slot | Supported by save/runtime, no first-slice items yet. |
| `legs` | Secondary survivability source | Required by `q001_gate_pass` so the player learns multiple slots. |
| `feet` | Later movement/armour slot | Supported by save/runtime, no first-slice items yet. |
| `neck` | Later amulet slot | Supported by save/runtime, no first-slice items yet. |
| `ring_left` | Later ring slot | Supported by save/runtime, no first-slice items yet. |
| `ring_right` | Later ring slot | Supported by save/runtime, no first-slice items yet. |
| `relic` | Special clue/status slot, displayed as `Знак` | Introduced after the first quest, not required for onboarding. |

The first playable slice actively uses four slots:

| Slot | Role | First-slice use |
|---|---|---|
| `weapon` | Main damage source | Required by `q001_gate_pass` before the first fight. |
| `armour` | Main survivability source | Required by `q001_gate_pass` before the first fight. |
| `legs` | Secondary survivability source | Required by `q001_gate_pass` so the player learns multiple slots. |
| `relic` | Special clue/status slot, displayed as `Знак` | Introduced after the first quest, not required for onboarding. |

Do not add authored items for inactive slots until gameplay needs them. Empty
future slots can exist in the save schema, but the first equipment UX should keep
attention on `weapon`, `armour`, `legs`, and later `relic`.

## Item Kinds

Authored item kinds:

- `gear` - equipable item with a slot and stat deltas.
- `quest_item` - inventory item used by quests, gates, and contracts.
- `clue` - evidence/lore item that may later feed journal or report systems.
- `consumable` - deferred; potions/scrolls are not needed in the first quest.
- `material` - deferred; crafting is not part of the first slice.

Gold is not an item. It stays in `wallet.gold`.

## First Quest Gear

`q001_gate_pass` teaches equipment through a small three-piece set:

| Item | Slot | Purpose |
|---|---|---|
| `old_sword` | `weapon` | Shows weapon damage and unlocks the first fight. |
| `padded_jacket` | `armour` | Shows vitality/protection as survival stats. |
| `leather_greaves` | `legs` | Confirms that equipment has multiple slots. |

The first fight should be easy with all three equipped. The player should not
need relic gear, shop gear, grinding, or consumables to pass the first gate check.

## Early Upgrade Set

After `q001_gate_pass`, the blacksmith can sell a level 2-3 upgrade set:

| Item | Slot | Purpose |
|---|---|---|
| `iron_sword` | `weapon` | Clear damage upgrade. |
| `militia_axe` | `weapon` | Alternate damage upgrade that teaches strength scaling. |
| `patched_mail` | `armour` | Clear survivability upgrade. |
| `guard_coat` | `armour` | Vitality-heavy alternative to block-heavy armour. |
| `iron_greaves` | `legs` | Keeps the legs slot relevant after onboarding. |
| `runner_wraps` | `legs` | Lightweight alternative with a small intuition bonus. |
| `black_sun_charm` | `relic` | First special/clue-flavored stat item. |

The relic is not baseline armour. It is a story-tinted slot for unusual finds,
marks, fragments, and clue-related bonuses.

## First Location Drops

Old Mill and early road encounters can introduce a small found-gear set without
adding new slots:

| Item | Slot | Purpose |
|---|---|---|
| `miller_hook` | `weapon` | Low raw damage, small intuition flavor. |
| `chain_patches` | `armour` | Protection-heavy armour drop. |
| `scavenger_knee_plates` | `legs` | Found legs upgrade from scavenger fights. |
| `dragon_ash_token` | `relic` | Dragon-flavored survival/intuition relic. |
| `miller_lucky_nail` | `relic` | Simple protection relic for early comparison. |

## Authoring Rules

- Every gear item must have `kind: "gear"` and one MVP `slot`.
- Every equipable item should have `required_level`, `equipment_set` when it
  belongs to a set, `price_gold` if sellable, and `stats`.
- Quest items and clues should not use equipment slots.
- Do not create new slots from the editor. Slot expansion is a GameState/schema
  decision.
- Do not delete or rename item ids after they enter `content_compatibility.json`.
  Balance with stats first; destructive changes need a migration plan.

## Editor Expectations

The content editor should make item creation possible without raw JSON:

- create a new item from the Items tab;
- choose kind and slot from the fixed slot registry;
- edit required level, price, icon id, set id, tags, stackability, and stats;
- keep raw JSON available for advanced edits.
