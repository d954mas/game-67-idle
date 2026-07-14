---
id: T0423
title: Finish Canvas success-only CLI parity conversion
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

Convert the final five wholly success-only parity scenarios in
`canvas/tests/cli.test.mjs` through the existing in-process dispatcher helper.
Do not touch mixed transport/actor/env/error/stdout scenarios.

## Done when

- [x] Convert only nesting, add-images, groups-set, list-summary, and element
  resize/reposition scenarios; add no new helper or production API.
- [x] Focused CLI improves by at least 2 s from the 9.134 s T0422 baseline.
- [x] Focused, complete Canvas/Chat, Assets, and full Windows pass.
- [x] Independent review confirms the remaining subprocess set is
  process-owned or deliberately deferred.

## Decisions

- Keep the batch: the diagnostic repeat confirmed a 2.241 s focused gain.
- Stop here. Remaining subprocess calls are process-owned scenarios or setup
  kept inside mixed transport tests; splitting those further would reduce
  scenario clarity for a smaller expected gain.

## Log

- 2026-07-14: created after T0422 reduced focused CLI to 9.134 s and
  Canvas/Chat to 11.375 s while preserving all process-owned contracts.
- 2026-07-14: final success-only batch started; mixed boundary scenarios
  explicitly excluded.
- 2026-07-14: converted 34 launches across the five named success-only tests.
  The first focused timing was a 9.423 s host-load outlier; the one diagnostic
  per-test repeat passed 20/20 in 6.893 s and showed every converted test at
  25-50 ms, down 2.241 s from the 9.134 s T0422 baseline.
- 2026-07-14: Canvas/Chat 843 pass + 2 expected skips took 10.057 s. Assets
  passed in 13.639 s.
- 2026-07-14: independent review passed with no blockers and confirmed that
  stopping preserves coherent mixed transport scenarios without new helpers.
- 2026-07-14: full Windows passed all 10 applicable domains in 29.651 s;
  design was explicitly not applicable. No WSL was used.
- 2026-07-14: Quality: QTECH_001=pass; evidence: focused CLI 20/20; Canvas/Chat 843 pass + 2 expected skips; assets pass; full Windows 10 domains pass; independent review pass
