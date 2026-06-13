---
id: T0034
title: Add packaged parent observer guide
status: review
epic: ""
priority: P1
tags: [release, child-test, qa, packaging, handoff]
created: 2026-06-12
updated: 2026-06-12
---

## What

Add a packaged parent/observer guide so a non-developer adult understands the
child-test build, privacy/safety context, how to start the session, what to
observe, and what to return after the session.

## Done when

- [x] A durable parent observer guide exists outside build output.
- [x] Package generation copies it as `PARENT_OBSERVER_GUIDE.md`.
- [x] Package README and self-check point adults to the guide.
- [x] Manifest, checksums, zip, package self-check, package smoke, and
      readiness include the guide.
- [x] Package smoke and child-test readiness pass after regenerating the
      package.
- [x] `tasks/STATUS.md` points to the latest parent-guide evidence.

## Open questions

None. This is package/release handoff polish only; it does not change gameplay
or replace manual child-test acceptance.

## Log

- 2026-06-13: Started after result-recorder package reached review. Scope:
  parent/observer handoff in the native PC release package; no gameplay,
  balance, engine, or web changes.
- 2026-06-13: Added durable source
  `gamedesign/meme-evolution/parent_observer_guide.md` with child-test context,
  safety/privacy notes, adult observation points, stop conditions, and return
  instructions.
- 2026-06-13: Updated `tools/package_native_release.mjs` to package
  `PARENT_OBSERVER_GUIDE.md`, link it from `README.txt`, include it in
  manifest/checksums/zip, and make `VERIFY_PACKAGE.ps1` point adults to it
  before testing.
- 2026-06-13: Updated package smoke and child-test readiness to require/report
  `PARENT_OBSERVER_GUIDE.md`.
- 2026-06-13: Evidence passed: `node --check tools/package_native_release.mjs`;
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/devapi/scenarios/child_test_readiness.py`;
  `node tools/taskboard/cli.mjs validate`.
- 2026-06-13: `node tools/package_native_release.mjs` produced
  `build/release/67-world-pc/67-world/67-world.exe` (775168 bytes),
  `build/release/67-world-pc/67-world/assets/world67_art.ntpack` (20995020
  bytes), and `build/release/67-world-pc/67-world-pc.zip` (21790690 bytes).
- 2026-06-13: `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9314 build/captures/scenarios/package_release_smoke_v11_parent_guide.png`
  passed: `PARENT_OBSERVER_GUIDE.md` is present in package files, manifest,
  checksums, and zip; self-check passed with 13 checked files and prints
  `Before testing, read PARENT_OBSERVER_GUIDE.md.`; result recorder, branding,
  resources, package launch, and screenshot capture passed.
- 2026-06-13: `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9316 build/reports/child_test_readiness_v17_parent_guide.json build/captures/scenarios/child_test_readiness_v17_parent_guide`
  passed. Report result: `automated_review_passed=true`, package ok with
  parent guide, `ready_for_manual_child_test=true`, `release_ready=false` only
  because manual child-test/user acceptance is still required.
- 2026-06-13: Pixel health passed for
  `build/captures/scenarios/package_release_smoke_v11_parent_guide.png` and all
  five screenshots under
  `build/captures/scenarios/child_test_readiness_v17_parent_guide/`.
