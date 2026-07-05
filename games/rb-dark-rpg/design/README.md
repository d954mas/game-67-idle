---
type: Game Design Guide
title: RB Dark RPG Design
description: Design source-of-truth map for RB Dark RPG.
tags: [game-design]
game_id: rb-dark-rpg
status: draft
---

# RB Dark RPG Design

This folder is the game-owned design source of truth.

## Map

- `concept.md` - first-pass hook, audience, pillars, and no-go list.
- `gdd.md` - current game design document for implementation.
- `combat_mechanics.md` - full first-slice stat, autobattle, progression, and
  balance model.
- `combat_reward_loop_v1.md` - locked first-slice combat/reward resolution and
  implementation plan.
- `release_30_minute_balance.md` - release-scope 30-minute balance contract:
  10 levels, 10+ quests, 5 gear tiers, 10+ enemies, and visual proof target.
- `combat_entry_ux_v1.md` - first-slice UX contract for entering, watching, and
  resolving pure autobattle.
- `world_map_v1.md` - visual-region map contract replacing the temporary
  location graph with an authored terrain/region map.
- `quest_system.md` - quest authoring, runtime state shape, and first quest
  structure.
- `content_model.md` - data-driven entity model for characters, locations,
  items, dialogues, quests, services, encounters, and assets.
- `content_editor_spec.md` - local web content editor requirements and runtime
  preview contract.
- `editor/` - first local web content editor for the JSON data files.
- `knowledge/` - private game knowledge base: accepted reference lessons,
  playtest findings, build observations, and game-specific decisions.
- `knowledge/sources/` - source notes and reference packets for this game.
- `data/` - structured design data used by implementation.
- `layout/release_30min_fake_shot.*` - fake-shot visual proof for the release
  balance direction.

Keep reusable cross-game rules in the shared game-design knowledge base. Keep
work status in Taskboard.
