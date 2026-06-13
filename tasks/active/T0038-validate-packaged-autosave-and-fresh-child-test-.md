---
id: T0038
title: Validate packaged autosave and fresh child-test isolation
status: review
epic: ""
priority: P1
tags: [release, save, autosave, child-test, native, validation]
created: 2026-06-12
updated: 2026-06-13
---

## What

Add a focused native validation gate for release-save behavior: normal play
must autosave and reload progress after restart, while fresh child-test runs
must start clean and must not overwrite the normal autosave.

## Done when

- [x] Scenario uses an isolated runtime cwd so validation cannot read or
      overwrite a developer/player autosave.
- [x] Scenario creates progress through real 67 World gameplay actions
      (spawn/merge), not only raw state setters.
- [x] Scenario proves a normal autosave reloads after restart.
- [x] Scenario proves a fresh no-autosave child-test run starts clean.
- [x] Scenario proves fresh no-autosave child-test mutations do not overwrite
      the prior normal autosave.
- [x] Scenario captures native screenshot evidence and writes a machine-readable
      report.
- [x] Native build and scenario/pixel-health/taskboard validation pass.

## Open questions

None.

## Log

- 2026-06-13: Started after one-hour runtime proof; release risk is save
  persistence and child-test isolation.
- 2026-06-13: Added `tools/devapi/scenarios/package_save_isolation.py`. The
  scenario uses isolated cwd `build/tmp/save_isolation_runtime`, copies the
  current `world67_art.ntpack`, creates Berry 67 progress through gameplay
  spawn/merge actions, proves normal autosave reload, proves fresh no-autosave
  child-test starts clean, mutates to Banana 67 only in memory, and proves the
  old normal autosave hash still reloads afterward.
- 2026-06-13: Evidence passed: `py -3.12 -m py_compile tools/devapi/scenarios/package_save_isolation.py`;
  `cmake --build --preset native-debug`;
  `py -3.12 tools/devapi/scenarios/package_save_isolation.py 9336 build/reports/package_save_isolation_v1.json build/captures/scenarios/package_save_isolation_v1.png`;
  `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/package_save_isolation_v1.png`;
  `node tools/taskboard/cli.mjs validate`.
