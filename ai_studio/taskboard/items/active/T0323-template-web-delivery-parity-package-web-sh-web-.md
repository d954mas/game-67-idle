---
id: T0323
title: "Add packaged-web browser load smoke to the template release flow"
status: backlog
project: P001
epic: E009
priority: P1
tags: [template, web, release, smoke]
created: 2026-07-06
updated: 2026-07-14
---

## What

The template now has cross-platform `tools/game.mjs`, deterministic web build,
ZIP packaging/verification, serving, DevAPI smoke helpers, and scenario hooks.
The remaining jam-proven gap is exercising the packaged artifact in a real
browser before release; prior failures were visible in the console and first
frame.

## Done when

- [ ] One `tools/game.mjs` release command builds and packages, reopens the ZIP,
      serves only its contents, and launches the supported headless browser.
- [ ] Smoke fails on page/console/resource errors, missing runtime readiness, or
      a blank/black first frame, and reports one compact diagnostic.
- [ ] Tests cover success and each failure class without requiring WSL; Windows
      is the canonical agent entry point and Linux uses the same Node command.
- [ ] A real template wasm-release package passes the smoke in CI.

## Open questions

- Smoke runs for release packaging, not for ordinary native/unit checks.

## Log

- 2026-07-14: Removed already delivered shell/script/scenario scope. The card
  now owns only the missing packaged-artifact browser proof.
- 2026-07-07: T0333 delivered the shared build/copy/server foundation used by
  the current Node tools; this task must extend it rather than reimplement it.
- 2026-07-06: Captured after a jam artifact loaded only on later attempts while
  browser console errors exposed the failure.
