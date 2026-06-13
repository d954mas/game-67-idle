---
id: T0054
title: Polish reward toast readability in native child-test screens
status: review
epic: ""
priority: P1
tags: [ui, visual, child-test, native, readability]
created: 2026-06-13
updated: 2026-06-13
---

## What

Polish reward feedback in the native child-test screens so `NEW 67!`, `SLOT
CLEARED!`, and similar short feedback read as a clear game toast instead of
plain text floating over fence/field art. Current v22 desktop first-loop proof
has `NEW 67!` sitting on the bottom fence, which is acceptable for automation
but weaker than release-quality child-test readability.

## Done when

- [x] Desktop first-loop screenshot shows reward feedback on a readable
      reusable UI plaque, not directly on fence art.
- [x] Portrait first-loop screenshot keeps reward feedback separated from the
      FTUE plaque and crate CTA.
- [x] Stuck/full-board screenshots still show `FREE SLOT` clearly.
- [x] Native child-test readiness and release audit pass automated gates with
      only real manual child-test/user acceptance missing.
- [x] Task/status files point to the validation evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after reviewing v22 readiness screenshots. The CTA fix
  worked, but desktop reward feedback still reads as loose text over the bottom
  fence instead of a clear release-quality reward toast.
- 2026-06-13: Added a reusable reward toast plaque in the native art overlay
  path. An initial placement in the text phase tripped the renderer material
  assertion, so the slice9 background was moved before the sprite flush and the
  text phase now only places the label.
- 2026-06-13: Native proof passed with
  `build/reports/child_test_readiness_v24_reward_toast.json` and screenshots
  under
  `build/captures/scenarios/child_test_readiness_v24_reward_toast/`.
  Key proofs: `child_test_desktop_first_loop.png`,
  `child_test_portrait_first_loop.png`, and `child_test_portrait_stuck.png`.
- 2026-06-13: Rebuilt the release package, reran package smoke, reran
  readiness against the current package hashes, passed pixel-health checks for
  all five v24 screenshots, and wrote
  `build/reports/release_candidate_audit_v24_reward_toast.json`.
  Audit result: `automated_gates_passed=true`, `release_ready=false`; the only
  blocker is real manual child-test/user acceptance.
