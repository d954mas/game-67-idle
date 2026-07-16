---
type: Game Design Guide
title: Game Design
description: Design source-of-truth map for this game.
tags: [game-design]
game_id: TODO
status: draft
---

# Game Design

This folder is the game-owned design source of truth.

## Map

- `concept.md` - first-pass hook, audience, pillars, and no-go list.
- `gdd.md` - current game design document for implementation.
- `items/` - modular Items Lua catalog being parity-verified for the E016
  single-source cutover; `../../items.lua.json` is its evaluator manifest.
- `knowledge/` - private game knowledge base: accepted reference lessons,
  playtest findings, build observations, and game-specific decisions.
- `knowledge/sources/` - source notes and reference packets for this game.
- `data/` - structured design data used by implementation.

Keep reusable cross-game rules in the shared game-design knowledge base. Keep
work status in Taskboard.
