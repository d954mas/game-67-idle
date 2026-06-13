---
id: T0044
title: Bind release audit to current package smoke evidence
status: review
epic: ""
priority: P1
tags: [release, audit, packaging, validation, automation]
created: 2026-06-13
updated: 2026-06-13
---

## What

Release-candidate audit should not trust a standalone screenshot path without
knowing which package produced it. Package smoke needs a durable JSON evidence
report with package hashes and visual proof details, and release audit must
verify that report against the current staged package.

## Done when

- [x] Package smoke writes a JSON report with current package hashes, visual
      proof path/hash/health, cleanup status, and overall pass/fail.
- [x] Release-candidate audit requires that package smoke report.
- [x] Release-candidate audit fails if smoke evidence hashes do not match the
      current staged package.
- [x] Release-candidate audit still reports `automated_gates_passed=true` and
      `release_ready=false` for the clean package without manual child-test.
- [x] Compile/smoke/audit/taskboard/hygiene validation pass.

## Open questions

None.

## Log

- 2026-06-13: Started after noticing release audit checked package contents and
  screenshot health separately, without proving the visual proof came from the
  current package hashes.
- 2026-06-13: Added `build/reports/package_release_smoke_v2_evidence.json` from
  package smoke, containing package hashes, checksum/manifest/script checks,
  report cleanup status, framebuffer capture path/hash/health, launch probe,
  and overall pass/fail.
- 2026-06-13: Release-candidate audit now requires the smoke evidence report,
  matches its package hashes against the current staged package and zip, checks
  the framebuffer proof hash, and reads JSON with `utf-8-sig` so Windows BOMs
  produce normal audit results instead of traceback failures.
- 2026-06-13: Validation passed: `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/release_candidate_audit.py`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9348 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`;
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v7_bound_smoke_evidence.json`;
  intentional tampered smoke hash audit
  `build/tmp/release_candidate_audit_tampered_smoke_should_fail.json` with
  `automated_gates_passed=false`; restored smoke evidence; final audit
  `build/reports/release_candidate_audit_v8_bound_smoke_final.json` with
  `automated_gates_passed=true`, `release_ready=false`, and only the real
  manual child-test blocker.
