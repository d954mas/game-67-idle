---
id: T0198
title: Review current asset module and legacy asset leftovers
status: todo
epic: E001
priority: P2
tags: [assets, review, legacy]
created: 2026-07-01
updated: 2026-07-01
---

## What

Review the current `ai_studio/assets` architecture and the remaining legacy
asset-related files outside it. Identify what is already correctly migrated,
what should stay outside as game/template/runtime data, what should be deleted,
and what should become the next refactor task.

## Done when

- [ ] Current `ai_studio/assets` modules are mapped by ownership.
- [ ] Legacy asset-related files outside `ai_studio/assets` are identified.
- [ ] Findings separate bugs, cleanup, architecture debt, and intentional
      non-AI-Studio assets.
- [ ] Next concrete action is recorded.

## Open questions

## Log

- 2026-07-01: Started after asset viewer/storage commits; review is scoped to
  current asset module structure plus legacy leftovers.
- 2026-07-01: Paused before completion because the repo layout decision
  `templates/` + `games/` became a prerequisite for asset/template/game review.
