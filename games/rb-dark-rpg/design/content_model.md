---
type: Game Design Specification
title: RB Dark RPG Content Model
description: Data-driven entity model and authoring rules for RB Dark RPG content.
tags: [gdd, content, data, authoring, tooling]
game_id: rb-dark-rpg
status: draft
---

# Content Model

Status: draft

## Goal

The game should load content from data configs, and the lead should be able to
edit that content through a web editor without touching runtime code.

Content means:

- characters and NPCs;
- portraits, full character art, and dialogue faces;
- locations, screens, scene art, and clickable objects;
- items, gear, quest items, rewards, and stats;
- dialogues and choices;
- quests, requirements, steps, rewards, and unlocks;
- encounters and combat data;
- flags and save-state progress.

## Source Files

All authored game content for this game lives under:

```text
games/rb-dark-rpg/design/data/
```

V1 file split:

| File | Owns |
|---|---|
| `content_manifest.json` | Load order, registries, editor views, generated bundle target. |
| `characters.json` | NPCs, enemies, player-facing character metadata, art refs, interaction roles. |
| `locations.json` | Hub/location definitions, background art refs, clickable objects, exits, unlocks. |
| `items.json` | Gear, quest items, currencies, item stats, icons, stack rules. |
| `dialogues.json` | Dialogue trees, lines, choices, effects, speaker refs. |
| `quests.json` | Quest definitions, requirements, steps, objective refs, rewards, unlocks. |
| `combat.json` | Combat formulas, encounters, balance data. |
| `services.json` | Shops, healer services, prices, service effects. |
| `asset_manifest.json` | Asset ids, asset status, provenance, and source file paths for editor previews. |

Runtime should load these through a single content database, not with direct
file-specific ad hoc reads scattered through gameplay code.

## Id Rules

Use stable ids everywhere.

```text
characters: gate_guard, blacksmith
locations: hub_last_post, old_mill
objects: hub_last_post.gate, hub_last_post.contract_board
items: old_sword, padded_jacket, leather_greaves, seeker_token
dialogues: dlg_gate_guard_intro
quests: q001_gate_pass
encounters: gate_scavenger
assets: asset_portrait_gate_guard
flags: map_gate_unlocked
```

Rules:

- ids are lowercase ASCII `a-z0-9_`;
- object ids may use `location.object`;
- never use display text as an id;
- never rename ids after save-state depends on them; add aliases/migrations if
  a rename becomes necessary.

## Entity Relationships

The content graph is id-linked:

```text
quest step -> character/location/dialogue/item/encounter ids
dialogue node -> speaker character id
character -> portrait/full art/dialogue face asset ids
location object -> character/item/dialogue/quest/encounter ids
item -> icon asset id and stat deltas
encounter -> enemy character id or inline enemy stat profile
asset refs -> asset_manifest entries
available asset -> optional file_path relative to games/rb-dark-rpg, for example assets/scenes/last_post_background_candidate05_1280x700.png
```

The web editor should present these links through pickers/autocomplete, not free
text fields.

## Requirements

Use one shared requirement shape across quests, locations, objects, dialogues,
shops, and encounters.

Examples:

```json
{ "type": "quest_completed", "quest_id": "q001_gate_pass" }
{ "type": "flag", "flag_id": "map_gate_unlocked", "value": true }
{ "type": "level_min", "level": 2 }
{ "type": "has_item", "item_id": "seeker_token", "count": 1 }
{ "type": "equipped", "item_id": "old_sword" }
```

Runtime evaluates requirements the same way everywhere. UI can turn failed
requirements into blocked reasons.

## Effects

Use one shared effect shape for quest rewards, dialogue choices, objects, and
encounter results.

Examples:

```json
{ "type": "grant_item", "item_id": "old_sword", "count": 1 }
{ "type": "grant_gold", "amount": 5 }
{ "type": "grant_xp", "amount": 8 }
{ "type": "set_flag", "flag_id": "map_gate_unlocked", "value": true }
{ "type": "unlock_location", "location_id": "old_mill" }
{ "type": "unlock_quest", "quest_id": "q002_bread_for_post" }
{ "type": "start_dialogue", "dialogue_id": "dlg_gate_guard_intro" }
{ "type": "advance_quest", "quest_id": "q001_gate_pass", "step_id": "talk_gate_guard" }
```

