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
- `canvas.md` - dedicated Canvas project reference and local browser deep link,
  written by `games/new_game.mjs`.
- `items/` - modular Items Lua catalog being parity-verified for the E016
  single-source cutover; `../../items.lua.json` is its evaluator manifest.
- `knowledge/` - private game knowledge base: accepted reference lessons,
  playtest findings, build observations, and game-specific decisions.
- `knowledge/sources/` - source notes and reference packets for this game.
- `data/` - structured design data used by implementation.
- `style_lock.json` (when accepted) - operational prompt, owned-exemplar,
  background, size, and deterministic asset-gate contract; start from
  `ai_studio/assets/style_lock/style_lock.example.json` and keep the broader
  taste brief in `art/art_contract.json`.

Keep reusable cross-game rules in the shared game-design knowledge base. Keep
work status in Taskboard.
