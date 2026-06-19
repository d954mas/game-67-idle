# Project Status

## Current Goal

Iterate `Backrooms Liminal` (backrooms-liminal) toward a beautiful native 3D
liminal horror game with a distinctive non-Euclidean hook: rooms can be bigger
inside than outside, copy player marks, and require spatial/object reasoning to
escape. Current focus is T0010: make the impossible-room/portal proof universal
enough to grow into arbitrary levels instead of another one-off shader trick.

## Blocking Work

- No native runtime blocker is known for the current game repo slice.
- T0010 product gate is still red. The portal proof is now scene-driven and
  asset-backed, with generated material sampling and a larger native `nt_gfx`
  solid/detail portal-room layer. It still does not reach production-quality
  Backrooms construction because the portal interior remains a hybrid
  matte/composite plus native overlay.
- T0011 / https://github.com/d954mas/neotolis-engine/issues/238 tracks an
  engine-facing dependency for true fast multi-pass portal rendering: public
  `nt_gfx` render-target/framebuffer support. The game repo must not patch
  `external/neotolis-engine`; use public APIs or carry an evidence-backed
  engine task.

## Non-blocking Debt

- AI profiling scope is set to `T0010/opaque-native-dominant-room`, but the
  current profile guard reported unresolved failed records during the
  performance pass. Do not use AI profile output as runtime performance
  evidence until the profile is cleaned up or re-imported.
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
native debug performance gate
`gamedesign/projects/backrooms-liminal/reviews/perf_gate_latest.md`, and
taskboard validation.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
py -3.12 tools/assets/build_backrooms_liminal_materials.py
cmake --build --preset native-debug --target game_seed
py -3.12 tmp/capture_backrooms_t0010_portal_memory.py
py -3.12 tools/devapi/smoke.py
py -3.12 tools/devapi/ui_readability.py build\captures\backrooms_t0010_impossible_geometry.png
py -3.12 tools/perf/backrooms_perf_gate.py
node tools/taskboard/cli.mjs validate
```

## Last Known Good Evidence

- `src/backrooms_portal_scene.*` defines the current game-local universal portal
  scene foundation: rooms, material/light/finish/authored-construction
  descriptors, portal descriptors, flags, validation, and GPU params.
- `src/clean_seed_main.c` composites a dimmed portal backing, then draws a
  separate native room pass. The latest status JSON proves 1386 portal overlay
  vertices: 936 non-blended solid-shell vertices and 450 blended detail
  vertices.
- `tools/assets/build_backrooms_liminal_materials.py` builds the current
  Backrooms material source asset atomically into
  `assets/backrooms-liminal/materials/portal_material_atlas.ppm` plus
  `portal_material_atlas.json` from
  `gamedesign/projects/backrooms-liminal/art/source/portal_material_source_sheet_v1.png`.
  The source has prompt/workflow/generation records under the active project art
  folder.
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
- `build/captures/backrooms_t0010_gameplay.mp4` is the latest native gameplay
  video proof: 18.0s, 1280x720, H.264, 30fps.
- `build/captures/backrooms_t0010_impossible_geometry_uizoom.png` is the latest
  readability montage; readability passed but must still be eyeballed.
- `gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md`
  is the latest strict desktop product gate and is FAIL for art quality and
  audience fit.
- `gamedesign/projects/backrooms-liminal/reviews/perf_gate_latest.md` is the
  latest native debug performance gate and currently passes, including
  1386/1450 portal overlay vertices and mouse-look yaw delta 0.396.

## Next Priorities

1. Stop using more shell decoration as the main improvement path; implement a
   more complete opaque native portal-room draw path or unblock T0011
   render-target-backed portal lighting before expanding content.
2. Keep one-pass shader changes limited to alignment with the real
   mesh/material/render-target contract.
3. Keep content expansion frozen while the T0010 product gate remains red,
   unless the lead explicitly accepts that visual debt.
