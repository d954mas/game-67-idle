---
id: T0426
title: Shorten full verification critical path and new_game tests
status: done
project: P001
epic: E001
priority: P1
tags: [verification, performance, workspace, new-game]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"new_game 47/47 on Windows; real CLI and race boundaries retained; independent review pass; 10-domain Windows full pass"}]}
---

## What

Reduce avoidable work on the canonical Windows full-verification critical path
without increasing concurrency, weakening owner evidence, or adding a generic
test framework. Measure scheduler ordering, then extract only the existing
new-game transaction seam needed to run non-transport cases in-process.

## Done when

- [x] Measure heavy-first scheduling while preserving stable result order and concurrency two; revert it when it does not improve the critical path.
- [x] Compare workspace test concurrency and retain the faster existing setting.
- [x] Expose a structured new-game transaction entry point with guaranteed claim cleanup and unchanged CLI envelopes.
- [x] Move only non-CLI, non-lock, and non-race tests in-process; retain representative real process boundaries.
- [x] Pass focused workspace tests, independent review, and one final Windows full verification.
- [x] Record QTECH_001 evidence, before/after timings, and atomic commits.

## Open questions

- None. Stop the new-game slice if its measured gain does not justify the production complexity.

## Log

- 2026-07-14: Started from a clean T0425 tip. Baseline full Windows was 30.6 s;
  workspace 19.75 s; assets 15.04 s; isolated new_game 20.43 s on the noisy
  current host and 14.70 s in the prior focused baseline.
- 2026-07-14: Heavy-first scheduling was implemented behind a RED test, then
  measured at 31.9, 31.1, and 30.0 s (median 31.1 s). It did not beat the
  30.6 s baseline and was fully reverted; no execution-priority contract was
  added. Workspace concurrency 4 measured 13.71 s versus 15.83 s at
  concurrency 1, so the existing setting remains.
- 2026-07-14: `new_game` now exposes synchronous structured `main(argv, io)`.
  Claim release is guaranteed by `finally`, manual release removes the process
  exit listener, and the direct CLI maps the returned status to `exitCode`.
  Success and failure tests prove claim/listener cleanup.
- 2026-07-14: Removed 34 redundant Node CLI launches from behavior/rollback
  tests. One real public creation, help, unknown-argument error, both
  cross-process races, Git, filesystem, transaction, privacy, and rollback
  boundaries remain real. Focused results were 22.51 s immediately before and
  20.79/14.32/13.74 s after; process-count reduction is deterministic, while
  wall time is explicitly treated as host-noisy.
- 2026-07-14: Independent review found no P1/P2 issues and confirmed CLI,
  cleanup, environment restoration, and retained boundary coverage. Its P3
  failure-cleanup hardening was added. Final Windows full passed all applicable
  domains in 32.1 s; no aggregate speedup is claimed because Workspace, Assets,
  and template release fluctuated upward together.
- 2026-07-14: Stop decision: remaining time is real fixture copy, Git, native
  filesystem, Canvas/Python, and release-build work. Manual scheduler weights
  or test-only Git bypass/DI would add maintenance ceremony for an unstable
  1-3 s estimate, so neither was added.
- 2026-07-14: Quality: QTECH_001=pass; evidence: new_game 47/47 on Windows; real CLI/race boundaries retained; independent review pass; 10-domain Windows full pass
- 2026-07-14: Quality: QTECH_001=pass; evidence: new_game 47/47 on Windows; real CLI and race boundaries retained; independent review pass; 10-domain Windows full pass
