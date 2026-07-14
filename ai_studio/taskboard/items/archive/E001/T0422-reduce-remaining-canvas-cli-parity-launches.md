---
id: T0422
title: Reduce remaining Canvas CLI parity launches
status: done
project: P001
epic: E001
priority: P1
tags: [canvas, cli, verification, performance, tests]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"focused CLI 20/20; Canvas/Chat 843 pass + 2 expected skips; assets pass; full Windows 10 domains pass; independent review pass"}]}
---

## What

Reduce the remaining success-only subprocess launches in
`canvas/tests/cli.test.mjs`. Keep real subprocess coverage for actor,
environment/private stores, exit/stderr, human stdout, and process locks.

## Done when

- [x] Re-measure and rank the remaining focused test costs.
- [x] Convert only a bounded next group of success-only parity scenarios using
  the existing T0420 dispatcher helper; add no framework or production API.
- [x] Focused, complete Canvas/Chat, Assets, and full Windows pass.
- [x] Focused CLI improves by at least 2 s or the conversion is removed.

## Decisions

- Convert archive metadata, valid clip toggles, node duplicate/delete/paste,
  and group-fit geometry. These are dispatcher/result parity only.
- Keep owner/private-store routing, agent-attributed history, human stdout,
  invalid flags, exit/stderr, and process-owned behavior as subprocesses.

## Log

- 2026-07-14: created after T0421 reduced Canvas/Chat to 13.648 s; the
  remaining critical path is `cli.test.mjs` with about 109 successful child
  CLI launches still present after T0420.
- 2026-07-14: focused CLI re-measurement started; boundary tests excluded
  before conversion. Baseline passed 20/20 in 11.329 s.
- 2026-07-14: converted 35 success-only launches in four tests through the
  existing T0420 helper; no production code or new test framework. Focused
  20/20 passed in 9.134 s, down 2.195 s.
- 2026-07-14: complete Canvas/Chat 843 pass + 2 expected skips took 11.375 s,
  down 2.273 s from T0421's 13.648 s. Assets passed; its 26.622 s wall-time
  was noisy and is not claimed as an improvement.
- 2026-07-14: independent review passed with no blockers: fixtures and env are
  isolated, assertions are unchanged, and all process-owned contracts remain
  real subprocesses.
- 2026-07-14: full Windows passed all 10 applicable domains in 31.020 s;
  design was explicitly not applicable. No WSL was used.
- 2026-07-14: Quality: QTECH_001=pass; evidence: focused CLI 20/20; Canvas/Chat 843 pass + 2 expected skips; assets pass; full Windows 10 domains pass; independent review pass
