---
id: T0343
title: Game-owned scaffold and private new-game flow
status: backlog
project: P001
epic: E014
priority: P1
tags: [private-repos, games, template, new-game]
created: 2026-07-09
updated: 2026-07-09
---

## What

Add the game-owned `.ai_studio/` scaffold and split public/private game
creation paths.

The public flow may continue to register tracked games in public files. The
private flow must create or verify a nested private game repo under `games/<id>`
without mutating tracked parent Studio outputs.

## Done when

- [ ] The game template includes a documented `.ai_studio/` scaffold:
      `taskboard/items/`, `canvas/projects/`, `evidence/`, and
      `workspace.json`.
- [ ] `games/new_game.mjs` or its replacement has an explicit private path
      (`--private` or separate command); private creation is never implicit.
- [ ] Public creation writes to `games/games.json`, public game folder, public
      game `.ai_studio/`, and tracked parent generated files as appropriate.
- [ ] Private creation writes to `games/<id>/`, initializes or verifies the
      nested Git repo, writes game-local `.ai_studio/`, updates only ignored
      local registry/exclude files, and does not write public Taskboard,
      Canvas, registry, or tracked `.vscode` entries.
- [ ] Private game names are treated as confidential unless the local registry
      provides an explicit public alias.
- [ ] Fresh private games inherit the same asset/provenance/source-first rules
      as public games.
- [ ] Creation dry runs or tests cover both public and private flows.
- [ ] `node ai_studio/taskboard/cli.mjs validate --json` passes, or unrelated
      validation failures are recorded.

## Open questions

- Should private creation initialize the nested Git repository automatically, or
  require an existing private remote/checkout?

## Log

- 2026-07-09: Created as child task from `T0341` review to separate scaffold and
  game creation from registry/leak-guard work.
