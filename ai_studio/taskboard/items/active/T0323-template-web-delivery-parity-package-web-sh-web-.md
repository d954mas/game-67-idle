---
id: T0323
title: "Add packaged-web browser load smoke to the template release flow"
status: review
project: P001
epic: E009
priority: P1
tags: [template, web, release, smoke]
created: 2026-07-06
updated: 2026-07-17
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"Independent review ACCEPT; packaged-browser focused tests 18/18; full focused owner set 43/43; real Windows Chrome package ready with matching runtime fingerprint and rendered frame; Studio verify --full passed all 10 domains; PR #24 exact implementation tip CI passed Ubuntu and Windows."}]}
---

## What

The template now has cross-platform `tools/game.mjs`, deterministic web build,
ZIP packaging/verification, serving, DevAPI smoke helpers, and scenario hooks.
The remaining jam-proven gap is exercising the packaged artifact in a real
browser before release; prior failures were visible in the console and first
frame.

## Done when

- [x] One `tools/game.mjs` release command builds and packages, reopens the ZIP,
      serves only its contents, and launches the supported headless browser.
- [x] Smoke fails on page/console/resource errors, missing runtime readiness, or
      a blank/black first frame, and reports one compact diagnostic.
- [x] Tests cover success and each failure class without requiring WSL; Windows
      is the canonical agent entry point and Linux uses the same Node command.
- [x] A real template wasm-release package passes the smoke in CI.

## Open questions

- Smoke runs for release packaging, not for ordinary native/unit checks.

## Log

- 2026-07-17: Selected from the Taskboard readyQueue after T0442 reached
  review. The slice extends the existing `tools/game.mjs verify` release path:
  reopen the exact ZIP, serve only its entries on loopback, launch the supported
  headless browser, and fail closed on browser/runtime/first-frame evidence.
- 2026-07-17: TDD implementation complete. `game verify` now reopens the exact
  package into an in-memory loopback server, launches isolated Chrome/Chromium
  over the official CDP pipe, requires document/overlay/WASM fingerprint
  readiness, captures page/console/resource plus socket/direct-network errors,
  and decodes the clipped canvas PNG to reject blank/black first frames. Focused
  owner tests pass 43/43; a real Windows itch package reached ready with matching
  runtime/compiled fingerprint, a 1280x720 frame (luma 20..255, variance 194),
  and no issues. Browser process-tree/profile cleanup was verified 0 before/0
  after. Independent re-review ACCEPT; QTECH_001=pass. Final local `verify
  --full` passed all 10 domains; the exact GitHub CI tip remains pending.
- 2026-07-17: First PR #24 CI attempt passed Ubuntu in 3m48s; Windows
  reached the browser-smoke-enabled template-release gate but failed an older
  T0442 fake-socket cleanup timing assertion. The bounded cleanup reservation
  was fixed in T0442, stress-tested 20/20, independently reviewed ACCEPT, and
  full local verify returned to 10/10. Exact-tip CI rerun remains pending.
- 2026-07-17: PR #24 exact implementation tip `9bf4e694c` passed the
  browser-smoke-enabled full GitHub Actions matrix: Ubuntu 4m30s and Windows
  5m30s (run 29624784705). All done criteria are now met; moved to review.
- 2026-07-14: Removed already delivered shell/script/scenario scope. The card
  now owns only the missing packaged-artifact browser proof.
- 2026-07-07: T0333 delivered the shared build/copy/server foundation used by
  the current Node tools; this task must extend it rather than reimplement it.
- 2026-07-06: Captured after a jam artifact loaded only on later attempts while
  browser console errors exposed the failure.
