---
id: T0199
title: Move seed template and games under root folders
status: done
epic: E001
priority: P1
tags: [bootstrap, assets, templates, games, layout]
created: 2026-07-01
updated: 2026-07-01
---

## What

Introduce explicit root folders for reusable game templates and created games:

- `templates/<template-id>/` owns runnable seed templates;
- `games/<game-id>/` owns created game projects.

Move the current root `template/` into `templates/template/`, update bootstrap
to copy templates into `games/<id>/`, and keep Asset Storage template/game
registries consistent with the new layout.

## Done when

- [x] The current seed template lives under `templates/template/`.
- [x] `new_game.mjs` defaults to `templates/template` and creates
      `games/<game-id>/`.
- [x] Template and game asset registries use `templates/.../assets` and
      `games/.../assets`.
- [x] Docs, tests, and map paths no longer point at root `template/` as the
      canonical layout.
- [x] Focused bootstrap/assets checks pass.

## Open questions

## Log

- 2026-07-01: Created after deciding root folders should be explicit:
  `templates/` for templates and `games/` for created games.
- 2026-07-01: Moved root `template/` to `templates/template/`, added
  `games/.gitkeep`, updated bootstrap defaults, registries, docs, tests, and
  template CMake/font paths for the deeper folder layout.
- 2026-07-01: Verified focused bootstrap/assets/state checks, CMake configure,
  architecture map strict validation, doc reference check, and restricted asset
  guard.
