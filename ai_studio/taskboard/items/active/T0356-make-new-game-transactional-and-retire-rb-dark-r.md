---
id: T0356
title: Make new_game transactional and retire rb-dark-rpg cleanly
status: backlog
project: P001
epic: E015
priority: P0
tags: [new-game, transaction, cleanup]
created: 2026-07-10
updated: 2026-07-10
---

## What

Make game creation a recoverable transaction and remove the abandoned
`rb-dark-rpg` prototype instead of migrating it. Identity must be transformed
from the new game's `game.json`, with no template-name fallback left behind.

## Done when

- [ ] `new_game` writes into a staging directory, validates the complete result,
      then performs one atomic final rename; failures clean the staging tree.
- [ ] Existing destinations fail safely unless explicit `--replace` is used;
      replacement also has rollback and cannot leave a half-created game.
- [ ] Game id, title, storage namespace, generated paths, build metadata, tests,
      and IDE wiring all derive from the new `game.json`.
- [ ] Tests cover stale destination files, invalid identity, late generator
      failure, replacement rollback, and unique test storage namespaces.
- [ ] `rb-dark-rpg` and its catalog/task/IDE/report references are deleted; no
      migration or compatibility entry is created.
- [ ] Asset references to `rb-dark-rpg` are inventoried and removed or replaced
      with sanitized reusable fixtures.
- [ ] External Canvas projects retain their `gameId`, receive an archive marker,
      and disappear from the active view; shared project data is not deleted.

## Open questions

- Confirm the exact `--replace` backup/rollback mechanics during implementation.

## Log

- 2026-07-10: The lead explicitly chose deletion of `rb-dark-rpg` and rejected
  migrating it as an active game.
