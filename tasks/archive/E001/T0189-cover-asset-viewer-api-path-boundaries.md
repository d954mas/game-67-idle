---
id: T0189
title: Cover asset viewer API path boundaries
status: done
epic: E001
priority: P1
tags: [assets, viewer, api, test]
created: 2026-07-01
updated: 2026-07-01
---

## What
Asset Viewer API is the Studio Shell-facing surface for source lists, gallery
media paths, lazy asset queries, model lookups, reindex, and preview refresh.
Its pure path/source boundary helpers are currently untested even though they
protect custom game paths and `/asset_viewer/gallery/...` media routing.

## Done when

- [x] Path/source helper exports stay small and do not turn API internals into a
      broad public surface.
- [x] Tests cover safe in-repo paths, traversal rejection, absolute external
      custom game paths, gallery local files, gallery library files, repo files,
      and unknown routes.
- [x] Architecture tree maps the new API test.
- [x] Focused viewer/storage tests, map validation, doc references, and
      taskboard validation pass.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after hardening standalone Asset Viewer helpers; API path
  boundaries are the next surface that should be explicit and tested.
- 2026-07-01: Exported only `safeResolve` and `selectSource` as small API
  boundary helpers, added `api.test.mjs`, and mapped it in `ai_studio/tree.json`.
- 2026-07-01: Validated 27 focused viewer tests, 45 focused storage/source/
  license tests, architecture map, markdown references, and taskboard state.
- 2026-07-01: Covered Asset Viewer API path/source boundaries with tests and mapped the new API boundary test in the architecture tree.
