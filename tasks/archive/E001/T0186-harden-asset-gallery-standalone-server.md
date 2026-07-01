---
id: T0186
title: Harden asset gallery standalone server
status: done
epic: E001
priority: P1
tags: [assets, viewer, server, test]
created: 2026-07-01
updated: 2026-07-01
---

## What
`serve_gallery.mjs` is a secondary Asset Viewer export helper, but it currently
parses CLI args and starts an HTTP server at module import time. That makes the
helper hard to test and keeps path routing unverified. Harden it without turning
it into another primary surface.

## Done when

- [x] `serve_gallery.mjs` can be imported without starting a server.
- [x] Path resolution for `/`, gallery files, `/lib/` files, query strings, and
      traversal attempts is covered by tests.
- [x] The architecture tree maps the new test.
- [x] Focused viewer/storage tests, map validation, doc references, and
      taskboard validation pass.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after the asset legacy audit identified standalone
  viewer helpers as secondary surfaces that should stay small and testable.
- 2026-07-01: Made `serve_gallery.mjs` import-safe, exported its parser/server
  helpers, replaced prefix path checks with confined target resolution, and
  added traversal/malformed-URL tests.
- 2026-07-01: Validated focused Asset Viewer tests, syntax check, architecture
  map, markdown references, and taskboard state.
- 2026-07-01: Also validated focused Asset Storage index/search/license tests.
- 2026-07-01: Hardened standalone gallery server import and request path handling; added serve_gallery tests and mapped them in architecture tree.
