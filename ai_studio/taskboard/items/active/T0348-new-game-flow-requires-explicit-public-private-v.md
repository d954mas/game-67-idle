---
id: T0348
title: New-game flow requires explicit public/private visibility
status: backlog
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

- [ ] New-game docs clearly state the two modes:
      public/tracked parent game vs private nested repo under `games/<id>`.
- [ ] The Studio/browser or agent-facing new-game flow asks for `public` vs
      `private` before creating a game.
- [ ] CLI behavior is made explicit without breaking existing automation: either
      `--visibility public|private` is added, or current public default remains
      only with a loud help/docs warning and private remains `--private`.
- [ ] Private creation still never mutates tracked parent registries, Taskboard,
      Canvas, evidence, or VS Code outputs.
- [ ] Public creation still registers the tracked game intentionally in
      `games/games.json` and related public outputs.
- [ ] Tests cover both explicit public and explicit private creation paths, plus
      the missing-choice behavior for the human/agent-facing workflow.

## Open questions

- Should the raw CLI stay public-by-default for backwards compatibility, or
  should it eventually require `--visibility public|private` too?

## Log

- 2026-07-09: Created after confirming `T0343` already implemented
  `games/new_game.mjs --private`, but no active card owned the explicit
  visibility-choice UX/agent contract.
