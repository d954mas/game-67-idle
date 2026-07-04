---
type: Tool Design Specification
title: RB Dark RPG Content Editor Spec
description: Web editor requirements for editing RB Dark RPG content configs.
tags: [tooling, editor, content, web]
game_id: rb-dark-rpg
status: draft
---

# Content Editor Spec

Status: draft

## Purpose

Create a local web editor that lets the lead edit game content in forms and
preview flows, while the game still loads plain JSON configs.

This is an authoring tool, not the game runtime UI.

Decision: use a web content platform as the primary editor. The game engine
should expose preview/debug hooks, not become the main content editor.

Why:

- quest steps, dialogue graphs, rewards, requirements, and references are data
  work, not game-rendering work;
- a browser editor can provide search, tables, graph previews, validation, and
  asset pickers faster than an in-engine UI;
- the game can stay a consumer of the same configs, which keeps runtime simpler.

## First Version Scope

Current implementation path:

```text
games/rb-dark-rpg/design/editor/
```

V1 editor should support:

- characters;
- locations and clickable objects;
- items and stat deltas;
- shops and healer services;
- dialogues;
- quests and quest steps;
- rewards, requirements, and effects;
- asset refs and visual previews from `asset_manifest.json`;
- validation and export.

Do not add a full visual scene editor in v1. Locations can use rough object
position hints until final art/layout tooling is chosen.

## Layout

Use a dense work-tool layout:

- left sidebar: entity type tabs;
- second column: searchable entity list;
- main panel: form editor;
- right panel: references, validation, and preview;
- bottom/status strip: changed files and save/validate status.

Reference UX lessons used for this editor:

- Unity Addressables: content is manageable when assets have groups, labels,
  type/path columns, and search/filter by label or path.
  Source: https://docs.unity3d.com/Packages/com.unity.addressables@1.21/manual/GroupsWindow.html
- Godot Inspector: a selected entity should expose searchable properties grouped
  into sections instead of one long flat form.
  Source: https://docs.godotengine.org/en/stable/tutorials/editor/inspector_dock.html
- Inky: narrative/content tools should keep validation visible while authoring,
  with an issue list that points back to the broken place.
  Source: https://www.inklestudios.com/ink/
- Airtable/Notion-style databases: group/filter/sort views are essential once
  content has types, statuses, tags, and visual assets.
  Sources:
  https://support.airtable.com/docs/grouping-records-in-airtable
  https://www.notion.com/help/views-filters-and-sorts

Primary tabs:

1. `Characters`
2. `Locations`
3. `Items`
4. `Services`
5. `Dialogues`
6. `Quests`
7. `Combat`
8. `Assets`
9. `Validation`

Each entity tab must support:

- search by id/title/name/role/description;
- grouping by the most important field for that entity type;
- filters for type/status/tags/location/priority where available;
- group counts;
- collapsible groups;
- thumbnail previews when the entity references an available asset.

## Quest Editing Flow

Quest editor must make this easy:

1. Create quest id/title/type.
2. Pick giver from characters.
3. Add requirements through a builder.
4. Add ordered steps.
5. For each step, pick objective type.
6. Pick target id from the correct registry.
7. Link optional dialogue.
8. Add rewards/effects.
9. Preview journal text and blocked reasons.
10. Validate broken refs.

The editor should not ask the lead to manually type ids when a registry picker
can be used.

## Dialogue Editing Flow

Dialogue editor:

- each dialogue has participants and nodes;
- each node has speaker, portrait override, line text, choices, effects;
- choices can have requirements;
- choices can advance quest steps or grant items;
- graph/tree preview should show unreachable nodes.

For v1, linear dialogues are enough. Branching choices are supported by data
shape but should be used sparingly.

## Location Editing Flow

Location editor:

- edit background asset id;
- edit map node asset id;
- add scene objects;
- choose object kind: `npc`, `hotspot`, `exit`, `combat`, `shop`, `healer`,
  `quest_board`, `memorial`;
- link character/dialogue/quest/encounter ids;
- set requirements and blocked text;
- preview hub object list and quest markers.

Precise coordinates can wait. Use `position_hint` first: `left_gate`,
`center_board`, `right_forge`, etc.

## Save Format

The editor writes the authored files directly:

```text
games/rb-dark-rpg/design/data/content_manifest.json
games/rb-dark-rpg/design/data/characters.json
games/rb-dark-rpg/design/data/locations.json
games/rb-dark-rpg/design/data/items.json
games/rb-dark-rpg/design/data/services.json
games/rb-dark-rpg/design/data/dialogues.json
games/rb-dark-rpg/design/data/quests.json
games/rb-dark-rpg/design/data/combat.json
games/rb-dark-rpg/design/data/asset_manifest.json
```

Implementation can later generate a packed runtime file, but the hand-edited
truth stays modular.

## Validation Rules

V1 validation must check:

- all ids unique within their registry;
- all refs resolve;
- all quest steps have objective type and target;
- all quest rewards/effects use known effect types;
- all dialogue choices point to existing nodes;
- all location object interactions point to valid content;
- all asset refs exist in `asset_manifest.json`;
- all item stat names exist in combat stat definitions.
- available assets with `file_path` load as thumbnails/previews in the editor.

Editor should show errors beside fields and in the Validation tab.

## Runtime Contract

The runtime should consume the same configs through a content loader:

```text
ContentManifest -> JSON files -> registries -> validation -> read-only ContentDb
```

Systems read data by id:

```text
QuestSystem uses ContentDb.quests
DialogueSystem uses ContentDb.dialogues + ContentDb.characters
LocationScreen uses ContentDb.locations + ContentDb.assets
Inventory uses ContentDb.items
Services use ContentDb.shops + ContentDb.healing_services
CombatSystem uses ContentDb.combat + ContentDb.items
```

Runtime save-state stores only player progress and inventory, never copied
dialogue or quest text.

## Engine Preview Contract

The editor should later be able to call or link to engine preview actions:

```text
preview location: old_mill
preview dialogue: dlg_gate_guard_intro
preview quest step: q001_gate_pass.clear_gate_scavenger
  preview encounter: gate_scavenger
  preview inventory item: old_sword
```

These actions prove content in context, but edits still go through the web
editor and JSON validation.

## Local Authoring Modes

The editor supports two local modes:

- server mode: `node games/rb-dark-rpg/design/editor/server.mjs 5191`, then
  open `http://127.0.0.1:5191/`;
- folder mode: open `games/rb-dark-rpg/design/editor/index.html` directly in
  Chrome and choose `games/rb-dark-rpg/design/data` with `Open data folder`.
