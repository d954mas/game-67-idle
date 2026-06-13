---
id: T0050
title: Require full checksum coverage in package self-check
status: review
epic: ""
priority: P1
tags: [release, packaging, self-check, validation, qa]
created: 2026-06-12
updated: 2026-06-12
---

## What

Make packaged `VERIFY_PACKAGE.ps1` prove full checksum coverage for every
required release file, not only that the checksum lines which exist are valid.
This keeps the tester-facing self-check aligned with the stricter smoke/audit
gates.

## Done when

- [x] `VERIFY_PACKAGE.ps1` fails if any required package file is missing from
      `CHECKSUMS.txt`.
- [x] `VERIFY_PACKAGE.ps1` fails if `CHECKSUMS.txt` contains an unexpected
      package file.
- [x] Package smoke requires the exact self-check count for the current package.
- [x] Rebuilt package and release audit pass automated gates with only manual
      child-test/user acceptance missing.
- [x] Task/status files point to the validation evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after release package review found that the packaged
  self-check validated existing checksum lines but did not explicitly require a
  checksum entry for every required file.
- 2026-06-13: Added strict checksum coverage to generated
  `VERIFY_PACKAGE.ps1`: missing, unexpected, and duplicate checksum entries
  fail; checked count must match every required package file except
  `CHECKSUMS.txt`.
- 2026-06-13: Rebuilt the native release package with
  `node tools/package_native_release.mjs`; zip size is 21806897 bytes.
- 2026-06-13: Package smoke passed with exact `Checked files: 19`, package
  `RETURN_INSTRUCTIONS.txt`, return zip inclusion, cleanup of generated
  reports/return zip, and framebuffer proof.
- 2026-06-13: Release audit
  `build/reports/release_candidate_audit_v20_strict_self_check.json` passed
  automated gates; `release_ready=false` only because real manual
  child-test/user acceptance is still missing.
