---
id: T0430
title: Review and simplify Taskboard architecture and tests
status: done
project: P001
epic: E001
priority: P1
tags: [taskboard, architecture, tests, performance, simplification]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=69 Taskboard tests, fresh browser smoke, full Windows verify, and two independent reviews"}]}
---

## What

Review the complete Taskboard feature boundary after the AI Studio refactor.
Map every production file and test, measure the real CLI/store/API/test costs,
identify duplication and ceremony, and implement only simplifications that keep
the markdown source of truth, store privacy, Quality closeout, and compact UI.

## Done when

- [x] Every production file has a concrete owner, responsibility, dependency,
      consumer, and keep/merge/delete judgment.
- [x] Every Taskboard test is reviewed for unique value, duplication, boundary,
      process cost, and acceleration opportunity.
- [x] CLI, store, aggregate-store, HTTP, browser, lifecycle, Quality, and context
      contracts are checked end to end.
- [x] Baseline timings and process counts exist before implementation.
- [x] Accepted simplifications are implemented without a compatibility layer or
      new framework.
- [x] Focused tests, store validation, browser smoke, full Windows verification,
      and independent review pass.

## Open questions

- Are `lib.mjs`, `profile.mjs`, and `stores.mjs` all earning separate modules?
- Does one broad test file hide redundant coverage or reduce process overhead?
- Which browser/API behavior is product-essential versus historical ceremony?

## Log

- 2026-07-14: Started from the accepted post-verification plan. Scope is the
  Taskboard feature boundary only; no UI redesign, storage replacement, or
  weakening of Quality/private-store behavior is assumed.
- 2026-07-14: Reviewed all 11 production/UI/docs files and all 65 baseline
  tests. Kept the public facade, API, store, aggregate-store, UI, and docs
  boundaries; merged the one-function profiler into `stores.mjs`; removed one
  dead CSS rule. Deliberately did not split `store.mjs`, add a fixture cache, or
  make public validation discover private stores.
- 2026-07-14: Baseline Taskboard test file was 65 tests / 4.251 s locally. The
  final suite is 69 tests / 3.158 s (25.7% faster) with path, failure, privacy,
  and concurrent-close regressions covered. The allocation race still covers
  all three ID kinds with 12 concurrent processes; two pure aggregate tests use
  explicit store descriptors while CLI/API discovery seams remain
  integration-tested.
- 2026-07-14: Fixed archive path confinement and failure consistency. Only a
  canonical terminal `E####` becomes an archive directory; invalid refs go to
  `unassigned`. A move prepares its destination before touching the source,
  keeps the prior atomic `rename`, and restores the original source bytes after
  a synchronous rename failure.
- 2026-07-14: Fresh Windows Studio Shell browser smoke passed for Board,
  Projects, Epics, Stores, and Statuses; `/api/board?includePrivate=1` returned
  200 and Chromium reported zero console warnings/errors.
- 2026-07-14: Two independent final reviews report no residual P0-P3. Final
  `node ai_studio/studio.mjs verify --full` passed all nine applicable domains;
  design was correctly not-applicable.
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=69 Taskboard tests, fresh browser smoke, full Windows verify, and two independent reviews
