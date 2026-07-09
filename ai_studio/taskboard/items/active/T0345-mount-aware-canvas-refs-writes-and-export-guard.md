---
id: T0345
title: Mount-aware Canvas refs writes and export guard
status: doing
project: P001
epic: E014
priority: P1
tags: [canvas, private-repos, workspace]
created: 2026-07-09
updated: 2026-07-09
---

## What

Make Canvas mount-aware for game-owned project roots.

This slice owns Canvas refs, listing, mutation/write routing, export routing,
and leak prevention for private Canvas projects.

## Done when

- [ ] Canvas can mount public reusable projects plus
      `games/<id>/.ai_studio/canvas/projects/` from the workspace resolver.
- [ ] Canvas refs have a v2 mount-aware form such as
      `canvas://game/<id>/<projectId>`; old `canvas://<projectId>` refs have an
      alias/migration path.
- [ ] Canvas list/show APIs include source store identity and visibility.
- [ ] Every write, mutate, import, export, render report, manifest, and
      `tool_runs.jsonl` update targets the owning store root explicitly.
- [ ] Private Canvas projects are excluded from aggregate listings unless active
      workspace or `--include-private` is provided.
- [ ] CLI commands using bare project IDs reject ambiguous IDs across stores.
- [ ] Export delivery to a parent tracked path is hard-rejected for private
      stores; there is no unsafe override for this path.
- [ ] Canvas project folders migrate as complete units: `project.json`, `files/`,
      `export/`, `tool_runs.jsonl`, `errors.jsonl`, and local chat/history
      sidecars.
- [ ] Tests or dry-run fixtures cover public project listing, explicit private
      listing, store-routed mutation, ambiguous ID rejection, and private export
      destination rejection.
- [ ] `node ai_studio/taskboard/cli.mjs validate --json` passes, or unrelated
      validation failures are recorded.

## Open questions

- Should private export to a non-parent ignored local directory be allowed, or
  should private exports always stay inside the owning game store?

## Log

- 2026-07-09: Created as child task from `T0341` review to close the Canvas
  side-channel risk before any current-game canvas migration.
- 2026-07-09: Closed unsafe override gap: private exports to parent tracked
  destinations must be rejected, not overridden.
- 2026-07-09: Started after T0344 store-qualified Taskboard commit fc3da87c5.
- 2026-07-09: Canvas store-routing slice: CLI create/show/list/mutations can select private game stores; private --to/--zip exports into parent Studio repo are rejected, including Windows path-case variants. Verified with canvas/private-games tests and architecture-map validation.
