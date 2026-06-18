---
id: T0010
title: Portal memory marking and object-placement spike
status: doing
epic: E001
priority: P1
tags: [prototype, backrooms-liminal, portal-rendering, marking, object-puzzle, native-first]
created: 2026-06-18
updated: 2026-06-18
---

## What

Turn the new differentiator into a narrow native spike: Backrooms Liminal is not
just another maze, it is a place that is bigger inside than outside and can copy
or corrupt the player's evidence. Prove one impossible-room/portal moment where
the player can mark a surface, find an object, place it at the correct landmark,
and reveal a real exit.

## Done when

- [x] Project direction doc exists and names the hook, core loop, and visual
      target for portal rooms, player marks, and object placement.
- [x] Native runtime shows one readable impossible-room/portal composition:
      the room reads larger/different inside than the approach suggests.
- [x] Player can place at least one visible mark on a wall or floor surface.
- [x] Player can pick up one mundane object and place it on one valid target.
- [x] Correct placement reveals or stabilizes a real exit; wrong/missing
      placement does not.
- [x] DevAPI scenario proves mark placement, pickup, placement, exit reveal,
      and escape.
- [ ] Screenshot/product gate judges whether the portal-room proof looks
      high-quality and distinctive, not like a debug shader trick.

## Open questions

- Should the first mark be freehand drawing, a stamped symbol, or both?
- Which object best communicates the rule: key, breaker handle, room plate, or
  mundane prop such as a chair/sign?
- Should the first portal be real recursive rendering or a staged one-portal
  illusion backed by explicit room state?

## Log

- 2026-06-18: Lead proposed the differentiator: portal-rendered impossible
  rooms, loops, drawing on walls/floors, finding keys/items, placing them on
  correct spots, and escaping by understanding the space. Direction captured in
  `gamedesign/projects/backrooms-liminal/portal_memory_loop_direction.md`.
- 2026-06-18: First native test scope narrowed to impossible rooms, closed
  doors, drawing marks, and one missing door handle. Ladders/keys remain in the
  object puzzle vocabulary but are deferred until this proof works.
- 2026-06-18: Native T0010 mechanics now pass through DevAPI:
  `build/captures/backrooms_t0010_portal_memory_status.json` proves mark
  placement, locked door rejection without the handle, handle pickup, handle
  fitting, exit reveal, and escape. Visual gate remains red in
  `gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md`;
  the impossible geometry reads too flat and needs a stronger non-Euclidean
  render pass before this task can close.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Replace the flat impossible-cut illusion with a stronger non-Euclidean render pass: nested room planes, convincing wall thickness, shadowed door depth, and authored room landmarks.
- 2026-06-18: Non-Euclidean render pass replaced the flat wall overlay with a
  secondary ray-box room visible through the architectural cut. The updated
  screenshot `build/captures/backrooms_t0010_impossible_geometry.png` now reads
  as a deeper room that should not fit, but product gate remains red for
  material, lighting, bevel/contact-shadow, and UI plate quality.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Do a dedicated visual pass on production materials and lighting: stronger wallpaper/carpet material depth, fluorescent shadows, beveled wall thickness, grime, and a more physical locked-door/room landmark.
- 2026-06-18: Material/lighting HUD pass added dirtier wallpaper generation,
  damp wall/carpet variation, darker impossible-room corners, localized
  fluorescent depth cues, and moved the minimal journal into the visible UI
  texture. Readability improved, but product gate remains red because the
  result is still shader-authored rather than production-quality realistic
  Backrooms geometry/materials.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Move the visual pass from shader-only polish to stronger authored geometry/material treatment: real bevel/thickness, better fluorescent shadowing, stronger carpet/wall material breakup, and less temporary prompt styling.
- 2026-06-18: Bevel/contact-shadow pass added stronger rim lighting, darker
  inner occlusion around the impossible wall cut, and softer prompt panels. The
  cut now reads more like thick architecture, but product gate remains red until
  geometry/material quality stops reading as one-file shader work.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Move beyond one-file shader tricks: introduce or emulate authored wall thickness, better carpet/wall material layers, stronger fluorescent shadowing, and a cleaner prompt treatment.
