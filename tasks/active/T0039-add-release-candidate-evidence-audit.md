---
id: T0039
title: Add release candidate evidence audit
status: review
epic: ""
priority: P1
tags: [release, audit, validation, evidence, native]
created: 2026-06-12
updated: 2026-06-13
---

## What

Add a release-candidate audit script that aggregates current automated evidence
for the native 67 World package: one-hour progression, FTUE/readiness,
packaging, save isolation, screenshot health, and the remaining manual
child-test/user-acceptance gate.

## Done when

- [x] Audit checks current package files, checksums, zip, manifest, and
      packaged child-test handoff files.
- [x] Audit checks one-hour progression evidence reaches Cosmic 67 inside the
      target window.
- [x] Audit checks child-test readiness evidence and screenshot health.
- [x] Audit checks autosave/fresh child-test isolation evidence.
- [x] Audit writes a machine-readable report with `automated_gates_passed`,
      `release_ready`, and explicit blockers.
- [x] Audit keeps `release_ready=false` until manual child-test/user acceptance
      evidence exists.
- [x] Script syntax, audit run, and taskboard validation pass.

## Open questions

None.

## Log

- 2026-06-13: Started to make release readiness auditable from current
  artifacts instead of relying on scattered task/status notes.
- 2026-06-13: Added `tools/release_candidate_audit.py`. It checks package
  files/checksums/zip/manifest, one-hour progression, child-test readiness,
  screenshot health, save isolation, and manual child-test acceptance.
- 2026-06-13: Corrected the manual-acceptance detector after the first run
  accidentally accepted the unfilled result template because of substring
  matching. The detector now requires exact `yes` lines, filled session fields,
  at least 45 played/session minutes, and exact `Overall result: pass`.
- 2026-06-13: Evidence passed: `py -3.12 -m py_compile tools/release_candidate_audit.py`;
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v1.json`;
  `node tools/taskboard/cli.mjs validate`. Report result:
  `automated_gates_passed=true`, `release_ready=false`, blocker
  `Manual child-test/user acceptance report is missing or incomplete.`
- 2026-06-13: Refreshed release audit inputs after optional evidence packaging:
  `tools/release_candidate_audit.py` now checks
  `build/reports/child_test_readiness_v20_optional_evidence_package.json` and
  the matching v20 desktop/portrait screenshots instead of the older v19
  readiness set. Evidence passed:
  `py -3.12 -m py_compile tools/release_candidate_audit.py`;
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v14_current_readiness.json`.
  Report result: `automated_gates_passed=true`, `release_ready=false`, blocker
  remains real manual child-test/user acceptance only.
