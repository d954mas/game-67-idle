---
id: E019
title: Items Runtime v2 containers and atomic transactions
status: active
project: P001
priority: P2
tags: [items, runtime, state, containers]
created: 2026-07-14
updated: 2026-07-14
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

## Log

- 2026-07-14: Extracted from E016 during Taskboard grooming. Runtime/state
  redesign is valuable but must not block the smaller authoring vertical.
