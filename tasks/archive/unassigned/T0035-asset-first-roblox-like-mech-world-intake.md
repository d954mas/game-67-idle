---
id: T0035
title: Asset-first Roblox-like mech/world intake
status: done
priority: P0
tags: [visual, assets, mech, world, sourcing, native]
created: 2026-06-20
updated: 2026-06-20
---

# T0035 - Asset-First Roblox-Like Mech/World Intake

## Why

The lead explicitly rejected the cube-like mech direction and asked why we were
not trying harder to search for and use ready internet assets. The next visual
slice must therefore start from permissively licensed downloaded/source assets,
not from more hand-authored block placeholders.

## What

- In scope: shortlist usable mech/world asset sources, verify license and
  attribution requirements, download or reuse a concrete candidate, record
  provenance, and integrate one stronger visual asset path into the native PC
  harness.
- Out of scope: ripped Roblox assets, unclear-license marketplace files,
  economy/balance work, web/mobile export, or atlas/trim-sheet production.

## Done when

- [x] A sourcing note records at least three concrete mech/world candidates with
      URL, author/source, license, fit, and integration risk.
- [x] One selected asset has local source/provenance files, including license
      evidence and any required attribution.
- [x] The selected asset is converted or packed through the native asset path.
- [x] Native smoke captures before/after screenshots showing the stronger
      Roblox-like mech/world read.
- [x] Strict product gate records the visual result and remaining debt.
- [x] Taskboard validation passes.

## Activity Log

- 2026-06-20: Created after T0034. Priority is asset-first visual improvement:
  improve the mech/world with ready permissive models before adding more combat,
  economy, or procedural detail.
- 2026-06-20: Downloaded Poly Pizza `Sentinel Mech` by Tekano Bob from the
  official static source URLs, recorded CC-BY attribution requirement, and saved
  source GLB/preview under `assets/source/models/poly_pizza/tekano_bob/`.
- 2026-06-20: Converted Sentinel into 10 material-split GLTF parts, packed them
  into the native mesh pack, and rendered the model as a side-pad showroom/rival
  mech in the hangar.
- 2026-06-20: DevAPI smoke PASS captured
  `build/captures/mech_t0035_asset_first_sentinel_hangar_smoke.png`.
- 2026-06-20: product gate PASS (desktop-asset-first-sentinel-showcase); review:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T02-05-10_desktop-asset-first-sentinel-showcase.md`;
  evidence:
  `gamedesign/projects/mech-builder-battler/evidence/t0035_asset_first_sentinel_showcase_2026-06-20.md`.
- 2026-06-20: Post-prototype cleanup: archived as historical Mech Builder Battler work after the user stopped the game.
