---
id: T0188
title: Harden asset gallery builder CLI contract
status: done
epic: E001
priority: P1
tags: [assets, viewer, gallery, test]
created: 2026-07-01
updated: 2026-07-01
---

## What
`build_review.mjs` is already import-safe, but its CLI contract is only partly
testable. `parseArgs` is private, `main()` reads `process.argv` directly, and an
unknown `--mode` currently falls into the library branch. Make the contract
explicit without changing gallery output behavior.

## Done when

- [x] `parseArgs` is exported and covered for normal options, `--ref`, unknown
      options, missing values, and invalid modes.
- [x] `main(argv)` can be called with explicit argv.
- [x] Unknown `--mode` values fail before doing filesystem work.
- [x] Focused viewer/storage tests, map validation, doc references, and
      taskboard validation pass.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after hardening standalone viewer helpers; this keeps the
  static gallery builder's CLI behavior explicit and testable.
- 2026-07-01: Exported `parseArgs`/`main`, added mode validation, and covered
  normal options, `--ref`, missing values, unknown options, and invalid modes in
  `build_review.test.mjs`.
- 2026-07-01: Validated 24 focused viewer tests, 33 focused storage tests,
  architecture map, markdown references, and taskboard state.
- 2026-07-01: Hardened Asset Viewer build_review CLI contract with exported parser/main, mode validation, and focused tests.
