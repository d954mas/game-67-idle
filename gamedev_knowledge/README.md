---
type: Gamedev Knowledge Guide
title: Gamedev Knowledge Workspace
description: Folder map for reusable game-development knowledge and source notes.
tags: [gamedev_knowledge, knowledge, sources]
timestamp: 2026-06-13T00:00:00Z
---

# Gamedev Knowledge Workspace

This folder is for reusable game-development knowledge and reusable source notes.
Game-specific design, references, screenshots, playtest notes, GDDs, and
decisions live under `games/<game-id>/design/`.

## Folder Map

- `knowledge/` - general game design knowledge that should apply across
  projects: patterns, checklists, research methods, validation rules, and
  agent-readable design principles.
- `sources/` - raw or near-raw source notes for reusable knowledge.

## Rule Of Thumb

If a note names the current game, current characters, current balance numbers,
current tasks, accepted concept decisions, local screenshots, playtest results,
or implementation status, it belongs under `games/<game-id>/design/`, the game
folder, or task files; not under `gamedev_knowledge/`.

Promote something from game-specific design into `knowledge/` only when it becomes a
reusable rule for future games.
