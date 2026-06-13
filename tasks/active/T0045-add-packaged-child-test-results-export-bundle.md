---
id: T0045
title: Add packaged child-test results export bundle
status: review
epic: ""
priority: P1
tags: [release, child-test, packaging, handoff, validation]
created: 2026-06-12
updated: 2026-06-13
---

## What

Add a packaged child-test export step so a parent/tester can produce one
returnable evidence zip only after the child-test report validates. This should
reduce the chance of returning an incomplete report or an untraceable file.

## Done when

- [x] Package includes `EXPORT_CHILD_TEST_RESULTS.ps1` and
      `EXPORT_CHILD_TEST_RESULTS.bat`.
- [x] `START_HERE.bat`, `README.txt`, `VERIFY_PACKAGE.ps1`,
      `release_manifest.json`, `CHECKSUMS.txt`, and zip entries include the
      export step.
- [x] Export fails for a blank/generated report and succeeds for a filled
      synthetic valid report in package smoke.
- [x] Export smoke leaves no generated report or export zip side effects.
- [x] Release candidate audit requires the export files and current smoke
      evidence.

## Open questions

None.

## Log

- 2026-06-13: Started as release handoff hardening while the only remaining
  release blocker is manual child-test/user acceptance.
- 2026-06-13: Added packaged `EXPORT_CHILD_TEST_RESULTS.ps1/.bat`, START_HERE
  option 6, README/self-check instructions, manifest/checksums/zip entries,
  smoke coverage, and release audit enforcement. Evidence passed:
  `node --check tools/package_native_release.mjs`;
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/release_candidate_audit.py`;
  `node tools/package_native_release.mjs`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9352 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`;
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v9_child_test_export.json`.
  Smoke proved blank report export fails, synthetic valid report export
  produces `child_test_results_for_return.zip`, and cleanup leaves no
  `child_test_results` folder or return zip in the staged package.
