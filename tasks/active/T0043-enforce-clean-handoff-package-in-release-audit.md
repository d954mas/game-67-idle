---
id: T0043
title: Enforce clean handoff package in release audit
status: review
epic: ""
priority: P1
tags: [release, audit, packaging, child-test, qa]
created: 2026-06-13
updated: 2026-06-13
---

## What

Release-candidate audit should verify the latest clean package smoke evidence
and reject a staged package polluted by stale/generated child-test report files.
The package may contain no reports before manual child-test, or only filled
reports that pass the same acceptance criteria.

## Done when

- [x] Release-candidate audit expects the latest clean-smoke framebuffer proof.
- [x] Release-candidate audit passes when `child_test_results` is absent.
- [x] Release-candidate audit fails automated gates when a blank/generated
      child-test report is present in the staged package.
- [x] Release-candidate audit can still allow a filled passing manual report.
- [x] Compile/audit/taskboard/hygiene validation pass.

## Open questions

None.

## Log

- 2026-06-13: Started after seeing release audit still referenced
  `package_release_framebuffer_proof_v1.png` while the latest clean handoff
  smoke proof is `package_release_framebuffer_proof_v2_clean_smoke.png`.
- 2026-06-13: Updated release-candidate audit to require
  `package_release_framebuffer_proof_v2_clean_smoke.png`, reject staged package
  `child_test_results` files unless every report is a filled passing acceptance
  report, and reject any report entries inside the release zip.
- 2026-06-13: Validation passed: `py -3.12 -m py_compile tools/release_candidate_audit.py`;
  clean audit `build/reports/release_candidate_audit_v5_clean_handoff.json`
  with `automated_gates_passed=true`; intentional blank generated report audit
  `build/tmp/release_candidate_audit_blank_report_should_fail.json` with
  `automated_gates_passed=false`; intentional synthetic filled report audit
  `build/tmp/release_candidate_audit_synthetic_valid_report.json` with
  `release_ready=true`; cleanup; final clean audit
  `build/reports/release_candidate_audit_v6_handoff_cleanliness.json` with
  `automated_gates_passed=true`, `release_ready=false`, and only the real
  manual child-test blocker.
