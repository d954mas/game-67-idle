---
id: T0343
title: Game-owned scaffold and private new-game flow
status: done
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

- [x] The game template includes a documented `.ai_studio/` scaffold:
      `taskboard/items/`, `canvas/projects/`, `evidence/`, and
      `workspace.json`.
- [x] `games/new_game.mjs` or its replacement has an explicit private path
      (`--private` or separate command); private creation is never implicit.
- [x] Public creation writes to `games/games.json`, public game folder, public
      game `.ai_studio/`, and tracked parent generated files as appropriate.
- [x] Private creation writes to `games/<id>/`, initializes or verifies the
      nested Git repo, writes game-local `.ai_studio/`, updates only ignored
      local registry/exclude files, and does not write public Taskboard,
      Canvas, registry, or tracked `.vscode` entries.
- [x] Private game names are treated as confidential unless the local registry
      provides an explicit public alias.
- [x] Fresh private games inherit the same asset/provenance/source-first rules
      as public games.
- [x] Creation dry runs or tests cover both public and private flows.
- [x] `node ai_studio/taskboard/cli.mjs validate --json` passes, or unrelated
      validation failures are recorded.

## Open questions

- Resolved: private creation initializes a nested Git repository when missing
  and verifies any existing nested repository with `git rev-parse`.

## Log

- 2026-07-09: Created as child task from `T0341` review to separate scaffold and
  game creation from registry/leak-guard work.
- 2026-07-09: Started after T0342 private registry/guard commit 63fff6007; implementing game-owned scaffold and explicit private new-game flow.
- 2026-07-09: Implemented template `.ai_studio/` scaffold, `games/new_game.mjs --private`, local registry upsert, parent `.git/info/exclude` updates, nested private git init/verify, private preflight, and public alias handling. Targeted tests pass: `node --test ai_studio/workspace/tests/private_games_registry.test.mjs games/new_game.test.mjs`.
- 2026-07-09: Addressed review findings: private `--force` refuses parent-tracked target roots before copy, nested git verification checks the game top-level, and parent exclude path uses `git rev-parse --git-path info/exclude` for worktree compatibility. PASS: `node --test ai_studio/workspace/tests/private_games_registry.test.mjs games/new_game.test.mjs ai_studio/assets/backlog/storage/sources/tests/games.test.mjs ai_studio/dev_environment/vscode_projects.test.mjs`; PASS: `node ai_studio/workspace/games.mjs preflight --json`; PASS: `node ai_studio/taskboard/cli.mjs validate --json`.
- 2026-07-09: Archived after commit ab435349e verified private new-game flow and scaffold.
