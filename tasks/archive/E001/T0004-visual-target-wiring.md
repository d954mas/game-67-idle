---
id: T0004
title: Visual target wiring
status: done
epic: E001
priority: P1
tags: [fantasy-pocket-rpg, art, ui]
created: 2026-06-11
updated: 2026-06-11
---

## What

Runtime scene resembles the accepted fake shots enough for playtest.

## Done when

- [x] ruins/camp backgrounds load
- [x] UI is readable
- [x] no blank frame
- [x] mobile portrait layout is not broken

## Open questions

## Log

- 2026-06-11: Seeded from implementation_tasks.json phase list.
- 2026-06-11: Read `gamedesing/fantasy-pocket-rpg/data/asset_manifest.json`: accepted ruins/camp PNGs exist for the visual GDD, but `runtime_ready` is false. Next step is a real runtime asset path for `art/fake-shot-ruins-background.png` and `art/camp-preparation-background.png` via pack/texture loading, then desktop + portrait visual QA.
- 2026-06-11: Added build-generated RGBA runtime backgrounds and native textured-quad rendering for ruins/camp. Evidence: `build/captures/fantasy_slice_runtime_bg_ruins.png`, `build/captures/fantasy_slice_runtime_bg_full_loop.png`, `build/captures/portrait_layout/portrait_combat.png`, `build/captures/portrait_layout/portrait_camp.png`.
