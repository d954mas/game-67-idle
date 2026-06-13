---
id: T0051
title: Require meaningful manual child-test report details
status: review
epic: ""
priority: P1
tags: [release, child-test, acceptance, validation, qa]
created: 2026-06-12
updated: 2026-06-12
---

## What

Harden the manual child-test acceptance path so a release-ready report cannot
pass with only toggled yes/pass values. The packaged validator and release
audit should require basic session metadata and short observer notes for the
first minute, five-minute loop, one-hour progression, audio, and final summary.
This keeps the final release gate tied to real child-test observations.

## Done when

- [x] Packaged `VALIDATE_CHILD_TEST_REPORT.ps1` rejects reports with missing
      required session metadata.
- [x] Packaged validator rejects reports with missing required observer notes.
- [x] `tools/release_candidate_audit.py` uses the same stricter acceptance
      rules for staged reports and returned zip bundles.
- [x] Package smoke proves blank reports are rejected and synthetic meaningful
      reports still export/validate.
- [x] Rebuilt package and release audit pass automated gates with only real
      manual child-test/user acceptance missing.
- [x] Task/status files point to the validation evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after v20 release audit showed the only remaining hard
  blocker is manual child-test/user acceptance. Tightening the report quality
  gate reduces the risk of a formal but uninformative release acceptance.
- 2026-06-13: Updated the packaged report validator to require filled session
  metadata, four meaningful `Notes:` entries, and a meaningful
  `Observer summary`; fixed the parser to handle CRLF without reading the next
  line as a field value.
- 2026-06-13: Updated `tools/release_candidate_audit.py` to apply the same
  stricter manual acceptance rules to staged reports and returned zip bundles.
- 2026-06-13: Updated `CHILD_TEST_RESULT_TEMPLATE.md` and
  `CHILD_TEST_ACCEPTANCE.md` so testers know notes and observer summary are
  required for release acceptance.
- 2026-06-13: Rebuilt package with `node tools/package_native_release.mjs`;
  zip size is 21808722 bytes.
- 2026-06-13: Package smoke passed. It now proves a blank report is rejected,
  an almost-valid report with missing notes/summary is rejected with
  `need 4 meaningful entries for Notes` and
  `missing meaningful line: Observer summary`, and a meaningful synthetic
  report validates/exports cleanly.
- 2026-06-13: Release audit
  `build/reports/release_candidate_audit_v21_meaningful_child_test_report.json`
  passed automated gates; `release_ready=false` only because real manual
  child-test/user acceptance is still missing.
