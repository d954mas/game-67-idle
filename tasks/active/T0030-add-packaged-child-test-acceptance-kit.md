---
id: T0030
title: Add packaged child-test acceptance kit
status: review
epic: ""
priority: P1
tags: [release, child-test, qa, packaging, validation]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a structured child-test acceptance kit to the native PC release package so
manual testing can produce actionable pass/fail evidence instead of loose
feedback. The kit should ship with the package, be recorded in manifest and
checksums, and be validated by package smoke/readiness checks.

## Done when

- [x] A durable child-test acceptance source doc exists outside build output.
- [x] `tools/package_native_release.mjs` copies the acceptance kit into the
      packaged `67-world` folder and includes it in manifest, checksums, and
      zip.
- [x] Package smoke fails if the acceptance kit is missing from package,
      checksums, manifest, or zip.
- [x] Child-test readiness reports whether the acceptance kit is present.
- [x] Package smoke and child-test readiness pass after regenerating the
      package.
- [x] `tasks/STATUS.md` points to the latest acceptance-kit evidence and keeps
      manual child-test/user acceptance as the remaining blocker.

## Open questions

None. This pass does not claim manual acceptance; it makes that acceptance
repeatable and auditable.

## Log

- 2026-06-13: Started after branded package reached review. Scope: release
  handoff/QA only; no gameplay, balance, engine, or web changes.
- 2026-06-13: Added durable source doc
  `gamedesign/meme-evolution/child_test_acceptance.md` covering first-minute,
  five-minute, one-hour, audio, stop-condition, and final acceptance checks.
- 2026-06-13: Updated package generation to copy
  `CHILD_TEST_ACCEPTANCE.md` into the package and include it in
  `release_manifest.json`, `CHECKSUMS.txt`, and `67-world-pc.zip`.
- 2026-06-13: Static validation passed:
  `node --check tools/package_native_release.mjs` and
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/devapi/scenarios/child_test_readiness.py`.
- 2026-06-13: Regenerated package:
  `node tools/package_native_release.mjs` produced
  `build/release/67-world-pc/67-world/67-world.exe` (775168 bytes),
  `build/release/67-world-pc/67-world/assets/world67_art.ntpack`
  (20995020 bytes), and `build/release/67-world-pc/67-world-pc.zip`
  (21777049 bytes).
- 2026-06-13: Package smoke passed:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9296 build/captures/scenarios/package_release_smoke_v5_acceptance.png`.
  It verified `CHILD_TEST_ACCEPTANCE.md` in package files, checksums, manifest,
  and zip, plus existing branded exe metadata/resources and launch screenshot.
- 2026-06-13: Pixel health passed for
  `build/captures/scenarios/package_release_smoke_v5_acceptance.png`.
- 2026-06-13: Child-test readiness passed:
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9298 build/reports/child_test_readiness_v12_acceptance.json build/captures/scenarios/child_test_readiness_v12_acceptance`.
  Report result: `automated_review_passed=true`, package ok with acceptance kit,
  manifest, checksums and zip, `ready_for_manual_child_test=true`,
  `release_ready=false`.
- 2026-06-13: Pixel health passed for all five v12 readiness screenshots under
  `build/captures/scenarios/child_test_readiness_v12_acceptance/`.
  Remaining blocker: manual child-test/user acceptance.
