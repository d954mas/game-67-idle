---
id: T0419
title: Shorten verification critical path without new modes
status: done
project: P001
epic: E001
priority: P1
tags: [verification, performance, tests]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=Targeted Node/Python/new_game suites passed; 10-domain Windows full passed at 39.218s; three independent code reviews returned PASS."}]}
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

- [x] Owner-domain test execution has at most two internal lanes: Node and
  Python. Explicit validate/build commands still run afterwards.
- [x] Parallel-lane failure reporting is deterministic and covered without
  weakening any existing check.
- [x] Cheap `new_game` argument/identity failures call import-safe owner seams;
  subprocess coverage remains for creation, privacy, replacement, rollback,
  and concurrency contracts.
- [x] Python image-tool behavior uses import-safe `main(argv)`/owner seams;
  subprocess tests remain only where exit/stdout/stderr is the contract.
- [x] Canvas CLI refactoring is attempted only if a remeasurement after the
  preceding changes still predicts at least 2 s end-to-end gain.
- [x] Windows Assets, Workspace, and one final `studio verify --full` pass; the
  final full run is at most 40 s on the same host, or the task records why the
  measured bottleneck moved and stops without adding machinery.

## Open questions

- None before implementation. The lane model is internal execution, not a new
  user-facing test taxonomy.

## Log

- 2026-07-14: research localized the cost to test-owned subprocesses and fixture
  work. Discovery is only tens of milliseconds, so map walking, caches, and a
  more elaborate scheduler are explicitly out of scope.
- 2026-07-14: implementation started with generic Node/Python test lanes and
  `new_game` process-boundary reduction.
- 2026-07-14: Assets Node/Python lanes overlap while validate/build commands
  still wait for both; deterministic failure ordering is covered. Assets fell
  from 40.6 s to 21.1 s.
- 2026-07-14: `new_game` batches clean Git probes and directly tests cheap
  parsing/identity rules while retaining real transaction, privacy, rollback,
  dependency, concurrency, and CLI-envelope subprocess coverage. Its suite fell
  from 22.45 s to 18.33 s and removed eight runtime Node children.
- 2026-07-14: source-sheet and review-atlas tests retain four real CLI smokes
  while their repeated domain scenarios use import-safe entry points. The hot
  49-test slice fell from about 13.55 s to 2.14 s and Python children fell from
  about 64 to 4.
- 2026-07-14: Canvas still owns most of the remaining 21.1 s Assets duration.
  Direct CLI conversion was reviewed but deferred: it would coordinate global
  actor/env/stdout state across many files after the full-run target was already
  met, adding more harness than the current measured need justifies.
- 2026-07-14: Windows verification passed: Assets 21.105 s, Workspace 19.462 s,
  and full 39.218 s across all 10 owner domains (design correctly reported
  not-applicable). Independent reviews passed for all three code slices.
- 2026-07-14: closed at commits 820e98a82, 94a04ebab, and d5c696d25 after
  Assets 21.105s, Workspace 19.462s, and full verification 39.218s; no new
  public modes, caches, or scheduler configuration.
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=Targeted Node/Python/new_game suites passed; 10-domain Windows full passed at 39.218s; three independent code reviews returned PASS.
