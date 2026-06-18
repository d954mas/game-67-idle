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
  geometry, material, light, and finish descriptors, but not yet
  production-quality realistic Backrooms room construction.
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
  scene foundation: rooms, material/light/finish descriptors, portal
  descriptors, flags, validation, and GPU params.
- `tasks/active/T0011-engine-render-target-api-for-portal-rendering.md` records
  the engine-facing render-target API gap with evidence from `nt_gfx`.
- `build/captures/backrooms_t0010_portal_memory_status.json` proves mark
  placement, locked-door rejection, handle pickup, handle fitting, exit reveal,
  escape, and active `portal_render` material/light/finish params including
  trim, fixture spacing, ceiling panel scale, and shadow spill.
- `build/captures/backrooms_t0010_impossible_geometry.png` is the latest native
  proof screenshot for the data-driven impossible room.
- `build/captures/backrooms_t0010_impossible_geometry_uizoom.png` is the latest
  readability montage; readability passed but must still be eyeballed.
- `gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md`
  is the latest strict desktop product gate and is FAIL for art quality and
  audience fit.

## Next Priorities

1. Improve T0010 visual quality beyond the current one-pass finish layer:
   use more real authored room construction where the game repo can, and move
   the reusable portal path toward T0011 render-target support instead of
   stacking more fullscreen shader tricks.
2. Decide whether to pursue T0011 in the engine repo now or prototype a
   game-local mesh/material room layer that can later feed the render target
   pipeline.
3. Keep content expansion frozen while the T0010 product gate remains red,
   unless the lead explicitly accepts that visual debt.
