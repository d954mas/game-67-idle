---
id: T0269
title: "New game flow: create and link a per-game canvas project"
status: backlog
project: P001
epic: E009
priority: P2
tags: [games, canvas, template]
created: 2026-07-04
updated: 2026-07-04
---

## What

When a new game is created through the AI Studio new-game flow, create a
dedicated Canvas project for that game and write the canvas reference into the
game workspace so agents and the lead can open it later without manual setup.

## Done when

- [ ] `games/new_game.mjs` (or the owning new-game workflow) creates a Canvas
      project for the new `games/<game-id>/` folder.
- [ ] The created canvas is named and discoverable by game id/title, and the
      resulting `canvas://...` ref is stored under the game design docs.
- [ ] Re-running or recovering a partially completed new-game flow does not
      create duplicate canvas projects when a canvas ref already exists.
- [ ] The workflow output prints the canvas ref and browser URL alongside the
      game path.
- [ ] Validation covers at least one dry-run or fixture path for a new game.

## Open questions

- Should the canvas start from a fixed per-game layout template, or only create
  an empty named project at this stage?

## Log

- 2026-07-04: captured from rb-dark-rpg setup; its game canvas was created
  manually after the game, so future game creation should do this by default.