- 2026-06-18: Universalization pass added a game-local
  `BackroomsPortalScene` foundation (`src/backrooms_portal_scene.*`) with room,
  portal, flag, and GPU-parameter descriptors. T0010 now drives the impossible
  room from scene data instead of only hardcoded shader constants. Native build,
  T0010 DevAPI scenario, smoke, readability, and taskboard validation passed.
  Product gate remains FAIL: the portal reads more like a physical larger room,
  but the visual bar still needs richer materials/lighting and, for true
  reusable multi-pass portals, engine render-target support tracked by T0011.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Build on the portal-scene foundation with reusable room/material descriptors, stronger lighting/material quality, and T0011 render-target support for true multi-pass portal views.
- 2026-06-18: Portal material/light pass added `BackroomsPortalMaterial` and
  GPU material/light vectors so portal rooms can drive wall-panel scale, carpet
  tile scale, grime, wetness, fluorescent width/intensity, corner shadow, and
  baseboard strength from scene data. `game.state` now exposes a compact
  `portal_render` block proving the runtime uses these values. Native build,
  T0010 scenario, smoke, readability, taskboard validation, and refreshed
  product gate ran; product gate remains FAIL because the image is still not
  production-quality realistic Backrooms art.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Use the portal material descriptors for a stronger visual pass: authored-looking trim, light fixtures, shadow gradients, roughness-like wallpaper/carpet response, and eventually T0011 render-target support.
- 2026-06-18: Portal finish pass extended `BackroomsPortalMaterial` and GPU
  params with trim strength, fixture spacing, ceiling-panel scale, and shadow
  spill. The native shader now uses those room descriptors for visible
  fluorescent fixture cues, ceiling panel seams, wall battens/cove trim, floor
  light pools, and softer side-opening darkness. Native build, T0010 scenario,
  smoke, readability, taskboard, and slice hygiene ran; product gate remains
  FAIL because the screenshot is improved but still not a production-quality
  realistic Backrooms room. Profiler guard is advisory/red due unresolved failed
  records.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Move beyond the current one-pass finish layer toward real authored room construction or T0011 render-target support, instead of stacking more fullscreen shader polish.
- 2026-06-18: Authored construction pass added
  `BackroomsPortalConstruction` and GPU construction params for jamb depth,
  threshold lip, conduit strength, and landmark column strength. The native
  screenshot now gets visible threshold/fixture/room-construction cues from
  room descriptors, and `build/captures/backrooms_t0010_portal_memory_status.json`
  proves those fields in `portal_render`. Native build, T0010 scenario, smoke,
  readability, and taskboard validation passed. Product gate remains FAIL:
  this is a better bridge toward real renderable room layers, not the final
  production-quality Backrooms image.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Move construction descriptors into actual renderable mesh/material or render-target-backed room layers, and avoid spending more cycles on cosmetic one-pass shader decoration unless it proves that contract.
- 2026-06-18: Native portal overlay pass added a separate `nt_gfx` geometry
  layer on top of the portal shader. The pass streams 66 world-space vertices
  for jambs, threshold lip, inner fixture, conduit, and landmark column from
  the portal scene params; `game.state.portal_render.native_overlay` exposes
  the active path and vertex count. Native build, T0010 DevAPI scenario, smoke,
  readability, and refreshed product gate ran. Product gate remains FAIL: this
  proves a real render layer but is still proxy geometry, not production-quality
  realistic Backrooms construction/material lighting.
- 2026-06-18: Portal overlay expanded into a texture-backed room mesh/material
  layer. The native pass now streams 288 vertices total, including 222 vertices
  for inner floor panels, side walls, back wall, ceiling panels, and light
  spill sampled through the generated wall texture. Native build, T0010
  scenario, smoke, readability, profiler guard, taskboard validation, and
  refreshed product gate ran. Product gate remains FAIL: this is a stronger
  authored room-layer proof, but it still blends like an overlay rather than a
  fully constructed realistic Backrooms room with convincing lighting.
