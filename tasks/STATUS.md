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
  geometry, material, light, finish, authored construction descriptors, denser
  texture-backed/material-kind-lit native `nt_gfx` room surfaces, and a
  separate aperture contact-shadow layer, but not yet production-quality
  realistic Backrooms room construction.
- T0011 tracks an engine-facing dependency for true fast multi-pass portal
  rendering: public `nt_gfx` render-target/framebuffer support. The game repo
  must not patch `external/neotolis-engine`; use public APIs or carry an
  evidence-backed engine task.

## Non-blocking Debt

- Global AI profile review confidence is still broken by older unresolved
  failed records. Current profiling evidence is advisory until those records are
  explained or repaired.
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
- `src/clean_seed_main.c` now has a separate native `nt_gfx` portal overlay
  pass that streams 450 world-space vertices: 366 texture-backed room
  mesh/material-detail vertices for denser inner floor/wall/ceiling/light-spill
  surfaces, grout seams, wall seams, back-wall strips, ceiling grid, and shadow
  bands plus a separate aperture contact-shadow layer, jambs, threshold, inner
  fixture, conduit, and landmark column from portal scene params. The overlay
  shader now receives material kind and world position to apply center light
  spill, side shadow falloff, seam darkening, depth falloff, and softer
  occluder/contact-shadow treatment.
- `tasks/active/T0011-engine-render-target-api-for-portal-rendering.md` records
  the engine-facing render-target API gap with evidence from `nt_gfx`.
- `build/captures/backrooms_t0010_portal_memory_status.json` proves mark
  placement, locked-door rejection, handle pickup, handle fitting, exit reveal,
  escape, and active `portal_render` material/light/finish/construction params
  including trim, fixture spacing, ceiling panel scale, shadow spill, jamb
  depth, threshold lip, conduit, landmark columns,
  `native_overlay.last_vertex_count = 450`, and
  `native_overlay.room_mesh_vertex_count = 366`.
- `build/captures/backrooms_t0010_impossible_geometry.png` is the latest native
  proof screenshot for the data-driven impossible room.
- `build/captures/backrooms_t0010_impossible_geometry_uizoom.png` is the latest
  readability montage; readability passed but must still be eyeballed.
- `gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md`
  is the latest strict desktop product gate and is FAIL for art quality and
  audience fit.

## Next Priorities

1. Move from the current denser blended overlay proof to opaque authored 3D
   room surfaces or T0011 render-target portal rendering, then revisit the
   product gate for art quality and audience fit.
2. Add stronger production texture/light evidence before expanding content;
   more one-pass shader decoration is now a low-value path unless it directly
   proves the future mesh/material contract.
3. Keep content expansion frozen while the T0010 product gate remains red,
   unless the lead explicitly accepts that visual debt.
