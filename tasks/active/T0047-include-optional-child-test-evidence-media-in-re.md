---
id: T0047
title: Include optional child-test evidence media in return bundle
status: review
epic: ""
priority: P1
tags: [release, child-test, packaging, handoff, evidence]
created: 2026-06-12
updated: 2026-06-13
---

## What

Make the packaged child-test return zip include optional tester-provided
evidence files from `child_test_results/evidence/`, so screenshots, short
videos, audio notes, and extra notes can travel with the validated report.

## Done when

- [x] Package docs tell the adult where optional evidence files go.
- [x] `EXPORT_CHILD_TEST_RESULTS.ps1` copies
      `child_test_results/evidence/` into the return zip when present.
- [x] Package smoke proves a synthetic evidence file is included in the return
      bundle and cleaned up afterward.
- [x] Release audit reports evidence entries from returned bundles without
      requiring media for acceptance.

## Open questions

None.

## Log

- 2026-06-13: Started after noticing the handoff asked for screenshots/video
  notes but the export bundle only carried the report and package metadata.
- 2026-06-13: Added optional evidence instructions to the child-test result
  template, parent observer guide, acceptance kit, and packaged `README.txt`.
- 2026-06-13: Updated packaged `EXPORT_CHILD_TEST_RESULTS.ps1` to copy
  `child_test_results/evidence/` into `evidence/` inside
  `child_test_results_for_return.zip` when the folder is present.
- 2026-06-13: Updated package smoke to create
  `evidence/evidence_note_for_smoke.txt`, verify it is present in the export
  bundle, and clean up the synthetic evidence folder afterward.
- 2026-06-13: Updated release audit to report `evidence_entries` from returned
  bundles without requiring evidence media for acceptance.
- 2026-06-13: Evidence passed: `node --check tools/package_native_release.mjs`;
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/release_candidate_audit.py`;
  `node tools/package_native_release.mjs`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9354 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`.
  Smoke proved the synthetic return bundle included
  `evidence/evidence_note_for_smoke.txt` and left no
  `child_test_results_for_return.zip` or `child_test_results/` in the staged
  package.
- 2026-06-13: Cleaned placeholder `- ` lines in child-test markdown templates,
  rebuilt the package, reran the same package smoke, and refreshed
  `build/reports/package_release_smoke_v2_evidence.json`. Updated package zip
  size: 21804900 bytes.
- 2026-06-13: Synthetic returned-bundle audit passed:
  `py -3.12 tools/release_candidate_audit.py --output build/tmp/release_candidate_audit_synthetic_evidence_bundle_should_pass.json`.
  Result: `release_ready=true`; `accepted_return_bundles` named the packaged
  return zip; `evidence_entries` contained
  `evidence/evidence_note_for_audit.txt`.
- 2026-06-13: Temporary synthetic report, evidence folder, and return zip were
  removed. Clean audit passed:
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v13_optional_evidence_clean.json`.
  Result: `automated_gates_passed=true`, `release_ready=false`, blocker remains
  real manual child-test/user acceptance only.
