# Shadow Cascades

## Purpose

The math of a directional sun's cascaded shadow maps, and of culling against
them. Pure C, no GPU: the game owns the depth atlas, the passes and the shaders.

- `csm_compute` fits each cascade to the sphere around its slice of the view and
  snaps the cascade's centre to whole shadow texels in a fixed light basis, so a
  moving or turning camera never shimmers shadow edges. Radii round up to half
  a metre, so float noise cannot flip a cascade's texel size.
- `frustum_*` builds normalized clip planes from a view-projection and tests
  boxes against them. The light variant drops the near plane, because casters
  between the light and its depth window still cast.
- `shadow_volume_*` extrudes a camera slice toward the sun into the region whose
  casters can reach it, tests boxes against it and bounds it inside a scene box.

The game keeps the knobs: cascade count, splits, resolution, depth range, sun
direction and `margin_texels` (the PCF kernel reach plus one texel for snapping).

## Public surface

`include/features/shadow_cascades/csm.h` and `frustum.h`. Matrices are
column-major, GL clip space.

## Layer

L0: pure C over `<math.h>` and the engine's `nt_math` (cglm). No GPU or save dependency.

## Validation

`test_csm` and `test_frustum` (CTest label `core`) in the feature consumer, and
`node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. PATCH changes internals without
changing results beyond tolerance; MINOR adds API or config fields with a
backward-compatible default; MAJOR renames or removes a function or field, or
changes the cascade layout.

## Extension points

A game with another filter changes `margin_texels`; a game with another cascade
layout reads `tile`, `viewport` and `lookup_*` from `csm_cascades_t` instead of
assuming the atlas grid.
