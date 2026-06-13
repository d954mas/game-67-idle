---
id: T0035
title: Add one-click package start menu
status: review
epic: ""
priority: P1
tags: [release, packaging, handoff, qa]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add a single double-click package entrypoint, `START_HERE.bat`, so a parent or
tester does not need to choose between many scripts on first contact. The
existing direct launchers remain for automation and advanced use.

## Done when

- [x] Package generation emits `START_HERE.bat`.
- [x] `START_HERE.bat` offers verify package, normal play, fresh child-test,
      create report, and quit choices.
- [x] README, manifest, checksums, zip, self-check, package smoke, and
      readiness include `START_HERE.bat`.
- [x] Package smoke validates `START_HERE.bat` contains the expected script
      paths/tokens.
- [x] Package smoke executes safe `START_HERE.bat` menu choices for package
      verification and report creation.
- [x] Package smoke executes `START_HERE.bat` normal-play and fresh-child-test
      choices, verifies the packaged native exe launches, and stops only the
      launched packaged process.
- [x] Package smoke and child-test readiness pass after regenerating the
      package.
- [x] `tasks/STATUS.md` points to the latest start-menu evidence.

## Open questions

None. This is package handoff polish only; it does not change gameplay,
balance, engine, or web behavior.

## Log

- 2026-06-13: Started after parent observer guide reached review. Scope:
  single-entry package start menu for native PC release handoff; no gameplay,
  balance, engine, or web changes.
- 2026-06-13: Updated `tools/package_native_release.mjs` to emit
  `START_HERE.bat`, a guided batch menu with verify package, normal play,
  fresh child-test, create report, and quit choices. Existing direct launchers
  remain packaged.
- 2026-06-13: Updated README, release manifest, checksums, zip contents,
  `VERIFY_PACKAGE.ps1`, package smoke, and child-test readiness to include
  `START_HERE.bat`.
- 2026-06-13: Evidence passed: `node --check tools/package_native_release.mjs`;
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/devapi/scenarios/child_test_readiness.py`;
  `node tools/taskboard/cli.mjs validate`.
- 2026-06-13: `node tools/package_native_release.mjs` produced
  `build/release/67-world-pc/67-world/67-world.exe` (775168 bytes),
  `build/release/67-world-pc/67-world/assets/world67_art.ntpack` (20995020
  bytes), and `build/release/67-world-pc/67-world-pc.zip` (21792072 bytes).
- 2026-06-13: `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9318 build/captures/scenarios/package_release_smoke_v12_start_here.png`
  passed: `START_HERE.bat` is present in package files, manifest, checksums,
  and zip; smoke validates it contains `VERIFY_PACKAGE.bat`,
  `RUN_67_WORLD.bat`, `START_CHILD_TEST_FRESH.bat`,
  `CREATE_CHILD_TEST_REPORT.bat`, `PARENT_OBSERVER_GUIDE.md`,
  `CHILD_TEST_ACCEPTANCE.md`, and `choice /C 1234Q`; self-check passed with 14
  checked files; packaged release launched and screenshot captured.
- 2026-06-13: `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9320 build/reports/child_test_readiness_v18_start_here.json build/captures/scenarios/child_test_readiness_v18_start_here`
  passed. Report result: `automated_review_passed=true`, package ok with
  `START_HERE.bat`, `ready_for_manual_child_test=true`,
  `release_ready=false` only because manual child-test/user acceptance is still
  required.
- 2026-06-13: Pixel health passed for
  `build/captures/scenarios/package_release_smoke_v12_start_here.png` and all
  five screenshots under
  `build/captures/scenarios/child_test_readiness_v18_start_here/`.
- 2026-06-13: Strengthened package smoke after review: it now runs
  `START_HERE.bat` through `cmd.exe` with choice `1` and verifies the packaged
  self-check output, then runs choice `4` and verifies a child-test report file
  is created. Fixed the report validator to parse the created report path from
  stdout so same-second report names do not create false failures.
- 2026-06-13: Evidence passed after start-menu action validation:
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py`;
  `node tools/taskboard/cli.mjs validate`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9322 build/captures/scenarios/package_release_smoke_v13_start_here_actions.png`;
  `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/package_release_smoke_v13_start_here_actions.png`.
- 2026-06-13: Strengthened package smoke again: it now runs
  `START_HERE.bat` choice `2` for normal play and choice `3` for fresh
  child-test, verifies the packaged native exe appears, then stops only the
  exact packaged `67-world.exe` process it launched. CIM process command-line
  access was denied in the sandbox, so the validator uses `Get-Process`
  filtered by exact executable path; the fresh-launcher flags remain covered by
  the static launcher validation.
- 2026-06-13: Evidence passed after start-menu launch validation:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9324 build/captures/scenarios/package_release_smoke_v14_start_here_launches.png`;
  `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/package_release_smoke_v14_start_here_launches.png`.
