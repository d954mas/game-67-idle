---
id: E015
title: AI Studio full-audit refactor 2026-07-10
status: active
project: P001
priority: P2
tags: [audit, refactor, process]
created: 2026-07-10
updated: 2026-07-10
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
