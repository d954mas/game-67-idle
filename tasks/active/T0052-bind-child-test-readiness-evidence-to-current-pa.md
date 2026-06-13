---
id: T0052
title: Bind child-test readiness evidence to current package
status: review
epic: ""
priority: P1
tags: [release, child-test, readiness, audit, validation]
created: 2026-06-12
updated: 2026-06-12
---

## What

Bind `child_test_readiness` evidence to the exact native package currently
being audited. The readiness report already proves FTUE, desktop/portrait
screenshots, stuck recovery, and audio cues, but the release audit should also
prove that report was generated against the same `67-world.exe`, art pack,
manifest, checksums, and zip that package smoke/audit are checking.

## Done when

- [x] `tools/devapi/scenarios/child_test_readiness.py` records hashes for the
      package exe, art pack, manifest, checksums, and zip.
- [x] `tools/release_candidate_audit.py` rejects stale child-test readiness
      reports whose package hashes do not match the current package.
- [x] A fresh child-test readiness report is generated for the current package.
- [x] Release audit passes automated gates with only real manual child-test/user
      acceptance missing.
- [x] Task/status files point to the validation evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after noticing
  `child_test_readiness_v20_optional_evidence_package.json` still reported
  older package bytes while the v21 release package and smoke evidence had
  changed. The audit should not allow stale child-test readiness evidence to
  satisfy the current release gate.
- 2026-06-13: Added package hashes to
  `tools/devapi/scenarios/child_test_readiness.py` for `67-world.exe`,
  `assets/world67_art.ntpack`, `release_manifest.json`, `CHECKSUMS.txt`, and
  `67-world-pc.zip`.
- 2026-06-13: Updated `tools/release_candidate_audit.py` to require the
  readiness report package hashes to match the current package before the
  child-test readiness gate can pass.
- 2026-06-13: Generated fresh readiness evidence:
  `build/reports/child_test_readiness_v21_package_bound.json` and screenshots
  under `build/captures/scenarios/child_test_readiness_v21_package_bound/`.
  The scenario passed desktop FTUE, upgrade, stuck recovery, audio cue, and
  portrait checks.
- 2026-06-13: Release audit
  `build/reports/release_candidate_audit_v22_bound_readiness_package.json`
  passed automated gates; `release_ready=false` only because real manual
  child-test/user acceptance is still missing.
