---
id: T0424
title: Profile and simplify remaining verification domains
status: done
project: P001
epic: E001
priority: P1
tags: [verification, performance, tests]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"65 Taskboard tests; 46 new_game tests; 44 template tests; 10-domain Windows full pass; three independent reviews"}]}
---

## What

Profile every verification domain not deeply covered by T0419-T0423, then
remove only measured process/setup overhead without adding public test levels,
caches, workers, or scheduler configuration.

## Done when

- [x] Record per-domain and per-file baselines plus real process-boundary classifications.
- [x] Optimize only owners with at least 1.5-2 s measured gain and no weaker coverage.
- [x] Keep real exit/stdout/stderr/env/locking/concurrency/transaction boundaries in subprocesses.
- [x] Pass focused owner tests and one full Windows verification run.
- [x] Record QTECH_001 evidence and leave the worktree clean in atomic commits.

## Open questions

- None. Stop an owner slice if the measured gain does not justify its complexity.

## Log

- 2026-07-14: Started from a clean 30.6 s Windows full baseline. Domain timings:
  workspace 19.93 s, assets 14.45 s, template-release 7.07 s,
  work-management 6.18 s, features 2.26 s, harness 2.24 s; all other
  applicable domains below 1 s. Canvas/Assets are stop zones unless new
  evidence disproves the T0423 boundary review.
- 2026-07-14: Audited all 185 tracked deterministic test files; every file is
  assigned to an owner domain. Small domains remained below the optimization
  threshold. Template release's 7 s is mostly real configure/build/CTest/web
  release proof; no new runner level, cache, worker, or scheduler was added.
- 2026-07-14: Taskboard removed 31 avoidable Node CLI children (75 to 44),
  retained 6 CLI transports, 24 allocator workers, and 14 Git fixtures, and
  improved its focused suite from 5.90 s to 4.03 s. A RED-GREEN regression
  test caught and fixed duplicate missing-id error output. Independent review
  passed.
- 2026-07-14: Workspace removed two duplicate full game creations plus 22
  avoidable Node/Git executions while retaining explicit public/private,
  privacy, rollback, replacement, dependency, and concurrency processes.
  Focused new_game passed 46/46 in 14.70 s; independent review passed.
- 2026-07-14: Template tests removed six redundant Git processes and one
  misleading duplicate integration iteration. Feature version/revision proof,
  real package CLI, ZIP/reopen, publication race, and reporter transports
  remain covered; 44/44 passed in 1.30 s and independent review passed.
- 2026-07-14: Final Windows full verification passed all 10 applicable owner
  domains in 33.8 s; design was explicitly not applicable. The host was slower
  than the 30.6 s baseline across Workspace, Assets, and template-release, so
  no aggregate wall-time improvement is claimed from this single noisy run.
- 2026-07-14: Quality: QTECH_001=pass; evidence: 65 Taskboard tests; 46 new_game tests; 44 template tests; 10-domain Windows full pass; three independent reviews
