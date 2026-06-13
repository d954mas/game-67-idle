---
id: T0032
title: Add fresh-start child-test launcher
status: review
epic: ""
priority: P1
tags: [release, child-test, packaging, ftue, validation]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a dedicated child-test launcher to the native PC release package that
starts 67 World from FTUE/fresh state and disables autosave for the test run.
The regular player launcher should remain unchanged, but the acceptance kit and
README should direct manual child-test sessions to the fresh launcher.

## Done when

- [x] Package generation emits `START_CHILD_TEST_FRESH.bat` using the existing
      runtime flags `--fresh-state --disable-autosave`.
- [x] README and `CHILD_TEST_ACCEPTANCE.md` tell testers to use the fresh-start
      launcher for manual child-test sessions.
- [x] Manifest, checksums, zip, and package self-check include the fresh-start
      launcher.
- [x] Package smoke validates the launcher is present and references the fresh
      runtime flags.
- [x] Child-test readiness reports whether the fresh-start launcher is present.
- [x] Package smoke and child-test readiness pass after regenerating the
      package.
- [x] `tasks/STATUS.md` points to the latest fresh-start evidence and keeps
      manual child-test/user acceptance as the remaining blocker.

## Open questions

None. Runtime already supports the required flags; this pass only adds the
tester-facing release launcher and validation.

## Log

- 2026-06-13: Started after package self-check reached review. Scope: release
  launcher/handoff only; no gameplay, balance, engine, or web changes.
- 2026-06-13: Added generated `START_CHILD_TEST_FRESH.bat` to the native
  package. It launches `67-world.exe --fresh-state --disable-autosave`, keeping
  the normal `RUN_67_WORLD.bat` unchanged for regular play.
- 2026-06-13: Updated generated `README.txt` and durable
  `gamedesign/meme-evolution/child_test_acceptance.md` to direct manual
  child-test sessions through `VERIFY_PACKAGE.bat` and
  `START_CHILD_TEST_FRESH.bat`.
- 2026-06-13: Updated `VERIFY_PACKAGE.ps1`, manifest, checksums, zip, package
  smoke, and child-test readiness package status to require/report the fresh
  child-test launcher.
- 2026-06-13: Static validation passed:
  `node --check tools/package_native_release.mjs` and
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/devapi/scenarios/child_test_readiness.py`.
- 2026-06-13: Regenerated package:
  `node tools/package_native_release.mjs` produced
  `build/release/67-world-pc/67-world/67-world.exe` (775168 bytes),
  `build/release/67-world-pc/67-world/assets/world67_art.ntpack`
  (20995020 bytes), and `build/release/67-world-pc/67-world-pc.zip`
  (21782184 bytes).
- 2026-06-13: Package smoke passed:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9304 build/captures/scenarios/package_release_smoke_v8_fresh_launcher.png`.
  It verified package files, manifest, checksums, zip,
  `START_CHILD_TEST_FRESH.bat`, fresh/no-autosave flags, packaged self-check,
  branded exe metadata/resources, and release launch screenshot.
- 2026-06-13: Child-test readiness passed:
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9308 build/reports/child_test_readiness_v15_fresh_launcher.json build/captures/scenarios/child_test_readiness_v15_fresh_launcher`.
  Report result: `automated_review_passed=true`, package ok with fresh launcher,
  self-check, acceptance kit, manifest, checksums and zip,
  `ready_for_manual_child_test=true`, `release_ready=false`.
- 2026-06-13: Pixel health passed for
  `build/captures/scenarios/package_release_smoke_v8_fresh_launcher.png` and
  all five v15 readiness screenshots under
  `build/captures/scenarios/child_test_readiness_v15_fresh_launcher/`.
- 2026-06-13: Direct self-check run passed:
  `powershell -NoProfile -ExecutionPolicy Bypass -File build/release/67-world-pc/67-world/VERIFY_PACKAGE.ps1`
  printed `PASS 67 World package self-check`, checked files `9`, and directed
  child-test sessions to `START_CHILD_TEST_FRESH.bat`.
  Remaining blocker: manual child-test/user acceptance.
