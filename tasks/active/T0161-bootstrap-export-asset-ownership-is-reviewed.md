---
id: T0161
title: Bootstrap export asset ownership is reviewed
status: backlog
epic: ""
priority: P2
tags: [assets, bootstrap, legacy]
created: 2026-06-30
updated: 2026-06-30
---

## What

Review `tools/bootstrap/` and export-base asset behavior after the asset module
refactor. Decide whether bootstrap/export remains legacy, moves into a future
AI Studio module, or only keeps thin compatibility entrypoints.

## Done when

- [ ] Asset-related bootstrap/export responsibilities are classified.
- [ ] Any stale asset path or skill names are removed from bootstrap docs/tests.
- [ ] If moved, architecture map and module README show the new owner.
- [ ] If kept legacy, task notes explain why and what would trigger migration.

## Open questions

- Is game creation/export a future AI Studio module or a root repo tool?
- Should bootstrap own asset source registration, or should it call an Asset
  Storage API only?

## Log

- 2026-07-01: Legacy scan found `tools/bootstrap/new_game.mjs`,
  `export_base.mjs`, `template_paths.mjs`, and `TEMPLATE.md` still carrying
  asset registration/export behavior. They are not direct asset-storage logic,
  but they are coupled to Asset Storage registries.
