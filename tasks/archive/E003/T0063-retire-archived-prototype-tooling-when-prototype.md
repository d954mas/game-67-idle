---
id: T0063
title: Retire archived-prototype tooling when prototypes are formally closed
status: done
epic: E003
priority: P3
tags: [subtraction, tooling, cleanup]
created: 2026-06-15
updated: 2026-06-15
---

## What

NOT a P3 housekeeping cut -- it is a direction decision. Finding (2026-06-15):
the project-specific tooling is NOT orphaned. `tools/playtest/*_probe.py` are
referenced only by closed-project docs (safely removable), BUT the builders are
wired into the BUILD: `CMakeLists.txt:74-96` sets and uses
`build_rune_marches_assets.py`, `build_rune_marches_ui_kit_v2.py`,
`build_rune_marches_ui_bases_v2.py`, `build_rune_marches_ui_compact_bases_v5.py`,
`build_roblox_fishing_ui_assets.py`, and `tools/roblox_fishing/*` behind
`GAME_CLOSED_PROTOTYPES_ENABLED`. AGENTS.md INTENTIONALLY KEEPS that closed-
prototype runtime as opt-in history ("Closed prototype runtime history remains
in `src/main.c` and is opt-in"). So retiring this tooling = dropping the entire
opt-in closed-prototype build/runtime from the template, which reverses that
AGENTS decision and touches CMake + src/main.c + src/generated + assets/runtime.

## Done when

- [x] LEAD decided: DROP the opt-in closed-prototype runtime/tooling from the template (test runs; design evidence kept under `gamedesign/projects/`).
- [x] DROP executed: removed the closed-prototype CMake blocks + `GAME_CLOSED_PROTOTYPES_ENABLED`, the `*-closed-prototypes` presets, the project-specific builders + probes + `tools/roblox_fishing/` + `tools/playtest/`, `src/main.c` + `src/game_state_actions.*`, the closed generated sources in `src/generated/`, `assets/runtime/roblox-fishing-ui-v1` + `rune-marches-v1`, `state/closed_prototypes_game_state.schema.json`; updated AGENTS.md; clean-seed `native-debug` configures + builds and `pipeline_validate` passes.

## Open questions

- LEAD: keep the opt-in closed-prototype runtime+tooling as history (AGENTS current), or drop it from the template entirely? This is the gate; nothing is removed until you choose.

## Log

- 2026-06-15: Captured from the second simplification/speed iteration. On execution found the builders are wired into the intentionally-kept opt-in closed-prototype build (CMakeLists.txt + GAME_CLOSED_PROTOTYPES_ENABLED). Re-scoped from "P3 cleanup" to a lead direction decision; NOT executed (would break the kept build + reverse AGENTS). Surfaced to lead.
- 2026-06-15: LEAD approved DROP. Executed: removed `GAME_CLOSED_PROTOTYPES_ENABLED` + both closed codegen blocks + the `*-closed-prototypes` presets, deleted `src/main.c` (~2637 LOC), `src/game_state_actions.c/.h` (~1057 LOC), the closed generated sources in `src/generated/` (~785k LOC of committed art bytes), the rune/roblox builders + `tools/roblox_fishing/` + `tools/playtest/`, `assets/runtime/{roblox-fishing-ui-v1,rune-marches-v1}`, and `state/closed_prototypes_game_state.schema.json`. Updated AGENTS.md and the state-codegen unittest (clean-variant only). Clean-seed `native-debug` configures + builds `game_seed.exe`; state codegen + unittest + pipeline_validate pass. Not committed. Orphaned `assets/runtime/rune-marches-ui-*` dirs flagged to lead.
