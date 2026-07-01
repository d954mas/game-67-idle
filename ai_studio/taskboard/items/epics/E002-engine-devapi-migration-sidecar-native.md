---
id: E002
title: Engine DevAPI migration (sidecar -> native)
status: active
project: P001
priority: P2
tags: []
created: 2026-06-19
updated: 2026-07-01
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
- 2026-07-01: updated `external/neotolis-engine` to 4d6dcc42, where cJSON and
  the default DevAPI groups are engine-owned. Rewired the seed template to link
  engine DevAPI/debug groups in Debug builds, keep Release automation-free, and
  removed stale sidecar UI files. Runtime Automation now uses the engine default
  native port 17890 and engine-native PNG capture when available.
- 2026-07-01: added a copied template DevAPI smoke bot under
  `templates/template/devapi/`, plus a `devapi_smoke` CMake target. The example
  launches the game, discovers endpoints, checks `command.describe`, waits for
  `ui.tree`, toggles `render.set_enabled`, and captures PNG evidence.
- 2026-07-01: Quality: QCLR_002=review; QTECH_001=pass; evidence:
  `quality_responsive` captures the 4:3/16:9/tall-phone landscape+portrait
  screenshot matrix plus `ui.tree` bounds under `tmp/quality/qclr_002_responsive/`.
- 2026-07-01: Re-added the template-owned `game.state` endpoint on the engine
  DevAPI bus under `templates/template/src/devapi/`, and made the copied smoke
  bot assert it as the default game snapshot surface.
