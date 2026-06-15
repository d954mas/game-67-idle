---
id: T0063
title: Retire archived-prototype tooling when prototypes are formally closed
status: idea
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

## Done when (lead decision first)

- [ ] LEAD decides: keep the opt-in closed-prototype runtime as history (status quo, AGENTS as written) OR drop it entirely to make a cleaner template.
- [ ] If DROP: remove the closed-prototype CMake blocks, the project-specific builders + probes + `tools/roblox_fishing/`, the generated sources, `assets/runtime/*`, the `GAME_CLOSED_PROTOTYPES_ENABLED` path in `src/main.c`, and update AGENTS.md; verify the clean-seed build + `pipeline_validate` still pass.
- [ ] If KEEP: leave as-is (the tooling is part of the kept opt-in build); optionally still drop only the playtest probes if desired.

## Open questions

- LEAD: keep the opt-in closed-prototype runtime+tooling as history (AGENTS current), or drop it from the template entirely? This is the gate; nothing is removed until you choose.

## Log

- 2026-06-15: Captured from the second simplification/speed iteration. On execution found the builders are wired into the intentionally-kept opt-in closed-prototype build (CMakeLists.txt + GAME_CLOSED_PROTOTYPES_ENABLED). Re-scoped from "P3 cleanup" to a lead direction decision; NOT executed (would break the kept build + reverse AGENTS). Surfaced to lead.