Runtime should not need quest-specific code for these common outcomes.

## Dialogue Quest Preview

Dialogue nodes that start or explain a quest may include a compact
`quest_preview` block. It is UI metadata derived from the same ids used by
effects and quest rewards:

```json
{
  "goal": "Надень меч, броню и поножи, убери падальщика у ворот и вернись к стражу.",
  "immediate_rewards": [
    { "type": "item", "item_id": "old_sword", "count": 1 }
  ],
  "completion_rewards": [
    { "type": "item", "item_id": "seeker_token", "count": 1 },
    { "type": "grant_xp", "amount": 10 },
    { "type": "unlock_screen", "screen_id": "ash_border_map" }
  ]
}
```

The runtime dialogue panel displays quest-taking previews in this order:

1. dialogue text;
2. separator;
3. `Задание` - short objective text that can be understood without rereading
   the dialogue;
4. separator;
5. `Текущая награда` - reward cells granted immediately by this dialogue;
6. separator;
7. `Награда за квест` - reward cells granted on quest completion.

Reward previews are inventory-like cells: icon/image first, short tooltip on
hover, and a tap/click detail window for touch screens. The preview must not
replace real `effects` or quest `completion_rewards`; it only makes the upcoming
outcomes readable before the player accepts the mandatory first quest.

## Runtime Loading

Recommended loading pipeline:

1. Load `content_manifest.json`.
2. Load listed files in manifest order.
3. Build registries by id:
   `characters`, `locations`, `items`, `dialogues`, `quests`, `encounters`,
   `assets`.
4. Validate ids and cross-references.
5. Expose a read-only `ContentDb` to runtime systems.
6. Initialize save-state from `quests.example_initial_state` and player defaults.

Runtime save-state stores progress, not authored content.

## Web Editor Requirements

The editor should be a local browser surface for this game. It is the primary
authoring tool. The game engine should provide runtime preview/debug entry
points, but not be the main place where content is authored.

Reason:

- web forms/tables/graphs are better for quests, dialogue trees, rewards, and
  requirements;
- content can be validated without launching the full game;
- the same JSON can be loaded by the game and edited by the tool;
- the engine stays focused on rendering and playtest feel;
- a broken editor does not risk corrupting runtime gameplay code.

Core views:

- `Content Dashboard`: broken refs, changed files, validation status.
- `Characters`: edit names, roles, art refs, location, interactions.
- `Locations`: edit background art, clickable objects, exits, unlock rules.
- `Items`: edit item type, slot, stats, icon, price, stack rules.
- `Services`: edit shops, healer services, prices, and inventories.
- `Dialogues`: graph/tree editor for speaker lines, choices, requirements, effects.
- `Quests`: step list editor with objective picker, dialogue links, rewards, blockers.
- `Rewards`: compact grant-item/gold/xp/effect builder reused by quests/dialogues.
- `Preview`: simulate first quest state and show current journal/hub markers.

Required editor affordances:

- id picker/autocomplete for all refs;
- duplicate-id prevention;
- broken-reference warnings before save;
- visual asset picker from `asset_manifest.json`;
- step reorder controls for linear quests;
- JSON source view as an advanced/debug tab only;
- save writes the same JSON files the game loads.

## Validation

Content validation should fail on:

- duplicate ids;
- missing referenced character/location/item/dialogue/quest/encounter/asset ids;
- quest step objective type unknown to runtime;
- reward/effect type unknown to runtime;
- location object with no interaction;
- dialogue choice pointing to a missing node;
- quest step that cannot be reached;
- authored text stored in save-state;
- asset refs not present in `asset_manifest.json`.

Warnings, not hard failures:

- missing final art while asset status is `needed`;
- optional dialogue missing;
- placeholder portrait;
- untranslated internal note.

## Generated Runtime Bundle

For implementation, the editor or build step may generate:

```text
games/rb-dark-rpg/build/generated/content_pack.json
```

That generated file should be derived from the authored data files and should
not become the hand-edited source of truth.

## Engine Preview Role

The game should still have content debug hooks:

- open location by id;
- start quest by id;
- jump to quest step;
- start dialogue by id;
- grant item by id;
- start encounter by id;
- reload content configs without rebuilding when possible.

These are preview/playtest tools, not the main editor.
