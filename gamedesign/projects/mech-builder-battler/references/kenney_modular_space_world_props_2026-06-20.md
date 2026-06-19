# Kenney Modular Space World Props

Date: 2026-06-20

## Decision

Use a small CC0 subset from Kenney `Modular Space Kit` as the next downloaded
world-prop slice for the Roblox-like mech hangar. This keeps world visuals
asset-first instead of growing the arena from more procedural cubes.

## Provenance

- Source page: `https://kenney.nl/assets/modular-space-kit`
- Download archive:
  `https://kenney.nl/media/pages/assets/modular-space-kit/8261428a47-1771146076/kenney_modular-space-kit_1.0.zip`
- Author/source: Kenney.
- Package: `Modular Space Kit (1.0)`.
- Creation date in local license: `15-02-2026 09:59`.
- License in local package: Creative Commons Zero / CC0.
- Attribution required: no. Crediting `Kenney` or `www.kenney.nl` is optional.

## Local Assets

- Local license:
  `assets/source/models/kenney/modular_space_kit/License.txt`
- Local preview:
  `assets/source/models/kenney/modular_space_kit/kenney_modular_space_kit_preview.png`
- Selected source GLBs:
  - `assets/source/models/kenney/modular_space_kit/selected/kenney_modular_space_gate_cc0.glb`
  - `assets/source/models/kenney/modular_space_kit/selected/kenney_modular_space_corridor_wide_cc0.glb`
  - `assets/source/models/kenney/modular_space_kit/selected/kenney_modular_space_room_small_cc0.glb`
- Runtime GLBs:
  - `assets/meshes/kenney_modular_space_gate_cc0.glb`
  - `assets/meshes/kenney_modular_space_corridor_wide_cc0.glb`
  - `assets/meshes/kenney_modular_space_room_small_cc0.glb`

## Fit

- Roblox-like block/station shapes without copying Roblox-owned assets.
- CC0 license fits rapid prototype iteration and future redistribution.
- Useful for hangar framing, gates, portals, modular station dressing, and
  battle-arena landmarks around the mech.

## Integration Notes

- Keep native Y-up placement; selected meshes were positioned at floor contact
  using measured bounds before runtime scale.
- Render as sourced world props first. Do not convert these into atlas or trim
  sheets; standalone texture/material generation is a separate skill path.
- Use procedural/generated blocks only as connectors or temporary dressing
  around sourced props.
