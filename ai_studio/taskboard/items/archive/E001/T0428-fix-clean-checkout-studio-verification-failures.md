---
id: T0428
title: Fix clean-checkout Studio verification failures
status: done
project: P001
epic: E001
priority: P0
tags: [ci, verification, windows, linux, tests]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"Focused suites, native CTest 30/30, Windows full 23.215s, independent review, and GitHub Actions run 29316401973 passed on Windows and Ubuntu"}]}
---

## What

Restore green Studio verification on clean GitHub Actions checkouts without
weakening owner-domain evidence or adding CI-only setup ceremony. Fix the two
cross-platform hidden test prerequisites and keep Linux sanitizers scoped to
the runtime code they are intended to validate.

## Done when

- [x] Relative-anchor tests own their ignored `tmp/` prerequisite on a clean checkout.
- [x] Items Viewer tests do not require an unrelated pre-existing template build.
- [x] Linux game/runtime ASan, UBSan, and LSan coverage remains active; only
      the known external GLFW/GLX extension cache is suppressed for the pack builder.
- [x] Focused suites and native Windows full verification pass.
- [x] Independent review finds no quality regression.
- [x] Commit is pushed and Windows/Ubuntu Studio verification is green.

## Open questions

- Is the Ubuntu leak an engine lifecycle defect, or a template sanitizer-scope
  defect caused by global executable linker flags?

## Log

- 2026-07-14: GitHub Actions run 29315017750 failed on both platforms because
  `test_expand_jobs.py` assumed ignored `repo/tmp` existed and the Items Viewer
  live test assumed `templates/template/build/native-debug` existed. Ubuntu
  additionally applied LeakSanitizer to `build_game_packs` and reported a
  128-byte GLFW/GLX allocation through the engine shader builder.
- 2026-07-14: The relative-anchor test now creates its ignored repo `tmp/`
  parent; its Python file passed `20/20`. Removed the live native-build icon
  assertions from the Items Viewer catalog integration test because the same
  real pack and all six icon regions are already covered by the committed
  `icon_preview` fixture suite; the two focused files passed `26/26`.
- 2026-07-14: Engine inspection confirmed `glfwDestroyWindow` and
  `glfwTerminate` run. The 128-byte allocation is GLFW/GLX's external extension
  cache, already suppressed by exact signature in engine builder/window tests.
  The template pack builder now owns the same Linux-only
  `leak:extensionSupportedGLX` suppression; no engine issue or broad
  `detect_leaks=0` workaround is needed.
- 2026-07-14: `build_game_packs` now explicitly receives the same target-level
  sanitizer compile/link policy as the game, native tests, and engine modules.
  A broader removal of the cached global linker flag was tested, then reverted:
  it was not needed for this fix and would require migration ceremony for old
  CMake trees. The existing cache contract stays unchanged.
- 2026-07-14: A fresh native-debug configure/build completed 373 targets and
  CTest passed `30/30`. Changed-owner verification passed harness, assets,
  work-management, and template-release. Final native Windows full verification
  passed all applicable owner domains in `32.028 s` after the cold rebuild.
- 2026-07-14: Independent review found two P2 evidence/scope gaps. Kept the
  existing global CMake cache contract to avoid a partial migration, and added
  a clean temp-folder assertion proving `loadCatalogView` returns the honest
  `view.icons` no-pack envelope. Post-fix focused suites passed `65/65` Node,
  `22/22` runtime Python, and `20/20` anchor Python tests.
- 2026-07-14: Reviewer recheck found both P2s resolved and no residual P1/P2.
  Repeat native Windows full verification passed all applicable domains in
  `23.215 s` on the final pre-push diff.
- 2026-07-14: Commit `6843fe7f7` pushed to `origin/master`. GitHub Actions run
  `29316401973` passed: Ubuntu blocking verification `2m50s` and job `4m16s`;
  Windows blocking verification `3m34s` and job `5m41s`.
- 2026-07-14: Quality: QTECH_001=pass; evidence: clean-checkout prerequisites
  removed, exact external GLX suppression retained all other sanitizer checks,
  focused suites and Windows full passed, independent review clean, Windows and
  Ubuntu GitHub Actions green.
- 2026-07-14: Started from GitHub Actions run 29315017750 failure evidence.
- 2026-07-14: CI fix shipped in 6843fe7f7; run 29316401973 green on both platforms.
- 2026-07-14: Quality: QTECH_001=pass; evidence: Focused suites, native CTest 30/30, Windows full 23.215s, independent review, and GitHub Actions run 29316401973 passed on Windows and Ubuntu
