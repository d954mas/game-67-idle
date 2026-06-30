---
id: T0161
title: Bootstrap export asset ownership is reviewed
status: done
epic: ""
priority: P2
tags: [assets, bootstrap, legacy]
created: 2026-06-30
updated: 2026-06-30
---

## What

Review legacy bootstrap/export behavior after the asset module refactor. Move
project/game creation into an AI Studio Bootstrap module and keep Asset Storage
as the owner of asset registries.

## Done when

- [x] Asset-related bootstrap/export responsibilities are classified.
- [x] Any stale asset path or skill names are removed from bootstrap docs/tests.
- [x] If moved, architecture map and module README show the new owner.
- [x] If kept legacy, task notes explain why and what would trigger migration.

## Open questions

- Resolved: game creation/export belongs to `ai_studio/bootstrap/`.
- Resolved: bootstrap may call Asset Storage registry helpers, but Asset Storage
  owns source registries, manifests, license policy, previews, search, and viewer behavior.

## Log

- 2026-07-01: Legacy scan found `tools/bootstrap/new_game.mjs`,
  `export_base.mjs`, `template_paths.mjs`, and `TEMPLATE.md` still carrying
  asset registration/export behavior. They are not direct asset-storage logic,
  but they are coupled to Asset Storage registries.
- 2026-06-30: Started bootstrap ownership migration: moving tools/bootstrap into ai_studio/bootstrap and updating docs/tests/tree references.
- 2026-07-01: Moved bootstrap/export files into `ai_studio/bootstrap/`, added
  module README, updated public commands/docs/tests/tree references, and removed
  bootstrap from the Not Refactored map.
- 2026-07-01: Validation passed: bootstrap node tests, architecture map
  validation, taskboard validation, doc reference check, and skills sync check.
- 2026-06-30: 2026-07-01: Review closeout passed on current state: asset JS tests 112/112, Python prep tests 65/65, restricted asset guard dev/release green, architecture map validation clean, taskboard validation clean, doc reference check clean, skills sync clean.
