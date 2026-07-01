---
id: E002
title: Engine DevAPI migration (sidecar -> native)
status: active
project: P001
priority: P2
tags: []
created: 2026-06-19
updated: 2026-06-19
---

## Goal

Replace the repo sidecar DevAPI (src/devapi/) with the engine's now-native DevAPI
(engine/devapi/, pulled at 8ec758b7) so the seed and future games use the engine's
better bus/transport/input/time/discovery, keeping only repo-owned glue. Staged,
gated behind NT_DEVAPI_ENABLED, build-verified, reversible (do not delete the
sidecar until the engine path builds green and smoke.py passes).

## In scope

- Delete duplicated sidecar mechanics (transport, dispatch core, input/time builtins).
- Rewire seed + state codegen + python client + CMake onto the engine bus.
- Re-register the game-owned vocabulary (ui.*/entity.list/game.*/game.state.*) as
  group="game" on the engine bus (the engine deliberately omits these).
- Keep `nt-runtime-automation` aligned with the engine DevAPI spec.

## Out of scope

- Changing the engine (read-only submodule).
- New game content/concept (tracked separately, e.g. T0011).
- Keeping the sidecar once the engine path is green.

## Log

- 2026-06-19: opened after pulling engine to 8ec758b7 (native modular DevAPI +
  synthetic input). Full delete/rewire/keep plan + 12 steps + risks from the dedup
  analysis workflow; captured in T0012.
