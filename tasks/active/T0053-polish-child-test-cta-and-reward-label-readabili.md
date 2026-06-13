---
id: T0053
title: Polish child-test CTA and reward label readability
status: review
epic: ""
priority: P1
tags: [ui, visual, child-test, native, readability]
created: 2026-06-12
updated: 2026-06-13
---

## What

Polish the native child-test screen so the main action and reward feedback are
clear in both 960x540 desktop and 390x844 portrait captures. Current v21
screenshots show the central FTUE prompt, `NEW 67!`, and the crate action label
competing for the same visual space in portrait; the crate/`BOX` CTA is also
not obvious enough after the first merge.

## Done when

- [x] Portrait first-loop screenshot shows readable FTUE prompt and `NEW 67!`
      without overlapping each other.
- [x] Desktop and portrait screenshots show a clear spawn/crate CTA near the
      crate/ring, not only a small label inside the ring.
- [x] Full/stuck board still exposes a clear `FREE SLOT` action.
- [x] Native child-test readiness and release audit pass automated gates with
      only real manual child-test/user acceptance missing.
- [x] Task/status files point to the validation evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after visually reviewing
  `child_test_readiness_v21_package_bound` screenshots. In portrait, the
  `KEEP MERGING` prompt and `NEW 67!` reward text overlap, and the crate/ring
  CTA is less obvious than the core first-minute action should be for a child
  tester.
- 2026-06-13: Repositioned the reusable slice9 spawn CTA in `src/main.c` to sit
  near the crate in native art mode, enlarged/shadowed `TAP BOX` and
  `FREE SLOT`, and moved portrait reward feedback below the tutorial plaque.
  Visual proof: `build/captures/scenarios/child_test_readiness_v22_cta_readability/child_test_portrait_first_loop.png`
  and `build/captures/scenarios/child_test_readiness_v22_cta_readability/child_test_portrait_stuck.png`.
- 2026-06-13: Evidence passed: `cmake --build --preset native-debug`;
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9364 build/reports/child_test_readiness_v22_cta_readability.json build/captures/scenarios/child_test_readiness_v22_cta_readability`;
  pixel health for all five v22 readiness screenshots; `cmake --build --preset native-release`;
  `node tools/package_native_release.mjs`; `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9362 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`;
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v23_cta_readability.json`.
  Audit result: `automated_gates_passed=true`, `release_ready=false`, blocker
  remains only real manual child-test/user acceptance.
