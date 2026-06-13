---
id: T0040
title: Add packaged child-test report validator
status: review
epic: ""
priority: P1
tags: [release, child-test, validation, packaging, qa]
created: 2026-06-12
updated: 2026-06-13
---

## What

Ship a child-test report validator in the native PC package so a completed
manual report can be checked locally before the release-candidate audit accepts
it.

## Done when

- [x] Package includes `VALIDATE_CHILD_TEST_REPORT.ps1` and
      `VALIDATE_CHILD_TEST_REPORT.bat`.
- [x] Validator rejects a blank/generated template report.
- [x] Validator requires setup checks, first-minute/five-minute/one-hour/audio
      pass fields, filled session fields, at least 45 minutes played, and exact
      `Overall result: pass`.
- [x] Package manifest, checksums, zip, self-check, and package smoke include
      the validator files.
- [x] Release-candidate audit still reports `automated_gates_passed=true` and
      `release_ready=false` until a validated manual report exists.
- [x] Syntax/build/package/smoke/audit/taskboard validation pass.

## Open questions

None.

## Log

- 2026-06-13: Started after release-candidate audit showed the manual
  acceptance gate needs a packaged validator, not only a Python-side audit.
- 2026-06-13: Added packaged validator scripts, wired them into manifest,
  checksums, zip, self-check, `START_HERE.bat`, package smoke, and
  release-candidate audit. Fixed the generated PowerShell script after the
  first smoke run exposed that `param(...)` must be the first executable line.
- 2026-06-13: Validation passed: `node --check tools/package_native_release.mjs`;
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/release_candidate_audit.py`;
  `cmake --build --preset native-release`; `node tools/package_native_release.mjs`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9340 build/captures/scenarios/package_release_smoke_v17_report_validator_retry.png`;
  packaged `VERIFY_PACKAGE.ps1`; and
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v2_report_validator.json`.
- 2026-06-13: Final board/hygiene checks passed:
  `node tools/taskboard/cli.mjs validate`, `git diff --check` for touched
  files, and trailing-whitespace scan.
