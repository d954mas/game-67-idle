---
type: Design Workspace Guide
title: Game Design Workspace
description: Folder map for reusable design knowledge and project-specific game wikis.
tags: [gamedesign, knowledge, projects]
timestamp: 2026-06-13T00:00:00Z
---

# Game Design Workspace

This folder separates reusable design knowledge from game-specific project
knowledge.

## Folder Map

- `knowledge/` - general game design knowledge that should apply across
  projects: patterns, checklists, research methods, validation rules, and
  agent-readable design principles.
- `sources/` - raw or near-raw source notes for reusable knowledge.
- `projects/` - project-specific game wikis and GDDs. Each game gets its own
  folder, for example `projects/my-game/`.

## Rule Of Thumb

If a note names the current game, current characters, current balance numbers,
current tasks, accepted concept decisions, local screenshots, playtest results,
or implementation status, it belongs under `projects/<game-id>/`, not
`knowledge/`.

Promote something from a project wiki into `knowledge/` only when it becomes a
reusable rule for future games.
