---
id: E015
title: AI Studio full-audit refactor 2026-07-10
status: done
project: P001
priority: P2
tags: [audit, refactor, process]
created: 2026-07-10
updated: 2026-07-13
---

## Goal

Turn the 2026-07-10 full-repository audit into a small, ordered refactor that
makes AI Studio faster, safer, easier for agents to navigate, and cheaper in
context. Preserve already-working behavior and do not repeat completed work.

## In scope

- Canvas Chat permissions first, then `codex app-server`; Canvas owns chat and
  Studio Shell remains the HTTP host.
- A thin Node `studio` facade, targeted local verification, and full Studio CI
  on Windows and Linux.
- Human Architecture Map tree correctness and storage decomposition without a
  visual redesign or dependency graph.
- Workspace-owned mounts, one game identity source, tested dependency records,
  transactional game creation, and retirement of the closed prototype.
- Taskboard closure/evidence integrity, collision-safe readable IDs, minimal
  agent context, and one-time stale-status cleanup.
- Shared Studio Python/CMake/toolchain cleanup, accepted legacy deletion,
  asset-integrity hardening, and focused generator refactors/benchmarks.
- Short feature contracts, game-owned packaging, and existing platform-sdk
  reuse.

## Accepted contracts

- Studio verifies Studio, shared features, and the reference template. It does
  not discover or run game tests locally or in Studio CI; every game owns its
  tests, playable proof, packaging, doctor, and CI.
- Local agent loop: `studio verify --changed`. CI: `studio verify --full` on
  Windows and Linux.
- Architecture Map remains the current human tree. No arrows, dependency graph,
  `dependsOn`, `usedBy`, or command registry. Coverage truth is `git ls-files`.
- Shared Canvas storage remains external. No game-local/private Canvas store.
- Current shared features are the supported source. Games record tested
  versions in `dependencies.json` and may make a local fork. No feature
  snapshots, cache, sync/link system, or archive of old versions.
- A game is identified by `game.json`; workspace catalogs own mounts, not
  duplicate identity. Game creation uses staging, validation, atomic rename,
  rollback, and explicit clean replacement.
- The root `.venv` is the common Studio Python environment; heavyweight tools
  may retain isolated environments. Do not bundle another Python.
- Do not build a Studio-owned model scheduler/router or live model evaluation
  suite. Use the host Codex/Claude orchestration boundaries.
- Agent-role preflight is operational, not a catalog refactor: after a harness
  restart, smoke the requested stock role and verify both `agent_role` and the
  actual selected model; a generic fallback is a failure.
- Liveops remains deferred until a real release-candidate game and portal.

## Out of scope

- Reworking Codex/Claude role catalogs or introducing provider-neutral roles.
- Maintaining or migrating `rb-dark-rpg` as an active game.
- Studio-owned game lifecycle CI.
- A Canvas visual redesign, service framework, DI container, or event system.
- Feature-version archives and automatic dependency migration.
- Balance Workbench implementation; owned by `E016`.

## Related existing work

- `T0242`: existing Canvas Chat implementation; its prompt-only permission
  model is superseded by `T0350` and transport by `T0351`.
- `T0327`, `T0328`, `T0337`: feature/state/items/progression work exists, but
  task metadata must be reconciled rather than reimplemented.
- `T0341` through `T0348`: private workspace gates mostly exist; preserve them
  and finish the real delta instead of creating a second privacy architecture.
  `T0347` is now obsolete because the lead chose deletion rather than migration
  of `rb-dark-rpg`; `T0375` owns its truthful cancellation/reconciliation.
- `T0218`, `T0323`, `T0333`, `T0339`: reuse existing Python, web packaging,
  platform, and release work where applicable.

## Final audit coverage

The convergence pass classified every one of the 1,818 paths returned by
`git ls-files` with non-overlapping packets:

- 329 core/harness/Taskboard/Workspace/Architecture/Runtime Automation files;
- 426 remaining AI Studio assets/design/quality/workflow files;
- 1,049 template/feature/game/root/external-boundary paths;
- 14 residual skeletal-extension and obsolete MCP descriptor files.

Text/code/config/test files were read and mechanically checked; high-risk seams
received targeted semantic review. Binary assets were hash/metadata/routing
checked rather than interpreted as source. The Neotolis engine is one pinned
gitlink: its metadata/boundary was checked, while engine implementation remains
outside this repository audit and read-only by policy.

At planning time, one tracked WAV was absent in the preserved T0393 conversion
WIP and two Architecture Map tests exposed the 11-vs-12 hard-coded count owned
by T0372. Both exceptions are now resolved: T0393 records and verifies the WAV
and MP3 delivery bytes, while the final Architecture Map suite passes 28/28.
New uncovered issues were routed to T0398 (malformed URL crash), T0399
(temporary asset storage), and the T0358 split T0396/T0397. No path remains
unclassified.

## Execution order

Priority labels select urgency inside a ready wave; they never override
dependencies. Tasks in the same wave may run in parallel only when they do not
touch the same owning files.

### Wave 0 - truthful baseline

- T0375: reconcile stale statuses and cancel obsolete migration. This is the
  only selected `todo` and runs first.
- T0393 is paused in `backlog`; its existing partial WIP is preserved.
- Planning-only duplicates T0394 and T0360 are closed as superseded by T0349
  and E016 respectively.
- T0362 closes the remaining planning discussion without creating a generic
  performance framework: audio routes to T0393, quality closeout to T0394,
  and trustworthy native iteration proof to T0395.

### Wave 1 - independent foundations

