---
id: T0056
title: Polish portrait collection drawer framing
status: review
epic: ""
priority: P1
tags: [ui, visual, portrait, native, child-test]
created: 2026-06-13
updated: 2026-06-13
---

## What

Polish the portrait collection drawer framing so the generated drawer reads as
a complete UI component instead of a strip pressed against the bottom of the
screen. Current v25 portrait screenshots pass automation, but the lower drawer
border and card row feel visually clipped at the viewport edge.

## Done when

- [x] Portrait first-loop screenshot shows the collection drawer with visible
      lower frame/breathing room and readable `COLLECTION` label.
- [x] Portrait stuck/full-board screenshot keeps the same drawer framing while
      preserving `FREE SLOT` readability.
- [x] Desktop child-test screenshots do not lose collection drawer readability.
- [x] Native child-test readiness and release audit pass automated gates with
      only real manual child-test/user acceptance missing.
- [x] Task/status files point to the validation evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after reviewing v25 top-HUD screenshots. The top HUD is
  improved, but portrait collection drawer still sits too close to the bottom
  edge for release-quality child-test framing.
- 2026-06-13: Increased portrait drawer height and bottom margin, and moved the
  collection label slightly inside the generated drawer. Desktop layout path
  was left unchanged.
- 2026-06-13: Validation evidence:
  `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9386 build/reports/child_test_readiness_v26_collection_drawer.json build/captures/scenarios/child_test_readiness_v26_collection_drawer`,
  `cmake --build --preset native-release`,
  `node tools/package_native_release.mjs`,
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9384 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`,
  pixel-health checks for all five v26 screenshots, and
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v26_collection_drawer.json`.
  Audit result: `automated_gates_passed=true`, `release_ready=false`; the only
  blocker is real manual child-test/user acceptance.
