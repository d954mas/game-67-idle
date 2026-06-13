---
id: T0042
title: Clean package smoke child-test report side effects
status: review
epic: ""
priority: P2
tags: [release, packaging, qa, automation, child-test]
created: 2026-06-13
updated: 2026-06-13
---

## What

Package smoke verifies the packaged child-test result recorder and validator by
creating temporary `child_test_result_*.md` files inside the staged package
folder. Those files should not remain after smoke, because the folder may be
handed to a tester and stale blank reports make the manual child-test flow
confusing.

## Done when

- [x] Package smoke deletes only the child-test report files it creates.
- [x] Package smoke leaves pre-existing reports untouched.
- [x] Package smoke fails if its own generated report files remain.
- [x] Package/release audit/taskboard validation pass.

## Open questions

None.

## Log

- 2026-06-13: Started after observing smoke-created blank reports under
  `build/release/67-world-pc/67-world/child_test_results`.
- 2026-06-13: Updated package smoke to track report files created by the
  `START_HERE.bat` report action and direct report-recorder action, delete only
  those files after validation, remove the empty smoke-created
  `child_test_results` folder, and fail if any generated report remains.
- 2026-06-13: Validation passed: `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py`;
  `node tools/package_native_release.mjs`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9346 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`;
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v4_clean_smoke.json`;
  strict pixel-health on `package_release_framebuffer_proof_v2_clean_smoke.png`;
  and direct check that `build/release/67-world-pc/67-world/child_test_results`
  does not exist after smoke.
