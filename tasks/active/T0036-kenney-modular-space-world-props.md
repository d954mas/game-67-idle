---
id: T0036
title: Kenney modular space world props
status: review
priority: P0
tags: [visual, assets, world, props, cc0, native]
created: 2026-06-20
updated: 2026-06-20
---

# T0036 - Kenney Modular Space World Props

## Why

T0035 proved the asset-first mech path with a second sourced mech display. The
next visible gap is that the world is still mostly procedural block dressing.
The game needs real downloaded world props with clear licensing before adding
more procedural cubes.

## What

- In scope: use a small CC0 subset from Kenney `Modular Space Kit`, record
  license/provenance, pack selected sci-fi station pieces, render them in the
  native hangar/battle scene, and capture screenshot proof.
- Out of scope: full kit import, full level editor, animated prop playback,
  new combat/economy systems, or web/mobile export.

## Done when

- [x] Kenney Modular Space Kit provenance and local license/source files are
      recorded.
- [x] Selected source GLBs are copied into stable source/runtime paths.
- [x] Native pack contains the selected world prop meshes.
- [x] Runtime renders the sourced props in the world without blocking mech/UI
      readability.
- [x] DevAPI smoke captures a named T0036 screenshot.
- [x] Strict product gate records visual scores and remaining debt.
- [x] Taskboard validation passes.

## Activity Log

- 2026-06-20: Created after T0035 to keep the next visual pass asset-first and
  move the Roblox-like world away from mostly procedural block props.
- 2026-06-20: Imported selected CC0 Kenney Modular Space Kit gate/corridor/room
  GLBs, packed them into the native mesh pack, rendered them in the hangar, and
  captured `build/captures/mech_t0036_kenney_space_props_hangar_smoke.png`.
- 2026-06-19: product gate PASS (desktop-kenney-space-world-props); review: gamedesign\projects\mech-builder-battler\reviews\product_read_gate_2026-06-20T02-15-10_desktop-kenney-space-world-props.md; screenshot: build/captures/mech_t0036_kenney_space_props_hangar_smoke.png; next: continue to the next narrow slice
