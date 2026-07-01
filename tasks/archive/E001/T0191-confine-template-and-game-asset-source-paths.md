---
id: T0191
title: Confine template and game asset source paths
status: done
epic: E001
priority: P1
tags: [assets, storage, sources, test]
created: 2026-07-01
updated: 2026-07-01
---

## What

Template and game asset sources are repository-owned source roots. Their
registered `folder` and `assets` paths should stay repo-relative and must not
escape the repository through absolute paths or `..` traversal. Libraries remain
allowed to point at external shared storage.

## Done when

- [x] Registering a template with absolute or escaping paths fails clearly.
- [x] Registering a game with absolute or escaping paths fails clearly.
- [x] Existing template/game/library source behavior still passes.
- [x] Source registry docs state the path boundary.

## Open questions

## Log

- 2026-07-01: Created after review found template/game registries normalized
  paths but did not reject repo escapes at registration time.
- 2026-07-01: Added repo-relative path guards for template/game source
  registration and tests for `../` and absolute asset roots.
- 2026-07-01: Validation passed: source registry tests 14/14, asset viewer tests
  28/28, asset storage/source/license/intake/manifest/snapshot/preview tests
  56/56, syntax checks for source registry modules, `validate_map.mjs
  --strict`, `doc_reference_check.mjs`, and taskboard `validate --json`.
