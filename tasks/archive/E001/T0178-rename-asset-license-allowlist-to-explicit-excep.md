---
id: T0178
title: Rename asset license allowlist to explicit exceptions
status: done
epic: E001
priority: P2
tags: [assets, licensing, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What
The asset license guard still names its narrow skip mechanism as an allowlist and
mentions legacy assets. The current system should not preserve legacy asset
routes. Keep the mechanism only as an empty-by-default explicit exception list
for already reviewed public binaries that cannot yet be represented by a
manifest.

## Done when

- [x] Rename the guard config from allowlist to explicit exceptions.
- [x] Update guard messages and tests to remove legacy/allowlist wording.
- [x] Document that new assets must use manifests or restricted storage, not
      exceptions.
- [x] Update the architecture tree.
- [x] Validate license guard tests, guard CLI, map, docs, and taskboard.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after asset legacy scan found the live license guard still
  using allowlist/legacy wording while the actual list is empty.
- 2026-07-01: Renamed the config to `restricted_assets_exceptions.json`,
  updated guard API/test wording to `exceptionPrefixes`, confirmed live
  `ai_studio`/skill search has no allowlist references, and validated the
  license guard tests, guard CLI, map, docs, and taskboard.
- 2026-07-01: Renamed asset license guard allowlist to explicit exceptions;
  tests, guard CLI, map, docs, and taskboard validated.
