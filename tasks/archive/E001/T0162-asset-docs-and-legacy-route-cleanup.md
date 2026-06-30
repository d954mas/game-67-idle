---
id: T0162
title: Asset docs and legacy route cleanup
status: done
epic: E001
priority: P2
tags: [assets, legacy, docs]
created: 2026-07-01
updated: 2026-06-30
---

## What

Review the remaining asset-related legacy routes after closing the main asset
refactor tasks. Keep current AI Studio asset ownership clear, remove stale
asset workflow wording, and record what is still intentionally outside the
asset module.

## Done when

- [x] Remaining current docs do not route agents to removed asset tools, old OKF
      catalog flow, or stale per-game asset folder lists.
- [x] Bootstrap/template docs describe game-local assets without hard-coding the
      removed `assets/source|catalog|licenses|previews|runtime|packs` shape.
- [x] Any remaining asset mentions outside `ai_studio/assets/` are classified as
      current routing, historical archive, test fixture, or non-asset domain
      context.
- [x] Task log records the review evidence and next action.
- [x] Taskboard and architecture map validation pass.

## Open questions

- Should `tools/game_context/` become a future AI Studio module, or stay as
  runtime/game startup tooling until the technical workflow refactor?

## Log

- 2026-07-01: Started after closing T0151-T0161. Current asset ownership lives
  under `ai_studio/assets/`, `.codex/skills/nt-asset-workflow`, and
  `.codex/skills/nt-asset-image-generation`. Legacy asset task history moved to
  archive.
- 2026-07-01: Initial scan found no current `tools/assets`, `tools/lib/licenses`,
  `download_source_asset`, `accept_incoming_asset`, old OKF navigation, or old
  asset skill entrypoints in live routing. Remaining live cleanup target:
  `ai_studio/bootstrap/TEMPLATE.md` still had a stale game-only asset folder
  list and non-ASCII punctuation that rendered poorly in PowerShell.
- 2026-07-01: Cleaned `ai_studio/bootstrap/TEMPLATE.md` game-only asset wording.
  Current legacy asset route scan now finds only this task and archive history
  for removed asset tools/OKF/old skills. Current non-archive asset mentions are
  valid routing (`ai_studio/assets`, `nt-asset-workflow`), test fixtures, or
  non-asset `tools/game_context` startup context.
- 2026-07-01: Validation passed: `node ai_studio/taskboard/cli.mjs validate
  --json` and `node ai_studio/architecture_map/validate_map.mjs`.
- 2026-06-30: 2026-07-01: Closed after live asset legacy scan and bootstrap template cleanup; remaining removed-tool/OKF/old-skill references are only in task archive history.
