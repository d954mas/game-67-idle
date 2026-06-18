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
  runtime material atlas for wall/carpet/ceiling/trim sampling, but not yet
  production-quality realistic Backrooms room construction.
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
- `src/clean_seed_main.c` now composites the impossible room as an opaque
  fullscreen portal cut, then draws a separate native `nt_gfx` room pass that
  streams 744 world-space vertices: 660 texture-backed room
  mesh/material-detail vertices for denser inner floor/wall/ceiling/light-spill
  surfaces, grout seams, wall seams, back-wall strips, ceiling grid, and shadow
  bands. The first 294 solid-shell vertices for floor panels, side-wall panels,
  back-wall panels, ceiling panels, soffit, center-rib surfaces, a nested
  back-wall frame, and fixture/light-box surfaces are drawn through a
  non-blended pipeline; the remaining 450 vertices draw blended seams, light
  spill, aperture occlusion, softened jamb/threshold hints, inner fixture,
  conduit, and landmark column from portal scene params. The portal overlay
  samples a runtime 256x256 wall/carpet/ceiling/trim material atlas rather than
  using one wallpaper noise source for every surface.
  The fullscreen portal room now uses per-surface normals, direct/bounce
  fluorescent lighting, side/back occlusion, fixture cast shadow, and wet-floor
  specular response. The overlay shader receives material kind and world
  position to apply center light spill, side shadow falloff, seam darkening,
  depth falloff, and softer occluder/contact-shadow treatment.
- `tasks/active/T0011-engine-render-target-api-for-portal-rendering.md` records
  the engine-facing render-target API gap with evidence from `nt_gfx`.
- `build/captures/backrooms_t0010_portal_memory_status.json` proves mark
  placement, locked-door rejection, handle pickup, handle fitting, exit reveal,
  escape, and active `portal_render` material/light/finish/construction params
  including trim, fixture spacing, ceiling panel scale, shadow spill, jamb
  depth, threshold lip, conduit, landmark columns,
  `native_overlay.last_vertex_count = 744`,
  `native_overlay.room_mesh_vertex_count = 660`,
  `native_overlay.solid_shell_vertex_count = 294`,
  `native_overlay.solid_pass_vertex_count = 294`,
  `native_overlay.blended_detail_vertex_count = 450`, and
  `native_overlay.material_source =
  runtime_backrooms_material_atlas_wall_carpet_ceiling_trim`.
- `build/captures/backrooms_t0010_impossible_geometry.png` is the latest native
  proof screenshot for the data-driven impossible room.
- `build/captures/backrooms_t0010_impossible_geometry_uizoom.png` is the latest
  readability montage; readability passed but must still be eyeballed.
- `gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md`
  is the latest strict desktop product gate and is FAIL for art quality and
  audience fit.

## Next Priorities

1. Turn the current runtime material-atlas proof into real source/runtime
   material assets, or unblock T0011 render-target portal lighting; revisit the
   product gate for art quality and audience fit after the room construction
   itself is no longer a hybrid proxy.
2. Avoid more one-pass shader or shell decoration unless it directly proves the
   future mesh/material/render-target contract.
3. Keep content expansion frozen while the T0010 product gate remains red,
   unless the lead explicitly accepts that visual debt.
