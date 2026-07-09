---
id: T0344
title: Store-qualified Taskboard CLI API and UI
status: backlog
project: P001
epic: E014
priority: P1
tags: [taskboard, private-repos, workspace]
created: 2026-07-09
updated: 2026-07-09
---

## What

Make Taskboard store-qualified so Studio tasks and game-owned tasks can coexist
without copying private work state into the public Studio task store.

This slice owns CLI, API, UI, validation, and agent-context behavior for mounted
Taskboard stores.

## Done when

- [ ] Taskboard can mount `ai_studio/taskboard/items/` plus
      `games/<id>/.ai_studio/taskboard/items/` stores from the workspace
      resolver.
- [ ] CLI commands that read or mutate items accept an explicit store selector
      (`--store`, `--game`, or active workspace) and reject ambiguous bare IDs in
      aggregate context.
- [ ] API payloads and UI rows include `storeId`, `visibility`, and qualified
      item IDs while keeping bare IDs valid inside one selected store.
- [ ] `/api/agent/context`, board list/search, and CLI list exclude private
      stores unless active workspace or `--include-private` is provided.
- [ ] Cross-store project/epic/task links must be qualified, for example
      `game:rb-dark-rpg:T0001`; validation rejects ambiguous links.
- [ ] Every game store may use local `P001`/`E001`/`T0001` IDs without colliding
      with other stores.
- [ ] New task/project creation writes to the selected store, never to the
      public Studio root by fallback.
- [ ] Tests cover public-only list, explicit private include, ambiguous bare ID
      rejection, and store-qualified mutation.
- [ ] `node ai_studio/taskboard/cli.mjs validate --json` passes, or unrelated
      validation failures are recorded.

## Open questions

- Should the default UI remember the last active store per browser, or should it
  always open on public Studio-only data?

## Log

- 2026-07-09: Created as child task from `T0341` review to make CLI/API/UI
  behavior explicit instead of only describing an abstract aggregator.
