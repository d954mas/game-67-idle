---
id: T0345
title: Mount-aware Canvas refs writes and export guard
status: done
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

- [x] Canvas can mount public reusable projects plus
      `games/<id>/.ai_studio/canvas/projects/` from the workspace resolver.
- [x] Canvas refs have a v2 mount-aware form such as
      `canvas://game/<id>/<projectId>`; old `canvas://<projectId>` refs have an
      alias/migration path.
- [x] Canvas list/show APIs include source store identity and visibility.
- [x] Every write, mutate, import, export, render report, manifest, and
      `tool_runs.jsonl` update targets the owning store root explicitly.
- [x] Private Canvas projects are excluded from aggregate listings unless active
      workspace or `--include-private` is provided.
- [x] CLI commands using bare project IDs reject ambiguous IDs across stores.
- [x] Export delivery to a parent tracked path is hard-rejected for private
      stores; there is no unsafe override for this path.
- [x] Canvas project folders migrate as complete units: `project.json`, `files/`,
      `export/`, `tool_runs.jsonl`, `errors.jsonl`, and local chat/history
      sidecars.
      Note: `chat/` is project-owned; undo/history cache is per-machine local
      cache and its preservation/loss is an explicit `T0347` migration decision,
      not a parent-public leak.
- [x] Tests or dry-run fixtures cover public project listing, explicit private
      listing, store-routed mutation, ambiguous ID rejection, and private export
      destination rejection.
- [x] `node ai_studio/taskboard/cli.mjs validate --json` passes, or unrelated
      validation failures are recorded.

## Open questions

- Resolved for this slice: private exports may stay inside the owning game store
  or outside the parent Studio repository; delivery into any parent Studio path
  outside the owning game root is hard-rejected.

## Log

- 2026-07-09: Created as child task from `T0341` review to close the Canvas
  side-channel risk before any current-game canvas migration.
- 2026-07-09: Closed unsafe override gap: private exports to parent tracked
  destinations must be rejected, not overridden.
- 2026-07-09: Started after T0344 store-qualified Taskboard commit fc3da87c5.
- 2026-07-09: Canvas store-routing slice: CLI create/show/list/mutations can select private game stores; private --to/--zip exports into parent Studio repo are rejected, including Windows path-case variants. Verified with canvas/private-games tests and architecture-map validation.
- 2026-07-09: Canvas API store-routing slice: /api/canvas/projects is public-only by default, ?include-private aggregates explicit private stores, and ?game/ ?store wraps project/file/mutation routes in the selected Canvas store. Verified targeted API route tests plus existing public API smoke tests.
- 2026-07-09: Canvas browser store-scope slice: site API helper, file/export URLs, deep links, last-project restore, image cache keys, and Copy ID refs now preserve selected game stores; private Copy ID refs omit project/object name tails. Verified site_store_scope tests and browser module syntax checks.
- 2026-07-09: UX privacy correction after lead review: Canvas and Taskboard browser surfaces now discover mounted stores from the server and expose store selectors/filters instead of requiring visible `?store=`/`?game=` URLs. Normal browser API requests route private stores through `x-ai-studio-store`; project URLs stay `/canvas?project=<id>`, and Taskboard create/update calls route by selected row/store identity. Verified Canvas site/API tests and full Taskboard test file.
- 2026-07-09: Closed browser binary URL gap for Canvas images: private thumbnails and canvas image cache now fetch files with `x-ai-studio-store` and render `blob:` URLs instead of embedding `?store=game:<id>` in `<img>` paths. Export fetches also use headers; private pinned result links are suppressed because anchors cannot carry headers.
- 2026-07-09: Closed Canvas chat store-scope gap: chat transcript/context/cancel routes now resolve private projects through `x-ai-studio-store`, private refs use `canvas://game/<gameId>/<projectId>/...` without name tails, and spawned chat prompts require `--store game:<id>` on private Canvas CLI commands.
- 2026-07-09: Closed the remaining CLI ambiguity gap: bare project IDs now
  reject private-only projects and duplicate IDs across mounted stores before
  lock/mutation; users must pass `--store` or `--game`. Verification:
  targeted Canvas CLI/API/site/store tests (15 passing), chat tests (56
  passing), `node ai_studio/taskboard/cli.mjs validate --json`, and
  `git diff --check`.
- 2026-07-09: Completed mount-aware Canvas refs/writes/export guard after adding bare-ID ambiguity refusal. Verification: targeted Canvas store/privacy tests, chat store-scope tests, taskboard validate, git diff --check.
- 2026-07-09: Follow-up review caveat: stale Canvas skill docs were updated for
  `canvas://game/<gameId>/...` refs and `--store game:<id>`. The local
  undo/history cache is documented as per-machine/non-portable; `T0347` now owns
  any current-game cache-preservation decision during migration.
