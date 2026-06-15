---
id: T0063
title: Retire archived-prototype tooling when prototypes are formally closed
status: backlog
epic: E003
priority: P3
tags: [subtraction, tooling, cleanup]
created: 2026-06-15
updated: 2026-06-15
---

## What

Tooling that only serves the closed Rune Marches / Splash Rods prototypes still
ships in the base: `tools/playtest/rune_marches_probe.py` +
`roblox_fishing_probe.py` (~254 LOC, no live caller), and the project-specific
builders `tools/assets/build_rune_marches_assets.py` /
`build_roblox_fishing_ui_assets.py` (~2,900 LOC opt-in). They are historical, not
reusable base infrastructure.

## Done when

- [ ] Lead confirms the prototypes are formally closed (STATUS already says fishing is closed and rune-marches is legacy).
- [ ] The project-specific probes + builders are removed from the reusable base (or moved to an archive/ignored location), keeping durable evidence under `gamedesign/projects/<id>/`.
- [ ] CMake / `src/main.c` closed-prototype opt-in still builds (or is cleanly removed too); `node tools/pipeline_validate.mjs` passes.

## Open questions

- Delete outright vs move to an archive folder? Lean: delete (git history preserves them); they are project-specific, not reusable.
- Bundle with retiring the `GAME_CLOSED_PROTOTYPES_ENABLED` runtime path in src/main.c, or keep that separate?

## Log

- 2026-06-15: Captured from the second simplification/speed iteration. MED risk (touches CMake + src/main.c); do when the lead is ready to formally drop the closed prototypes from the base.
