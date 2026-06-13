---
id: T0055
title: Polish top HUD readability in native child-test screens
status: review
epic: ""
priority: P1
tags: [ui, visual, child-test, native, readability]
created: 2026-06-13
updated: 2026-06-13
---

## What

Polish the native top HUD so coins, collection progress, world title, and the
upgrade tile are readable in both desktop and portrait child-test screenshots.
Current v24 evidence passes automation, but the portrait top row is crowded and
the desktop/portrait upgrade label reads as compressed `NEED702`/`NEED19`
instead of a clear child-readable state.

## Done when

- [x] Portrait first-loop screenshot shows the top HUD counters inside their
      generated plaques without edge crowding or clipped labels.
- [x] Desktop upgrade screenshot shows the upgrade tile with readable separated
      title/value text.
- [x] Portrait upgrade/readiness screenshots keep the world title and upgrade
      tile readable without overlapping the board or each other.
- [x] Native child-test readiness and release audit pass automated gates with
      only real manual child-test/user acceptance missing.
- [x] Task/status files point to the validation evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after reviewing v24 reward-toast screenshots. The
  reward feedback now reads well, but the top HUD remains dense: portrait
  counters sit tight to the screen edges and upgrade text is cramped.
- 2026-06-13: Expanded the portrait top HUD row, gave the upgrade tile more
  text width, reduced the icon/star intrusion, and added display-only spacing
  for `BUY 25` / `NEED 702` while keeping DevAPI/state labels as `BUY25` /
  `NEED702`. An initial attempt changed the DevAPI label and failed readiness;
  the final pass preserves the contract.
- 2026-06-13: Validation evidence:
  `cmake --build --preset native-debug`,
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9380 build/reports/child_test_readiness_v25_top_hud.json build/captures/scenarios/child_test_readiness_v25_top_hud`,
  `cmake --build --preset native-release`,
  `node tools/package_native_release.mjs`,
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9378 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`,
  pixel-health checks for all five v25 screenshots, and
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v25_top_hud.json`.
  Audit result: `automated_gates_passed=true`, `release_ready=false`; the only
  blocker is real manual child-test/user acceptance.
