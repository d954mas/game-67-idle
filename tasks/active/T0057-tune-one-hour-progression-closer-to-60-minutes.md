---
id: T0057
title: Tune one-hour progression closer to 60 minutes
status: review
epic: ""
priority: P1
tags: [balance, progression, release, native, child-test]
created: 2026-06-13
updated: 2026-06-13
---

## What

Tighten the release one-hour progression proof so deterministic ordinary play
finishes closer to a real one-hour session instead of passing near the lower
edge of the old 50-60 minute window.

Scope is late-game pacing only. Preserve the current FTUE, first minutes,
field-first loop, generated art, package flow, and native PC validation path.

## Done when

- [x] Better Crate late-game cost tuning is updated consistently in runtime
      code, simulator, and balance design data.
- [x] The one-hour runtime scenario uses a tighter lower target that rejects
      the previous 53.92 minute completion.
- [x] Native one-hour runtime evidence reaches Cosmic 67, WORLD COMPLETE, and
      finishes inside the updated release target window.
- [x] Release audit points to the refreshed one-hour evidence and still passes
      automated gates except the real manual child-test/user acceptance blocker.
- [x] Task/status files record commands and evidence paths.

## Open questions

None. This is a conservative balance iteration toward the existing release
goal.

## Log

- 2026-06-13: Started after current one-hour runtime proof completed at
  53.92 minutes, which passed the old 50-60 minute gate but is too close to the
  lower bound for a claimed roughly one-hour child-test session.
- 2026-06-13: Tuned Better Crate costs for levels 11-20 and 21+ in
  `src/game_state_actions.c`, `tools/balance/simulate_67_world.py`, and
  `gamedesign/meme-evolution/data/balance.json`; tightened the one-hour target
  window to 55-60 minutes in the runtime scenario, native DevAPI bot, and
  release audit.
- 2026-06-13: Evidence passed: `py -3.12 tools/balance/simulate_67_world.py`
  reached Cosmic 67 at 57.19 minutes; `cmake --build --preset native-debug`;
  `py -3.12 tools/devapi/scenarios/one_hour_progression_runtime.py 9388 build/reports/one_hour_progression_runtime_v2_balance.json build/captures/scenarios/one_hour_progression_runtime_v2_balance.png`
  reached Cosmic 67 at 57.1917 minutes with `WORLD COMPLETE`, `30/30`
  collection, Better Crate level 21, and screenshot pixel health passed.
- 2026-06-13: Release evidence refreshed after the C balance change:
  `cmake --build --preset native-release`; `node tools/package_native_release.mjs`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9390 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`;
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9392 build/reports/child_test_readiness_v27_balance.json build/captures/scenarios/child_test_readiness_v27_balance`;
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v27_balance.json`.
  Release audit result: `automated_gates_passed=true`, `release_ready=false`;
  only blocker remains real manual child-test/user acceptance.
