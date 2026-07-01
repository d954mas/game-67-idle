---
id: P002
title: Game Projects
status: dropped
kind: game
target: games
priority: P3
tags: [games, prototypes]
created: 2026-07-01
updated: 2026-07-01
---

## Goal

Legacy shared game bucket retained only as closed history. Active game work is
tracked by one project per concrete `games/<game-id>` folder.

## In scope

- None for active planning.

## Out of scope

- Concrete game work; create or use the matching `games/<game-id>` Taskboard
  project instead.
- Reusable template or AI Studio workflow changes; keep those under the AI
  Studio project until a concrete template/game owner exists.

## Log

- 2026-07-01: created as the top-level Taskboard project for game/prototype work.
- 2026-07-01: dropped because Taskboard should not have a shared "Game
  Projects" owner; each active game gets its own project when
  `games/new_game.mjs` creates `games/<game-id>`.
