---
id: E019
title: Items Runtime v2 containers and atomic transactions
status: done
project: P001
priority: P2
tags: [items, runtime, state, containers]
created: 2026-07-14
updated: 2026-07-17
---

## Goal

Replace the legacy fixed purse/backpack and string-key ownership model with a
bounded, save-safe container/entry runtime without blocking the independent
Items Lua authoring and Workbench vertical.

## In scope

- Exact-u32 and bounded nested Game State generation.
- Dynamic containers, numeric entry identity, ordering, capacity, and saves.
- Migration of legacy ownership/event consumers.
- Atomic payment, acquisition, and upgrade verbs over explicit payment scopes.

## Out of scope

- Items Lua evaluation, Snapshot, Viewer/Workbench, or JSON authoring cutover;
  those remain in E016.
- A global core purse/backpack or game-specific inventory policy.

## Done when

- [x] Generated state supports exact u32 identity and bounded nested containers.
- [x] Persistent and ephemeral containers have explicit ownership, lifetime,
      inspection, reconciliation, and save boundaries.
- [x] Payment, acquisition, upgrade, move, and ownership mutations are atomic
      and produce bounded numeric-identity audit events.
- [x] Frozen legacy saves migrate deterministically with whole-document owner
      validation, corruption rejection, quarantine, and restoration coverage.

## Log

- 2026-07-14: Extracted from E016 during Taskboard grooming. Runtime/state
  redesign is valuable but must not block the smaller authoring vertical.
- 2026-07-17: Completed through T0390, T0391, and T0392. Full Studio verification
  passes all 10 domains; GitHub Actions 29567771839 passes on Ubuntu and Windows;
  independent reviews converged to ACCEPT.
- 2026-07-17: Runtime v2 complete via T0390-T0392; full verify and cross-platform CI pass.
