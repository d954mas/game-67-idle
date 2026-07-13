---
id: T0387
title: Add post-v1 scenario history statistics and multi-series charts
status: idea
project: P001
epic: E016
priority: P2
tags: [balance, charts, analytics]
created: 2026-07-10
updated: 2026-07-10
---

## What

After the narrow single-series chart proves the snapshot/query contract, decide
and implement the advanced comparison workflow informed by Machinations and
Unreal rather than silently expanding v1.

## Done when

- [ ] Persistent scenario/run history records source/snapshot hash, inputs,
      backend/tool version, timestamp, and reproducible result identity.
- [ ] Selected runs expose mean/median/min/max and distributions where the
      scenario is stochastic or batched; deterministic runs are labelled.
- [ ] Multi-series charts support explicit units, left/right axes, scale choice,
      interpolation, highlighting, and bounded/downsampled queries.
- [ ] History and charts remain local/Git-or-cache controlled, do not become a
      cloud/liveops service, and never become a second source of formulas.
- [ ] UX/performance budgets are measured and accepted before this card moves
      from `idea` to implementation.

## Open questions

- Which run artifacts are committed evidence versus disposable local cache?

## Log

- 2026-07-10: Created from final competitor re-review to trace adopted
  Machinations/Unreal patterns without bloating the weapon v1 slice.
