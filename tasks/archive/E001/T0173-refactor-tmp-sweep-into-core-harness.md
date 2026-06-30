---
id: T0173
title: Refactor tmp sweep into core harness
status: done
epic: E001
priority: P2
tags: [core-harness, tool-lib, tmp, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What
Move legacy `tools/tmp_sweep.mjs` into Core Harness Tool Lib, because it is
shared scratch housekeeping tied to `tmp_exports.mjs`, not a standalone legacy
tool. Keep it explicit and opt-in: report by default, delete only with
`--all-scratch`.

## Done when

- [x] Reviewed `tmp_sweep` behavior and ownership.
- [x] Moved command and tests under `ai_studio/core_harness/tool_lib/`.
- [x] Updated imports, command help, and test spawn path.
- [x] Added the command/test to `ai_studio/tree.json`.
- [x] Updated Tool Lib documentation.
- [x] Validated tmp sweep tests, architecture map, doc references, and taskboard.
- [x] Committed and pushed the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after `validate_map` showed `tools/tmp_sweep.mjs` and
  `tools/tmp_sweep.test.mjs` as the only remaining unmapped legacy tool files.
- 2026-07-01: Reviewed behavior: `--list` reports ignored `tmp/` scratch,
  `--all-scratch` deletes opt-in, and `--keep-validate` keeps newest pipeline
  validation exports through shared `tmp_exports.mjs`.
- 2026-07-01: Moved command/test to
  `ai_studio/core_harness/tool_lib/`, updated root resolution/imports/help/test
  path, added map entries, and documented commands in Tool Lib.
- 2026-07-01: Validation passed: `tmp_sweep` tests, `tmp_exports` tests,
  architecture map validation, doc reference check, and taskboard validation.
- 2026-06-30: Moved tmp_sweep into Core Harness Tool Lib, updated map/docs, and validated tests/map/docs/taskboard.
