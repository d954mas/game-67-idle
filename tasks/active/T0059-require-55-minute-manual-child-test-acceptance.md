---
id: T0059
title: Require 55-minute manual child-test acceptance
status: review
epic: ""
priority: P1
tags: [release, child-test, acceptance, validation, packaging]
created: 2026-06-13
updated: 2026-06-13
---

## What

Make manual child-test acceptance match the tuned one-hour release target. A
report should not pass release acceptance with only a short 45-minute session
when the automated balance target is now 55-60 minutes.

## Done when

- [x] Packaged child-test report validator requires at least 55 minutes for
      session length and minutes played.
- [x] Release audit applies the same 55-minute manual acceptance threshold to
      staged reports and returned bundles.
- [x] Package smoke proves a 45-minute otherwise-valid report is rejected.
- [x] Durable child-test docs tell testers that release acceptance requires
      55-60 minutes unless the report is marked fail/needs tuning.
- [x] Package smoke, release audit, task validation, and hygiene checks pass.

## Open questions

None.

## Log

- 2026-06-13: Started after finding the packaged validator and release audit
  still accepted `Session length in minutes` and `Minutes played` values at
  45+, while the current one-hour gameplay target is 55-60 minutes.
- 2026-06-13: Raised packaged report validation to 55 minutes in
  `tools/package_native_release.mjs`, mirrored the threshold in
  `tools/release_candidate_audit.py`, added a synthetic 45-minute negative
  report to `tools/devapi/scenarios/package_release_smoke.py`, and documented
  the 55-minute release acceptance rule in the child-test template,
  acceptance kit, GDD, and roadmap.
- 2026-06-13: Evidence passed: `node --check tools/package_native_release.mjs`;
  `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/release_candidate_audit.py`;
  `node tools/package_native_release.mjs`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9394 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`.
  Package smoke wrote `build/reports/package_release_smoke_v2_evidence.json`
  and proved the 45-minute otherwise-valid report is rejected with
  `Session length in minutes must be at least 55 minutes` and
  `Minutes played must be at least 55 minutes`.
- 2026-06-13: Refreshed child-test readiness evidence with current package
  hashes: `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9396 build/reports/child_test_readiness_v29_manual_55.json build/captures/scenarios/child_test_readiness_v29_manual_55`.
- 2026-06-13: Release audit now points to the v29 readiness evidence and
  passed automated gates: `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v29_manual_55.json`.
  Result: `automated_gates_passed=True`, `release_ready=False`, with only the
  real manual child-test/user acceptance blocker remaining.
- 2026-06-13: Direct audit threshold check passed: a synthetic 45-minute report
  returned `accepted: False` from
  `release_candidate_audit.filled_manual_acceptance_report(...)` and was
  deleted afterward.
- 2026-06-13: Final hygiene passed: `node tools/taskboard/cli.mjs validate`;
  `git diff --check -- tools/package_native_release.mjs tools/devapi/scenarios/package_release_smoke.py tools/release_candidate_audit.py gamedesign/meme-evolution/child_test_result_template.md gamedesign/meme-evolution/child_test_acceptance.md gamedesign/meme-evolution/gdd.md gamedesign/meme-evolution/release_roadmap.md tasks/active/T0059-require-55-minute-manual-child-test-acceptance.md tasks/STATUS.md`;
  trailing-whitespace scan returned no matches for the same files; no
  `67-world` or `game_seed` processes were left running.
