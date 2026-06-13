---
id: T0037
title: Add native one-hour progression runtime proof
status: review
epic: ""
priority: P1
tags: [release, validation, balance, devapi, native]
created: 2026-06-12
updated: 2026-06-13
---

## What

Add a native runtime proof for the one-hour progression target. The existing
Python balance simulator proves the economy model, but release readiness also
needs a DevAPI scenario that drives the actual native game action endpoints
through the progression to Cosmic 67 and captures a report/screenshot.

## Done when

- [x] Native DevAPI exposes bulk passive-tick and one-hour progression bot
      actions for automation without changing non-DevAPI release play.
- [x] A scenario runs native runtime spawn/merge/recycle/upgrade/passive logic
      to Cosmic 67 without direct state forcing after reset.
- [x] The report proves the runtime reaches Cosmic 67 inside the 50-60 minute
      target window and records actions/unlock timings.
- [x] A final native screenshot is captured and passes pixel health.
- [x] `tasks/STATUS.md` points to the latest runtime one-hour evidence.
- [x] Taskboard and relevant compile/build checks pass.

## Open questions

None. This is native runtime validation; it does not change the intended player
balance, generated art, package content, or web behavior.

## Log

- 2026-06-13: Started after continuation audit. Scope: strengthen release
  evidence for the one-hour gameplay requirement by proving the existing
  balance through native DevAPI runtime actions, not only the separate balance
  simulator.
- 2026-06-13: Added DevAPI-only `game.action.tick_passive` and
  `game.action.run_one_hour_progression` in `src/main.c`, plus
  `tools/devapi/scenarios/one_hour_progression_runtime.py`. First Python-side
  loop attempt timed out because thousands of individual DevAPI round-trips
  were too slow; moved the long-run bot into native C so it uses the runtime
  `game_67_*` action functions directly.
- 2026-06-13: Evidence passed: `py -3.12 -m py_compile tools/devapi/scenarios/one_hour_progression_runtime.py`;
  `cmake --build --preset native-debug`;
  `cmake --build --preset native-release`;
  `py -3.12 tools/devapi/scenarios/one_hour_progression_runtime.py 9330 build/reports/one_hour_progression_runtime_v1.json build/captures/scenarios/one_hour_progression_runtime_v1.png`;
  `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/one_hour_progression_runtime_v1.png`.
  Result: Cosmic 67 reached at `53.916666666666664` minutes, Better Crate
  level `21`, actions `spawn=3232`, `merge=3220`, `buy_better_crate=21`,
  `recycle=8`, final goal `WORLD COMPLETE`.
- 2026-06-13: Regenerated current native PC release package after the
  `src/main.c` change and validated handoff: `node tools/package_native_release.mjs`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9332 build/captures/scenarios/package_release_smoke_v15_runtime_proof.png`;
  `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/package_release_smoke_v15_runtime_proof.png`.
- 2026-06-13: Re-ran child-test readiness against the current build/package:
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9334 build/reports/child_test_readiness_v19_runtime_proof.json build/captures/scenarios/child_test_readiness_v19_runtime_proof`.
  All five v19 readiness screenshots passed `pixel_health`. Report result:
  `automated_review_passed=true`, package ok, `ready_for_manual_child_test=true`,
  `release_ready=false` only because manual child-test/user acceptance is still
  required.
