---
id: T0158
title: Asset license registry no longer depends on tools shim
status: review
epic: E001
priority: P2
tags: [assets, licensing, legacy]
created: 2026-06-30
updated: 2026-06-30
---

## What

Asset license decisions are now owned by
`ai_studio/assets/storage/license/registry.mjs`. The old `tools/lib/licenses.mjs`
compatibility export should not stay in the asset runtime path or portable
export base.

## Done when

- [x] Asset promotion imports license URLs and decisions from the canonical
      Asset Storage registry.
- [x] `tools/lib/licenses.mjs` compatibility shim is removed.
- [x] `tools/bootstrap/export_base.mjs` no longer exports the removed shim.
- [x] Asset promotion tests pass.
- [x] Export base test passes.
- [x] Architecture map and taskboard validation pass.

## Open questions

## Log
- 2026-07-01: Found `ai_studio/assets/viewer/promote.mjs` importing license URLs through `tools/lib/licenses.mjs`; this kept a legacy tools shim in the asset module.
- 2026-07-01: Removed the shim and switched `promote.mjs` to import directly from `ai_studio/assets/storage/license/registry.mjs`.
- 2026-07-01: Validation: `promote.test.mjs` 9/9, `export_base.test.mjs` 1/1, taskboard validate, and architecture map validate passed. No current code/docs reference the removed shim outside this task's historical notes.
