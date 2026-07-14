---
id: T0412
title: Consolidate Studio verification domains and continue measured test acceleration
status: done
project: P001
epic: E018
priority: P0
tags: [verification, performance, tests, windows]
created: 2026-07-13
updated: 2026-07-13
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=Final Windows full 10 domains pass in 49.3s; repeated warm runs 46.2-49.3s versus 186.4s baseline; focused routing/CI tests and independent audit pass; Ubuntu remains mandatory pre-merge CI."}]}
---

## What

Replace the manual 28-suite execution model with coarse owner domains and two
proof classes: fast owner checks and explicit release proof. Keep test files
auto-discovered by owning roots, continue profiling real bottlenecks, and make
the normal Windows agent loop fast without deleting behavioral coverage.

## Done when

- [x] The registry has at most ten owner domains and no formal L0-L4 or
      per-test classification.
- [x] All current 28 suites receive a one-time owner-level disposition with a
      reason: keep in an owner domain, merge, delete as obsolete structure, or
      retain as explicit release proof. This audit is evidence, not a new
      per-test registry to maintain.
- [x] The working domain baseline is `harness`, `workspace`, `architecture`,
      `shell`, `assets`, `work-management`, `design`, `runtime`, `features`,
      and `template-release`; measured ownership may justify merging domains,
      but not splitting them into test-file categories.
- [x] `verify --changed` runs affected owner checks, `verify --domain <id>`
      supports focused debugging, and `verify --full` includes release proof.
- [x] Unknown shared paths fail with an ownership error instead of silently
      running every suite; game-owned paths remain excluded.
- [x] Independent Node tests run with bounded safe concurrency and only proven
      shared-state cases remain serial.
- [x] Two warm Windows full runs are at or below 90 seconds, with domain timings
      emitted directly by the facade and no WSL dependency.
- [ ] Windows and Linux CI keep native/web/package/platform proof green.
- [x] Deterministic Node/Python tests are discovered by owner convention;
      browser, external-service, model, live-runtime, and toolchain-dependent
      checks remain explicit release proof.
- [x] The generic `studio.validation` bucket is removed. Feature audio checks
      belong to `features`; native composition/release checks belong to
      `template-release`, without a duplicate `audio.native` suite.
- [x] Canvas, asset Python, new_game, and Taskboard overhead is measured before
      changing test seams. Domain/Node concurrency and built-in unittest
      batching meet the budget without risky handler rewrites; API, privacy,
      provenance, CLI, and transactional behavior coverage remains.

## Open questions

- The 75-second stretch target was exceeded without handler rewrites;
  revisit direct-handler conversion only if a focused domain regresses enough
  to justify the added seam.
- Watch one non-reproduced `test_game_save` failure seen during repeated full
  runs. Its targeted CTest, the complete template-release domain, and the next
  full run all passed; capture the original failing assertion if it recurs.

## Suite disposition

- Keep as owner checks: facade, Architecture Map, Canvas, assets, Core Harness,
  dev environment, Quality, runtime automation, shell, Taskboard, workspace,
  game creation, feature contracts/audio/platform/state/items/progression, and
  reference-template tools.
- Merge into owners: Canvas Python, asset Python, skill Python, and template
  Python checks.
- Release proof: template native and web/package; Linux live-runtime stays
  release-only inside `runtime`, and the unique strict Linux audio compile stays
  release-only inside `features`.
- Remove or route to owners: duplicate game-design doc check,
  `studio.validation`, and duplicate `audio.native`.

## Log

- 2026-07-13: Started from a measured 186.4-second full Windows run. Bounded
  Node file concurrency and a reusable isolated new_game fixture reduced the
  same 28/28 full contract to 113.3 seconds. Canvas fell from about 60.9 to
  20.3 seconds and new_game from about 40.1 to 24.7 seconds.
- 2026-07-13: Remaining measured leaders are asset Python 27.0 seconds,
  new_game 24.7, Canvas 20.3, assets Node 9.1, and Taskboard 8.2. A one-process
  Python probe saved only about five seconds and was rejected as insufficient
  value for new infrastructure.
- 2026-07-13: Quality: QTECH_001=pass; evidence: focused facade tests, Canvas
  concurrency proof, new_game regression suite, full Windows 28/28, and
  before/after wall-clock measurements.
- 2026-07-13: Replaced 28 public suites with ten owner domains and added
  changed/domain/full routing, fail-closed unknown ownership, convention-based
  Node/Python discovery, one Node process per domain, built-in asset unittest
  batching, and domain concurrency two. Full Windows warm runs passed in 48.5
  and 48.7 seconds; final post-discovery run passed in 46.2 seconds. A Windows
  `.pytest_cache` EPERM found by full proof now has a regression test.
- 2026-07-13: Independent review found and resolved loss of the strict Linux
  audio proof, unowned Studio config/VS Code paths, and the empty design false
  pass. Post-review full Windows proof passed in 46.9 seconds; focused facade
  tests passed 25/25, including a cross-platform Linux audio command contract.
  Quality: QTECH_001=pass; evidence: changed/domain/full behavior tests,
  convention discovery/cache regression tests, two sub-90-second warm full
  runs, post-review full release proof, and resolved independent review.
- 2026-07-13: Closure audit found `games/root-shared.txt` was incorrectly
  excluded as game-owned. A regression was added first; `isGameOwned` now
  requires `games/<id>/...`, and the Studio facade passes 23/23. Local
  architecture/performance proof is complete; the final Windows+Ubuntu CI
  checkbox remains intentionally open because current external CI state cannot
  be established locally. No WSL substitute was used.
- 2026-07-13: Final local close: real-worktree Windows full passed all ten domains in 49.3s. The unchecked live-CI criterion is explicitly waived only for local close because CI cannot run on uncommitted changes; Windows+Ubuntu full remains mandatory before merge, with no WSL substitute.
- 2026-07-13: Closure: waived; reason: Current Ubuntu CI cannot execute against an uncommitted local worktree; the Windows+Ubuntu full matrix remains an unchanged required pre-merge gate and is not substituted with WSL.; evidence: Final real-worktree Windows full verification passed all 10 domains in 49.3s; focused facade/CI contracts and independent closure audit pass; GitHub workflow still runs full proof on Windows and Ubuntu.
- 2026-07-13: Quality: QTECH_001=pass; evidence: QTECH_001=Final Windows full 10 domains pass in 49.3s; repeated warm runs 46.2-49.3s versus 186.4s baseline; focused routing/CI tests and independent audit pass; Ubuntu remains mandatory pre-merge CI.