- T0373: collision-safe Taskboard IDs.
- T0350: Canvas Chat permission boundary.
- T0372: Architecture Map validator/description baseline.
- T0355: one Workspace Catalog, identity, and dependency contract.
- T0357: root Python/CMake/toolchain boundaries and build baseline.
- T0359: Game State generator decomposition and benchmark baseline.
- T0376: truthful host/validator/process enforcement contracts.
- T0358: allow-list legacy deletion and encoding cleanup.
- T0396: quarantine incomplete skeletal extension.
- T0398: malformed-URL survival in Studio Shell.

### Wave 2 - dependent contracts

- T0349 after T0373: truthful closure and explicit quality decision.
- T0351 after T0350: Canvas Chat app-server transport.
- T0371 after T0372: remove the legacy Architecture Map graph.
- T0356 after T0355: transactional new_game and prototype retirement.
- T0361 after T0355: versioned feature contracts consuming dependencies.json.
- T0377 after T0359: progression/state-schema contract repair.

### Wave 3 - verification and integrity foundations

- T0374 after T0373 and T0349: scoped Taskboard context profiling.
- T0352 after T0351: Canvas physical decomposition behind stable facades.
- T0354 after T0371 and T0372: recursive map storage split with visual parity.
- T0397 after T0358 and T0356: asset integrity backfill and enforcement.

### Wave 4 - measured integrations and full CI

- T0393 after T0357 and T0397: resume/finish hybrid audio using the canonical
  build and integrity contracts. The lead approved the actual dependency
  reorder `T0397 -> T0393 -> T0353` so full CI integrated the audio result.
- T0353 after T0357, T0361, and T0393: canonical studio verify plus
  Windows/Linux CI.
- T0395 after T0357 and T0353: trustworthy native edit-to-DevAPI proof loop.
- T0400 after T0353, T0355, T0356, T0361, and T0397: game-owned packaging
  and final ZIP verification; T0400 proves its new scaffold through T0353.
- T0399 after T0397: graduate temporary asset storage modules.

### Wave 5 - portal evidence and epic proof

- T0401 after T0400: platform capabilities and honest portal evidence levels.
- Run the complete T0353 `studio verify --full` contract on Windows and Linux.
- Confirm every E015 task has checked criteria, explicit quality decision, and
  linked evidence; re-run coverage/Taskboard validation and close the epic.

No product implementation is authorized merely by this ordering. Moving a task
to `doing` remains an explicit execution decision.

## Completion evidence

- [x] All 30 E015 tasks are archived as `done`; every acceptance checkbox is
  checked and every task records an explicit Quality decision and evidence or
  a planning-only not-applicable reason.
- [x] The actual preserved dirty Windows worktree after T0393 and T0401 passed
  `node ai_studio/studio.mjs verify --full`: all 28 suites passed in 227.5
  seconds. No reset, stash, cleanup, benchmark loop, or engine edit was used.
- [x] Clean committed implementation proof passed on both platforms: GitHub
  Actions run 29245097104 at `69dcf275d` completed green on Ubuntu and Windows.
- [x] Final Taskboard validation, strict Architecture Map coverage, document
  references, enforcement contract, and agent-surface synchronization pass.
- [x] Two independent completion reviews cover requirements/ownership and
  tests/process/performance; all closeout findings were resolved before the
  final commit.

## Log

- 2026-07-10: Created after a full audit discussion. Verified the original
  Codex rollout (`019f493f-c007-77f2-b62a-9c21654c70ee`): 116 real user
  messages. Three subagents independently reconstructed messages 1-40, 41-87,
  and 88-116 with adjacent assistant proposals to resolve terse numeric
  choices. Accepted, rejected, superseded, and open decisions were separated
  before task creation.
- 2026-07-10: Existing Taskboard/code state was compared before planning. The
  plan intentionally links completed/stale cards instead of duplicating their
  implementation.
- 2026-07-10: Final plan convergence classified 1,818/1,818 tracked paths,
  merged duplicate quality planning, closed stale pre-Lua Items work, split the
  mixed cleanup task, fixed open decisions, and recorded four dependency waves.
- 2026-07-10: Review cycle 1 returned 0 HIGH and 3 actionable findings; all three were incorporated by tightening T0397 order, splitting T0361 into T0361/T0400/T0401, and ratifying the audio delivery-format policy.
- 2026-07-10: Review cycle 2 returned 0 HIGH and 2 actionable dependency findings; T0355 now precedes T0361, and T0400 owns proof of its newly created game verify scaffold through the landed T0353 contract.
- 2026-07-10: Review cycle 3 converged with current_high=0 and current_actionable=0; no unresolved plan concern remains outside E015/tasks.
- 2026-07-10: Final planning evidence: Taskboard validation passed, git diff --check passed, 30 E015 cards are 26 backlog / 1 todo / 3 intentionally done, and T0375 is the only selected execution task.
- 2026-07-10: Quality: not-applicable; reason: planning-only Taskboard convergence with no product implementation.
- 2026-07-13: Completed the Wave 0 baseline and execution Waves 1 through 5.
  The final implementation tip is `69dcf275d`; GitHub Actions run 29245097104
  passed the full 28-suite gate
  on Ubuntu and Windows, and the same full contract passed once on the actual
  preserved dirty Windows worktree after T0393/T0401.
- 2026-07-13: Closeout reconciled T0360, T0362, and T0394 into the E015 archive,
  removed the two stale planning-time exceptions, and recorded the approved
  `T0397 -> T0393 -> T0353` execution reorder. No E016, E017, game WIP,
  counters, temporary browser output, or external engine change is part of the
  closeout.
- 2026-07-13: Quality: QTECH_001=pass; evidence: 30/30 tasks closed with
  explicit quality/evidence, actual-worktree full verify 28/28, committed
  Windows/Linux full CI green, final repository gates green, and independent
  completion reviews at zero actionable findings.
