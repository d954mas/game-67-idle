---
id: T0419
title: Shorten verification critical path without new modes
status: backlog
project: P001
epic: E001
priority: P1
tags: [verification, performance, tests]
created: 2026-07-14
updated: 2026-07-14
---

## What

Reduce the Windows verification critical path by removing repeated process
boundaries inside tests. Keep the existing 10 owner domains and the three
public intents (`changed`, `domain`, `full`); do not add test levels, caches, or
per-domain scheduler configuration.

Measured 2026-07-14 baseline:

- full verification: 49.3 s;
- Assets: 40.6 s sequential; Node 21.3 s and Python 21.0 s;
- the same Assets Node/Python work completed together in 24.8 s with both lanes
  passing;
- Workspace support tests: 2.3 s; `games/new_game.test.mjs`: 21.8 s;
- Assets Python: the 20-file batch is 13.6 s and
  `audit_intake.test.py` is 6.0 s; both repeatedly cross a Python CLI boundary.

## Done when

- [ ] Owner-domain test execution has at most two internal lanes: Node and
  Python. Explicit validate/build commands still run afterwards.
- [ ] Parallel-lane failure reporting is deterministic and covered without
  weakening any existing check.
- [ ] Cheap `new_game` argument/identity failures call import-safe owner seams;
  subprocess coverage remains for creation, privacy, replacement, rollback,
  and concurrency contracts.
- [ ] Python image-tool behavior uses import-safe `main(argv)`/owner seams;
  subprocess tests remain only where exit/stdout/stderr is the contract.
- [ ] Canvas CLI refactoring is attempted only if a remeasurement after the
  preceding changes still predicts at least 2 s end-to-end gain.
- [ ] Windows Assets, Workspace, and one final `studio verify --full` pass; the
  final full run is at most 40 s on the same host, or the task records why the
  measured bottleneck moved and stops without adding machinery.

## Open questions

- None before implementation. The lane model is internal execution, not a new
  user-facing test taxonomy.

## Log

- 2026-07-14: research localized the cost to test-owned subprocesses and fixture
  work. Discovery is only tens of milliseconds, so map walking, caches, and a
  more elaborate scheduler are explicitly out of scope.
