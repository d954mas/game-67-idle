---
id: T0346
title: IDE generators assets and reports privacy boundary
status: done
project: P001
epic: E014
priority: P1
tags: [workspace, private-repos, assets, ide]
created: 2026-07-09
updated: 2026-07-09
---

## What

Harden generated parent Studio outputs so mounted private games do not leak
through IDE files, asset discovery, architecture maps, validation reports, or
runtime evidence.

## Done when

- [x] Tracked parent `.vscode` generation includes public/tracked games only.
- [x] Private-game IDE entries are generated inside the private game repo or
      ignored local workspace files.
- [x] Asset discovery can see private games only through explicit active
      workspace or `--include-private`, and does not write private asset metadata
      into tracked public files.
- [x] Asset/provenance/source-first rules remain unchanged for private games:
      committed assets still require license, provenance, integrity, and origin;
      paid or non-redistributable binaries stay out of git.
- [x] Architecture-map validation and reports do not scan ignored private nested
      repo internals from the parent Studio repo.
- [x] Generated validation reports, screenshots, runtime logs, and evidence paths
      are ignored or game-owned, and cannot become committed private path leaks.
- [x] The leak validator from `T0342` is run by relevant generator workflows or
      documented as the required preflight.
- [x] Tests or dry runs prove private registry entries do not appear in tracked
      parent `.vscode`, docs, reports, asset manifests, or generated evidence.
- [x] `node ai_studio/taskboard/cli.mjs validate --json` passes, or unrelated
      validation failures are recorded.

## Open questions

- Which generated reports should be completely public-only versus redacted
  aggregate reports?

## Log

- 2026-07-09: Created as child task from `T0341` review to handle non-Taskboard
  and non-Canvas leak surfaces.
- 2026-07-09: Started privacy-boundary implementation after T0345 closure commits 608846c78 and 2f2cbe845.
- 2026-07-09: Added parent `.vscode` regression coverage proving
  `ai_studio/workspace/games.local.json` private mounts do not enter tracked
  tasks or launch configs; documented that private IDE entries must stay
  game-owned or ignored local workspace state.
- 2026-07-09: Updated architecture-map validation to exclude private local game
  mounts from shallow parent scans before report/API output; added regression
  coverage proving `games/secret-game` does not appear in the report JSON.
- 2026-07-09: Added asset search `--game <game-id>` resolution through the
  workspace game resolver/preflight. This covers agent search; gallery and item
  viewer surfaces still need the same private-aware resolver before the asset
  discovery checklist item can close.
- 2026-07-09: Routed Asset Viewer gallery sources and Items Viewer catalog lists
  through workspace game mounts: public games remain visible by default,
  `include-private`/`game:<id>` explicitly exposes private mounts after
  preflight, and raw private `games/<id>/assets` paths are rejected.
- 2026-07-09: Added `ai_studio/dev_environment/vscode_projects.mjs --game
  <game-id>` for private mounted games. It writes game-local `.vscode` files
  inside `games/<id>/` after preflight and does not touch parent `.vscode`.
- 2026-07-09: Added workspace preflight regression coverage for generated
  report and evidence path leaks. Verified private ids stay out of parent
  `.vscode`, architecture reports, asset search/viewer source lists, and Items
  Viewer catalog lists unless explicitly selected through workspace mounts.
