---
id: T0031
title: Add packaged release self-check for testers
status: review
epic: ""
priority: P1
tags: [release, packaging, qa, validation, windows]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a tester-facing self-check to the native PC release package. The self-check
should run on Windows from the extracted package folder, verify required files,
manifest fields, and SHA-256 checksums, then tell the tester whether the package
is intact before starting the child-test.

## Done when

- [x] Package generation emits `VERIFY_PACKAGE.ps1` and a double-clickable
      `VERIFY_PACKAGE.bat`.
- [x] README tells testers to run the self-check before the game.
- [x] Manifest, checksums, and zip include the self-check files.
- [x] Package smoke runs the self-check from the packaged folder and fails on
      missing files/checksum/manifest issues.
- [x] Child-test readiness reports whether the self-check files are present.
- [x] Package smoke and child-test readiness pass after regenerating the
      package.
- [x] `tasks/STATUS.md` points to the latest self-check evidence and keeps
      manual child-test/user acceptance as the remaining blocker.

## Open questions

None. This is release handoff/clean-machine QA support only; it does not change
gameplay, engine code, or acceptance criteria.

## Log

- 2026-06-13: Started after acceptance kit reached review. Scope: package
  self-check for testers; no gameplay, balance, engine, or web changes.
- 2026-06-13: Added package-generated `VERIFY_PACKAGE.ps1` and
  `VERIFY_PACKAGE.bat`. The PowerShell self-check verifies required files,
  manifest fields, and SHA-256 checksums from the extracted package folder.
- 2026-06-13: Updated `README.txt` generation to tell testers to run
  `VERIFY_PACKAGE.bat` before `RUN_67_WORLD.bat`.
- 2026-06-13: Updated package smoke to require the self-check files, verify
  they are in manifest/checksums/zip, and run `VERIFY_PACKAGE.ps1` from the
  package folder.
- 2026-06-13: Updated child-test readiness package status to report
  `self_check_script_exists` and `self_check_launcher_exists`.
- 2026-06-13: Static validation passed:
  `node --check tools/package_native_release.mjs` and
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/devapi/scenarios/child_test_readiness.py`.
- 2026-06-13: Regenerated package:
  `node tools/package_native_release.mjs` produced
  `build/release/67-world-pc/67-world/67-world.exe` (775168 bytes),
  `build/release/67-world-pc/67-world/assets/world67_art.ntpack`
  (20995020 bytes), and `build/release/67-world-pc/67-world-pc.zip`
  (21781018 bytes).
- 2026-06-13: First package smoke attempt caught a real self-check issue:
  `VERIFY_PACKAGE.ps1` wrongly required `release_manifest.json` inside
  `manifest.package.files`. Fixed the self-check to validate manifest via
  checksums while skipping manifest/checksum self-membership in package files.
- 2026-06-13: Package smoke passed:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9300 build/captures/scenarios/package_release_smoke_v6_selfcheck.png`.
  It verified package files, manifest, checksums, zip, branded exe metadata and
  resources, ran packaged `VERIFY_PACKAGE.ps1`, and captured a nonblank release
  launch screenshot. Self-check output: `PASS 67 World package self-check`,
  checked files `8`.
- 2026-06-13: Child-test readiness passed:
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9302 build/reports/child_test_readiness_v13_selfcheck.json build/captures/scenarios/child_test_readiness_v13_selfcheck`.
  Report result: `automated_review_passed=true`, package ok with self-check
  files, acceptance kit, manifest, checksums and zip,
  `ready_for_manual_child_test=true`, `release_ready=false`.
- 2026-06-13: Pixel health passed for
  `build/captures/scenarios/package_release_smoke_v6_selfcheck.png` and all
  five v13 readiness screenshots under
  `build/captures/scenarios/child_test_readiness_v13_selfcheck/`.
  Remaining blocker: manual child-test/user acceptance.
- 2026-06-13: Direct self-check run also passed:
  `powershell -NoProfile -ExecutionPolicy Bypass -File build/release/67-world-pc/67-world/VERIFY_PACKAGE.ps1`
  printed `PASS 67 World package self-check`, checked files `8`.
