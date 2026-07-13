---
id: T0361
title: Version reusable feature contracts and synchronize skill routers
status: done
project: P001
epic: E015
priority: P1
tags: [features, semver, contracts, context]
created: 2026-07-10
updated: 2026-07-11
---

## What

Give each reusable feature one short versioned human/agent contract without
creating feature archives, a package manager, or duplicated skill prose.
Packaging and platform/portal work are split to T0400 and T0401.

## Done when

- [x] Every reusable feature declares mandatory SemVer in its existing
      feature metadata and has one concise owning router for purpose, public
      surface, validation, compatibility, and extension points.
- [x] Matching skills route to the owning feature contract and contain only
      workflow guidance that cannot live in feature docs; duplicated
      long-form contracts are removed or reduced to links.
- [x] T0355 `dependencies.json` records tested engine/feature versions and
      compatibility without copying feature source or contract snapshots.
- [x] Version changes have an explicit rule for patch/minor/major and tests
      prove metadata/docs/skill synchronization.
- [x] T0353 feature/reference-template verification consumes these contracts
      and remains independent from arbitrary game tests.
- [x] No feature archive, dependency cache, sync/link command, tag strategy,
      automatic migration, packaging implementation, or portal claim is added.

## Open questions

None.

## Log

- 2026-07-10: Final review split game-owned packaging to T0400 and
  platform/portal evidence to T0401 to keep this task context-minimal.
- 2026-07-10: Execute in Wave 1 before T0353 consumes feature contracts.
- 2026-07-10: Dependency corrected in review cycle 2: execute after T0355 defines the dependencies.json contract; T0353 remains after T0361.
- 2026-07-11: Checkpoint: started after T0355 and T0356. Inventorying reusable feature contracts and router overlap before edits. The preserved T0393 audio-core/template WIP and the second agent's games/web-dressup work are excluded; no audio-core or dress-up path will be changed or staged unless repository ownership proves a safe T0361-only boundary.
- 2026-07-11: Blocked checkpoint: read-only inventory proved that committed templates/template/game-dependencies.json already consumes audio-core, while audio-core feature.json/README/INSTALL and its features/README registration exist only in the preserved untracked/dirty T0393 WIP. T0361 cannot enforce mandatory SemVer and synchronized contracts for every consumed reusable feature without changing/committing T0393 before Wave 4, which the lead explicitly prohibited. games/web-dressup/dependencies.json also needs the versioned schema but is owned by the concurrently running dress-up agent. No T0361 implementation files were changed or staged. Resolution requires a lead-approved metadata-only ownership carve-out and coordination with the dress-up agent, or an explicit dependency/order revision.
- 2026-07-11: Lead authorization: approved metadata-only carve-out of audio-core feature.json/README/INSTALL and the owning features/README router from preserved T0393 WIP. Runtime/backend/tests/benchmarks/template audio files remain T0393 and must not be changed or staged. Dress-up dependency/codegen files may be updated only after exact no-overlap verification.
- 2026-07-11: TDD evidence: RED proved missing skill-route synchronization, empty router bodies, missing PATCH/MINOR/MAJOR rules, undeclared root feature directories, seed/manifest drift, and invalid engine SemVer; GREEN feature-contract validator 8/8 and live inventory 6 modules/2 pointers.
- 2026-07-11: Integration evidence: new_game 42/42; new_template 4/4; workspace 23/23; affected Catalog consumers 165/165; Architecture Map 23/23 plus strict 354 mapped/0 issues; doc references 10/10; Taskboard validation green.
- 2026-07-11: Review cycle 1 found one HIGH v1-fixture cutover regression, two actionable MEDIUM validator gaps, and two LOW mapping/template-fixture drifts; all fixed. Two independent final read-only rechecks returned 0 HIGH and 0 actionable MEDIUM/LOW.
- 2026-07-11: Scope evidence: audio carve-out contains metadata/docs/router only; T0393 runtime/backend/tests/benchmarks/template audio WIP and unrelated web-dressup work remain unstaged. No archives, caches, sync/link commands, tag strategy, migration automation, packaging, or portal claims were added.
- 2026-07-11: Quality: QTECH_001=pass; evidence: exact contract parsers plus focused, full creation, cross-consumer, workspace, map, documentation, and Taskboard suites above.
