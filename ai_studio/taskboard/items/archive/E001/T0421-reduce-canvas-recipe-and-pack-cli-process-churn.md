---
id: T0421
title: Reduce Canvas recipe and pack CLI process churn
status: done
project: P001
epic: E001
priority: P1
tags: [canvas, recipe, pack, verification, performance, tests]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"recipe 55/55; pack 37/37; Canvas/Chat 843 pass + 2 expected skips; assets pass; full Windows 10 domains pass; independent review pass"}]}
---

## What

Reduce repeated one-shot Canvas CLI launches in the recipe and pack test
owners. Reuse the importable dispatcher added by T0420, keep fixture state
isolated, and retain real subprocess coverage for exit/stderr, environment,
actor, human stdout, locking, Python, and other external-tool boundaries.

## Done when

- [x] Measure focused recipe and pack baselines and identify exact removable
  CLI launches before editing.
- [x] Convert only success-path parity cases that do not exercise a process or
  external-tool contract; preserve all real boundary smokes.
- [x] Focused tests, complete Canvas/Chat, Assets domain, and full Windows pass.
- [x] Canvas/Chat improves by at least 1.5 s on the same host, or the added
  conversion is removed instead of retained as ceremony.

## Decisions

- Keep both focused conversions: together they reduce shared runner contention
  enough to clear the Canvas/Chat threshold while deleting net test code.
- Keep pack local `fail()` guards and the final pack-slice stdout/Python path as
  subprocesses. Direct Python coverage already uses the warm worker and is not
  replaced or mocked.

## Log

- 2026-07-14: created after T0420 reduced Canvas/Chat from 19.988 s to
  17.179 s by removing 35 repeated CLI launches from `cli.test.mjs`.
- 2026-07-14: baseline measurement started with recipe before pack; no
  implementation selected before timing and boundary classification.
- 2026-07-14: focused baselines passed: recipe 3.167 s and pack 3.885 s.
  Mapping found 28 recipe CLI launches and 22 pack CLI launches plus one
  unnecessary `node -e` file write.
- 2026-07-14: reused the T0420 dispatcher locally; domain success/errors now
  run in-process. Retained 5 recipe local-guard subprocesses, 3 pack
  local-guard subprocesses, the real pack-slice stdout/Python CLI, and all
  direct warm-worker Python coverage. Removed the `node -e` JSON write.
- 2026-07-14: focused recipe 55/55 passed in 2.992 s and pack 37/37 passed in
  2.238 s. Complete Canvas/Chat 843 pass + 2 expected skips took 13.648 s,
  down 3.531 s from the 17.179 s T0420 close measurement.
- 2026-07-14: Assets passed in 16.069 s, down 3.056 s from 19.125 s. Full
  Windows passed all 10 applicable domains in 46.239 s; design was explicitly
  not applicable. Full wall-time remained host/other-domain bound and was not
  claimed as improved. No WSL was used.
- 2026-07-14: independent review passed with no blockers: assertions retain
  domain parity, actor does not leak, fixture env is restored, process-owned
  contracts remain subprocesses, and the two files have 34 fewer net lines.
- 2026-07-14: Quality: QTECH_001=pass; evidence: recipe 55/55; pack 37/37; Canvas/Chat 843 pass + 2 expected skips; assets pass; full Windows 10 domains pass; independent review pass
