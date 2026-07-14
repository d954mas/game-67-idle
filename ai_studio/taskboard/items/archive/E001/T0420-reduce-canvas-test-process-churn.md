---
id: T0420
title: Reduce Canvas test process churn
status: done
project: P001
epic: E001
priority: P1
tags: [canvas, verification, performance, tests]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"focused 20/20; Canvas/Chat 843 pass + 2 expected skips; assets domain pass; full Windows 10 domains pass; independent review pass"}]}
---

## What

Reduce avoidable child Node processes in the Canvas test suite while keeping
the real CLI transport, actor, store-selection, locking, stdout/stderr, and
external-tool contracts covered. Prefer direct calls through the existing
Canvas owner over a new shared test framework.

## Done when

- [x] A measured baseline identifies the files and CLI calls that own the
  avoidable process cost.
- [x] The smallest import-safe Canvas CLI seam has no global actor, cwd, env,
  stdout, or store-selection leak between tests.
- [x] Repeated parity scenarios run in-process; real subprocess smokes remain
  for transport envelopes and process-owned behavior.
- [x] Focused changed-file tests, the complete Canvas/Chat suite, Assets domain,
  and one Windows full verification pass.
- [x] Canvas or Assets improves by at least 2 s on the same host; otherwise the
  attempted seam is removed instead of retained as ceremony.

## Decisions

- This slice converts 35 calls in `tests/cli.test.mjs`; `recipe.test.mjs` and
  `pack.test.mjs` are the next measured candidates only if another speed task
  is justified.
- Actor-sensitive coverage stays in a real CLI subprocess. This task does not
  expand the public ops API or add actor-scoping infrastructure.

## Log

- 2026-07-14: created after T0419 reduced full Windows verification to 39.218 s
  and left Canvas as the dominant part of the 21.105 s Assets domain.
- 2026-07-14: implementation started with read-only process mapping and CLI
  seam risk review; no production design selected before measurement.
- 2026-07-14: baseline isolated the bottleneck to `tests/cli.test.mjs`: about
  165 child processes and 16.76 s. The complete 65-file Canvas/Chat suite took
  19.988 s; a cold Canvas CLI launch averaged 93 ms.
- 2026-07-14: added a small importable dispatcher seam with explicit repo root
  and result writer. Converted 35 repeated success-path parity calls in three
  representative tests. Exit/stderr failures, environment/private-store
  routing, agent attribution, human stdout, locks, and external tools remain
  real subprocess boundaries.
- 2026-07-14: independent review found and verified a fixture isolation fix:
  temporary repo and Canvas roots are separate, and an ambient
  `CANVAS_PROJECTS_ROOT` is restored exactly after each in-process call.
- 2026-07-14: focused 20/20 passed in 13.068 s (best repeat 13.016 s), down
  3.69-3.74 s from 16.76 s. Canvas/Chat 843 pass + 2 expected skips took
  17.179 s, down 2.809 s from 19.988 s. Assets passed in 19.125 s versus
  21.105 s. Full Windows passed 10 domains in 39.757 s; design was explicitly
  not applicable. No WSL was used.
- 2026-07-14: Quality: QTECH_001=pass; evidence: focused 20/20; Canvas/Chat 843 pass + 2 expected skips; assets domain pass; full Windows 10 domains pass; independent review pass
