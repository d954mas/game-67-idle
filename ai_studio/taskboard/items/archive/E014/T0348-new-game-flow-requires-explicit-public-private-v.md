---
id: T0348
title: New-game flow requires explicit public/private visibility
status: done
project: P001
epic: E014
priority: P1
tags: [new-game, private-repos, template, privacy, workspace]
created: 2026-07-09
updated: 2026-07-09
---

## What

`games/new_game.mjs` can already create both tracked public games and private
nested-repo games (`--private`). This follow-up makes the creation contract
harder to misuse: human-facing and agent-facing new-game workflows must require
an explicit visibility choice instead of silently defaulting commercial work into
the public Studio repo.

This is not a rewrite of `T0343`; it is the UX/API contract on top of the
already-implemented public/private paths.

## Done when

- [x] New-game docs clearly state the two modes:
      public/tracked parent game vs private nested repo under `games/<id>`.
- [x] The Studio/browser or agent-facing new-game flow asks for `public` vs
      `private` before creating a game.
- [x] CLI behavior is made explicit without breaking existing automation: either
      `--visibility public|private` is added, or current public default remains
      only with a loud help/docs warning and private remains `--private`.
- [x] Private creation still never mutates tracked parent registries, Taskboard,
      Canvas, evidence, or VS Code outputs.
- [x] Public creation still registers the tracked game intentionally in
      `games/games.json` and related public outputs.
- [x] Tests cover both explicit public and explicit private creation paths, plus
      the missing-choice behavior for the human/agent-facing workflow.

## Open questions

- Resolved for this slice: raw CLI stays public-by-default for backwards
  compatibility, but `--visibility public|private` is the documented command.
  Human-facing, agent-facing, and Studio flows pass `--require-visibility` so a
  missing choice fails before any game files are copied.

## Log

- 2026-07-09: Created after confirming `T0343` already implemented
  `games/new_game.mjs --private`, but no active card owned the explicit
  visibility-choice UX/agent contract.
- 2026-07-09: Started explicit visibility implementation after task creation commit cdda088e5.
- 2026-07-09: Implemented `--visibility public|private`,
  `--require-visibility`, private/public conflict checks, invalid value checks,
  and public-alias private-only validation. Updated new-game, template, Studio
  routing, GDD workflow, source registry, and gallery docs so non-archive
  creation paths require an explicit visibility decision. Evidence:
  `node --test games/new_game.test.mjs` (17 tests passing).
- 2026-07-09: Completed explicit visibility creation contract. Verification: node --test ai_studio/workspace/tests/private_games_registry.test.mjs games/new_game.test.mjs; node ai_studio/taskboard/cli.mjs validate --json; git diff --check.
