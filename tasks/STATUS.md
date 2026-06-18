# Project Status

## Current Goal

Iterate `Backrooms Liminal` (backrooms-liminal) toward a beautiful native 3D
liminal horror game with a distinctive non-Euclidean hook: rooms can be bigger
inside than outside, copy player marks, and require spatial/object reasoning to
escape. Current focus is T0010: make the impossible-room/portal proof universal
enough to grow into arbitrary levels instead of another one-off shader trick.

## Blocking Work

- No native runtime blocker is known for the current game repo slice.
- T0010 product gate is still red: the portal room now has data-driven
  geometry, material, light, finish, authored construction descriptors, a
  fully opaque fullscreen portal-room composite inside the aperture, denser
  texture-backed/material-kind-lit native `nt_gfx` room surfaces, reduced
  external ghost-frame artifacts, a stronger fixture-driven portal-room light
  model, a separate non-blended `nt_gfx` solid-shell pass, and a 256x256
  project source material atlas for wall/carpet/ceiling/trim sampling that the
  runtime loads from `assets/backrooms-liminal/materials/portal_material_atlas.ppm`,
  but not yet production-quality realistic Backrooms room construction.
- T0011 tracks an engine-facing dependency for true fast multi-pass portal
  rendering: public `nt_gfx` render-target/framebuffer support. The game repo
  must not patch `external/neotolis-engine`; use public APIs or carry an
  evidence-backed engine task.

## Non-blocking Debt

- Current profiling scope is usable for normal review:
  `T0010/opaque-surface-promotion`.
- T0001-T0008 are in review with historical evidence. Do not expand them unless
  the lead asks; current actionable work is T0009/T0010 plus the T0011 engine
  issue.

## Current Gate

Current native gate for backrooms-liminal: `data/core_loop.json`,
`gamedesign/projects/backrooms-liminal/portal_memory_loop_direction.md`,
native build, DevAPI smoke, T0010 portal-memory scenario,
`build/captures/backrooms_t0010_impossible_geometry.png`, readability zoom,
strict product gate
`gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md`,
and taskboard validation.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
py -3.12 tools/assets/build_backrooms_liminal_materials.py
cmake --build --preset native-debug --target game_seed
py -3.12 tmp/capture_backrooms_t0010_portal_memory.py
py -3.12 tools/devapi/smoke.py
py -3.12 tools/devapi/ui_readability.py build\captures\backrooms_t0010_impossible_geometry.png
node tools/taskboard/cli.mjs validate
```

## Last Known Good Evidence

- `src/backrooms_portal_scene.*` defines the current game-local universal portal
  scene foundation: rooms, material/light/finish/authored-construction
  descriptors, portal descriptors, flags, validation, and GPU params.
- `src/clean_seed_main.c` composites the impossible room as an opaque fullscreen
  portal cut, then draws a separate native `nt_gfx` room pass. The current
  status JSON proves 744 portal overlay vertices: 294 non-blended solid-shell
  vertices, 450 blended detail vertices, material-kind shading, per-surface
  portal lighting, nested back-wall frame/fixture geometry, and copied mark
  feedback.
- `tools/assets/build_backrooms_liminal_materials.py` builds the current
  Backrooms material source asset atomically into
  `assets/backrooms-liminal/materials/portal_material_atlas.ppm` plus
  `portal_material_atlas.json`. This is an iteration source asset with explicit
  `procedural_source_asset` provenance, not final generated/artist material art.
- `tasks/active/T0011-engine-render-target-api-for-portal-rendering.md` records
  the engine-facing render-target API gap with evidence from `nt_gfx`.
- `build/captures/backrooms_t0010_portal_memory_status.json` proves mark
  placement, locked-door rejection, handle pickup, handle fitting, exit reveal,
  escape, and active `portal_render` material/light/finish/construction params
  including material asset evidence:
  `native_overlay.material_source =
  asset_ppm_backrooms_material_atlas_wall_carpet_ceiling_trim`,
  `native_overlay.material_atlas_loaded_from_asset = true`, and
  `native_overlay.material_asset_path =
  assets/backrooms-liminal/materials/portal_material_atlas.ppm`.
- `build/captures/backrooms_t0010_impossible_geometry.png` is the latest native
  proof screenshot for the data-driven impossible room.
- `build/captures/backrooms_t0010_impossible_geometry_uizoom.png` is the latest
  readability montage; readability passed but must still be eyeballed.
- `gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md`
  is the latest strict desktop product gate and is FAIL for art quality and
  audience fit.

## Next Priorities

1. Replace the current procedural PPM source material atlas with generated or
   artist-authored Backrooms material source assets, or unblock T0011
   render-target portal lighting; revisit the product gate for art quality and
   audience fit after the room construction itself is no longer a hybrid proxy.
2. Avoid more one-pass shader or shell decoration unless it directly proves the
   future mesh/material/render-target contract.
3. Keep content expansion frozen while the T0010 product gate remains red,
   unless the lead explicitly accepts that visual debt.
