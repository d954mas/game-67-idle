---
id: T0058
title: Harden one-hour release audit evidence
status: review
epic: ""
priority: P1
tags: [release, audit, balance, validation, native]
created: 2026-06-13
updated: 2026-06-13
---

## What

Strengthen the release audit so one-hour progression evidence proves the new
55-60 minute release window and ordinary native loop actions, not only a final
state that happens to say Cosmic 67.

## Done when

- [x] Release audit rejects one-hour reports that do not declare the 55-60
      target window.
- [x] Release audit checks the expected v2 screenshot path from the one-hour
      report.
- [x] Release audit checks action-count evidence for spawn, merge, passive
      ticks, Better Crate buying, and stuck-board recycle.
- [x] Release audit still passes automated gates on current evidence and keeps
      manual child-test/user acceptance as the only blocker.
- [x] Task/status files record the new audit evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after `release_candidate_audit_v27_balance.json` proved
  the new balance but `audit_one_hour()` still did not assert target-window,
  screenshot-path, or ordinary-action-count details.
- 2026-06-13: Hardened `tools/release_candidate_audit.py` one-hour gate to
  require method string, declared target window `[55.0, 60.0]`, expected v2
  screenshot path, action counts (`spawn`, `merge`, `buy_faster_spawn`,
  `buy_better_crate`, `recycle`, `tick_passive`), max board use, and Cosmic
  unlock timing close to final minutes.
- 2026-06-13: Evidence passed: `py -3.12 -m py_compile tools/release_candidate_audit.py`;
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v28_one_hour_hardened.json`
  produced `automated_gates_passed=true`, `release_ready=false`, with only
  manual child-test/user acceptance blocking release.
- 2026-06-13: Negative evidence passed: temporarily changing
  `build/reports/one_hour_progression_runtime_v2_balance.json`
  `target_window_minutes` to old `[50.0, 60.0]` made
  `py -3.12 tools/release_candidate_audit.py --output build/tmp/release_candidate_audit_tampered_one_hour_window_should_fail.json`
  produce `automated_gates_passed=false`; the one-hour report was restored to
  `[55.0, 60.0]` and the positive v28 audit was rerun.
