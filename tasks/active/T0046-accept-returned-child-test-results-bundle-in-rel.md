---
id: T0046
title: Accept returned child-test results bundle in release audit
status: review
epic: ""
priority: P1
tags: [release, audit, child-test, packaging, validation]
created: 2026-06-12
updated: 2026-06-13
---

## What

Teach the release candidate audit to accept the packaged
`child_test_results_for_return.zip` produced by `EXPORT_CHILD_TEST_RESULTS.bat`
as manual child-test acceptance evidence, while still rejecting missing,
malformed, or incomplete returned bundles.

## Done when

- [x] Release audit checks `child_test_results_for_return.zip` in the package
      folder and validates its contents.
- [x] A returned zip only passes if it contains a filled passing
      `child_test_result_*.md` or valid synthetic-equivalent report plus package
      metadata/instructions.
- [x] Missing/invalid returned bundles keep `release_ready=false`.
- [x] A synthetic valid returned bundle flips only the manual gate to pass
      while automated gates remain enforced.
- [x] Temporary synthetic returned bundles are cleaned up after validation.

## Open questions

None.

## Log

- 2026-06-13: Started after adding packaged child-test export. The next release
  audit must accept the exact return artifact the tester is asked to send back.
- 2026-06-13: Updated `tools/release_candidate_audit.py` to accept
  `child_test_results_for_return.zip` from the package folder or package parent
  folder only when it contains a filled passing child-test report plus
  `release_manifest.json`, `CHECKSUMS.txt`, `CHILD_TEST_ACCEPTANCE.md`,
  `PARENT_OBSERVER_GUIDE.md`, and `RETURN_INSTRUCTIONS.txt`.
- 2026-06-13: Evidence passed:
  `py -3.12 -m py_compile tools/release_candidate_audit.py`;
  missing bundle audit `build/reports/release_candidate_audit_v10_return_bundle_missing.json`
  kept `release_ready=false`;
  synthetic valid exported bundle audit
  `build/tmp/release_candidate_audit_synthetic_return_bundle_should_pass.json`
  produced `release_ready=true`;
  invalid returned bundle audit
  `build/tmp/release_candidate_audit_invalid_return_bundle_should_fail.json`
  kept `release_ready=false`;
  cleanup removed `build/release/67-world-pc/67-world/child_test_results_for_return.zip`,
  `build/tmp/child_test_result_synthetic_return_bundle.md`, and
  `build/tmp/blank_child_test_result_for_bundle.md`;
  final clean audit
  `build/reports/release_candidate_audit_v12_return_bundle_clean.json`
  produced `automated_gates_passed=true`, `release_ready=false`.
