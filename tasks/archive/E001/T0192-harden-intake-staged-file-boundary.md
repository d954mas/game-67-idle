---
id: T0192
title: Harden intake staged file boundary
status: done
epic: E001
priority: P1
tags: [assets, intake, security, test]
created: 2026-07-01
updated: 2026-07-01
---

## What

`accept.mjs` resolves `--file` under the staged intake folder, but the boundary
check used a plain string prefix. A sibling folder with the same prefix as the
staged folder name can pass that check. Intake acceptance must require the
selected file to be exactly inside the staged directory.

## Done when

- [x] `--file ../<same-prefix-sibling>/...` is rejected.
- [x] Normal stage/accept/reject intake flows still pass.
- [x] Focused asset storage checks and core map/doc/taskboard checks pass.

## Open questions

## Log

- 2026-07-01: Created after finding prefix-based path containment in asset
  intake accept.
- 2026-07-01: Replaced string-prefix containment with strict resolved path
  containment and added a same-prefix sibling regression test.
- 2026-07-01: Validation passed: intake tests 5/5, asset viewer tests 28/28,
  asset storage/source/license/intake/manifest/snapshot/preview tests 57/57,
  `node --check ai_studio/assets/storage/intake/accept.mjs`,
  `validate_map.mjs --strict`, `doc_reference_check.mjs`, and taskboard
  `validate --json`.
