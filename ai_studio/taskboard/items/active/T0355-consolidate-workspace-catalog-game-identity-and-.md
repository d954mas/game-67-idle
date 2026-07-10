---
id: T0355
title: Consolidate Workspace Catalog, game identity, and tested dependency records
status: backlog
project: P001
epic: E015
priority: P0
tags: [workspace, games, dependencies]
created: 2026-07-10
updated: 2026-07-10
---

## What

Remove duplicate identity/mount/dependency registries. Workspace owns public and
private game/template mounts; each game owns its identity and tested dependency
record.

## Done when

- [ ] Minimal `game.json` owns `id`, `title`, and `storage_namespace`; runtime,
      CMake, web, storage, design, and generated IDE entries derive from it with
      no quiet `Template` fallback.
- [ ] Workspace Catalog stores mounts/root plus tracked public and ignored local
      overlays; identity is read from `game.json`/`template.json`.
- [ ] Old `games/games.json`, `templates/templates.json`, and
      `ai_studio/workspace/games.local.json` identity/mount duplication is
      migrated and removed without weakening private leak guards.
- [ ] Assets, VS Code generation, Studio Shell, Taskboard routing, and game/
      template creation use one Workspace resolver.
- [ ] Each game has `dependencies.json` recording tested engine/feature versions
      and compatibility; updates are explicit and local forks remain the escape
      hatch.
- [ ] No feature snapshot archive, dependency cache, sync/link command, tag
      strategy, or automatic old-game repair is added.

## Open questions

- Final Workspace Catalog schema/version lifecycle and migration order from the
  already-deployed private workspace registry.

## Log

- 2026-07-10: Preserve completed `T0341`-`T0348` privacy gates; this task owns
  the accepted consolidation delta, not a second workspace implementation.
