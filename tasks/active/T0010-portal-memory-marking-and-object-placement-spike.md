---
id: T0010
title: Portal memory marking and object-placement spike
status: doing
epic: E001
priority: P1
tags: [prototype, backrooms-liminal, portal-rendering, marking, object-puzzle, native-first]
created: 2026-06-18
updated: 2026-06-19
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
- 2026-06-18: Authored material-detail pass added thin native geometry for
  floor grout, wall seams, back-wall strips, ceiling grid, stronger shadow
  bands, and concentrated fixture spill. The portal overlay now streams 432
  vertices total, including 366 texture-backed room mesh/material-detail
  vertices. Native build, T0010 scenario, smoke, readability, profiler guard,
  taskboard validation, and refreshed product gate ran. Product gate remains
  FAIL: the room reads more constructed, but the render still needs proper 3D
  surface integration and stronger physically convincing light/shadow response.
- 2026-06-18: Material-kind lighting pass extended the native overlay vertex
  contract with a material kind and world-position shading. The overlay shader
  now applies different alpha, center light spill, side shadow falloff, seam
  darkening, and depth falloff for room surfaces, seams, and light strips.
  Native build, T0010 scenario, smoke, readability, profiler guard, taskboard
  validation, and refreshed product gate ran. Product gate remains FAIL: the
  portal reads more solid/scary, but it is still an overlay-lit proof rather
  than fully integrated 3D room surfaces or render-target portal lighting.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Move the portal room from blended overlay proof toward opaque authored interior surfaces or T0011 render-target rendering; remove the remaining ghosted frame artifacts and improve realistic lighting/material response
- 2026-06-19: Authored-room depth slice added a separate aperture-occluder
  material kind and denser alpha/shadow treatment for native `nt_gfx` portal
  room surfaces. The T0010 scenario now reports
  `native_overlay.last_vertex_count = 450` and
  `native_overlay.room_mesh_vertex_count = 366`. Native build, T0010 capture,
  smoke, readability, profiler guard, taskboard validation, strict product
  gate, and slice hygiene ran. Product gate remains FAIL for art quality and
  audience fit: the image is more solid, but still a blended overlay proof that
  needs opaque authored room surfaces or T0011 render-target portal rendering.
- 2026-06-19: Opaque aperture pass moved the impossible-room composite to full
  opacity inside the fullscreen portal cut, narrowed/dimmed the shader frame,
  and reduced the external overlay jamb/threshold alpha so the portal no longer
  reads as strongly as transparent side plates over the wall. Native build,
  T0010 capture, smoke, readability, taskboard validation, and strict product
  gate ran. Product gate remains FAIL for art quality/audience fit: the cleaner
  aperture is a stopgap, not a replacement for opaque authored interior
  geometry or T0011 render-target portal lighting.
- 2026-06-19: Physical-lighting pass added per-surface normals inside the
  fullscreen portal room, direct/bounce fluorescent lighting, side/back
  occlusion, fixture cast shadow, and wet-floor specular response. Native
  build, T0010 capture, smoke, readability, taskboard validation, and strict
  product gate ran. Product gate remains FAIL for art quality/audience fit: the
  room is scarier and less flat, but still needs real opaque authored interior
  geometry or T0011 render-target portal lighting instead of more cosmetic
  overlay decoration.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Use the solid-shell overlay as the bridge, then either build the portal interior as real opaque geometry in the native pass or unblock T0011 render-target portal lighting; avoid adding more cosmetic shell plates
- 2026-06-19: Authored solid-shell bridge layer added a new native overlay
  material kind for denser interior floor, side-wall, back-wall, ceiling,
  soffit, and center-rib planes. The T0010 scenario now reports
  `native_overlay.last_vertex_count = 492`,
  `native_overlay.room_mesh_vertex_count = 408`, and
  `native_overlay.solid_shell_vertex_count = 42`. Native build, T0010 capture,
  smoke, readability, profiler scope, taskboard validation, and strict product
  gate ran. Product gate remains FAIL for art quality/audience fit: this is a
  better bridge toward real opaque geometry, not the final render-target or
  production-room solution.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Promote more portal room surfaces from fullscreen composite into the native opaque pass, or unblock T0011 render-target portal lighting; do not expand gameplay content while art_quality and audience_fit remain 3
- 2026-06-19: Opaque native portal pass split the portal room layer into a
  non-blended `nt_gfx` solid-shell draw followed by blended detail draw. The
  T0010 scenario now reports `native_overlay.solid_pass_vertex_count = 42` and
  `native_overlay.blended_detail_vertex_count = 450` under
  `native_nt_gfx_solid_shell_plus_blended_detail_layer`. Native build, T0010
  capture, smoke, readability, profiler scope, taskboard validation, and strict
  product gate ran. Product gate remains FAIL for art quality/audience fit:
  this reduces the transparent-overlay feel, but the portal is still a hybrid
  fullscreen composite plus native surfaces rather than a fully native opaque
  interior or render-target portal.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Keep promoting the portal interior toward fully native opaque geometry/materials or unblock T0011 render-target portal lighting; do not expand gameplay content while art_quality and audience_fit remain 3
- 2026-06-19: Opaque surface promotion moved the main floor, side-wall,
  back-wall, and ceiling panel quads into the non-blended native pass. The
  T0010 scenario now reports `native_overlay.last_vertex_count = 702`,
  `native_overlay.room_mesh_vertex_count = 618`,
  `native_overlay.solid_pass_vertex_count = 252`, and
  `native_overlay.blended_detail_vertex_count = 450`. Native build, T0010
  capture, smoke, readability, profiler scope, taskboard validation, and strict
  product gate ran. Product gate remains FAIL for art quality/audience fit: the
  portal interior is more physically native, but still needs production material
  assets and fully integrated lighting/depth or T0011 render-target support.
- 2026-06-19: Runtime material-atlas slice replaced the single wallpaper noise
  source with a 256x256 wall/carpet/ceiling/trim material atlas and made the
  native portal overlay shader choose the material by surface. The solid pass
  also gained a native nested back-wall frame and fixture/light-box geometry so
  the aperture reads more like a constructed impossible room. The T0010
  scenario now reports `native_overlay.last_vertex_count = 744`,
  `native_overlay.room_mesh_vertex_count = 660`,
  `native_overlay.solid_pass_vertex_count = 294`,
  `native_overlay.blended_detail_vertex_count = 450`, and
  `material_source = runtime_backrooms_material_atlas_wall_carpet_ceiling_trim`.
  Native build, T0010 capture, smoke, readability, profiler scope, taskboard
  validation, and strict product gate ran. Product gate remains FAIL for art
  quality/audience fit: this is a stronger runtime material/native-geometry
  bridge, but the game still needs real source assets and integrated
  render-target portal lighting before this task can close.
- 2026-06-19: Asset-backed material atlas slice added
  `tools/assets/build_backrooms_liminal_materials.py`,
  `assets/backrooms-liminal/materials/portal_material_atlas.ppm`, and
  `portal_material_atlas.json` so the portal material atlas now has a
  game-local source/runtime asset boundary instead of living only in runtime C
  generation. The runtime keeps the procedural C fallback, loads the PPM when
  present, and `build/captures/backrooms_t0010_portal_memory_status.json`
  reports `native_overlay.material_source =
  asset_ppm_backrooms_material_atlas_wall_carpet_ceiling_trim`,
  `material_atlas_loaded_from_asset = true`, and
  `material_asset_path =
  assets/backrooms-liminal/materials/portal_material_atlas.ppm`. Native build,
  T0010 capture, smoke, readability, profiler scope, taskboard validation, and
  strict product gate ran. Product gate remains FAIL for art quality/audience
  fit: this proves the reusable material asset contract, but the source is
  still procedural and must be replaced by generated or artist-authored
  Backrooms materials, or by T0011 render-target portal lighting, before T0010
  can close.
- 2026-06-19: Generated material source slice accepted
  `gamedesign/projects/backrooms-liminal/art/source/portal_material_source_sheet_v1.png`
  as the first source sheet for wallpaper, carpet, ceiling tile, and aged trim.
  The builder now crops that generated source into the runtime PPM atlas and
  records `generated_source_asset` provenance plus art job, prompt, workflow,
  and generation-record paths. The main portal shader also samples all four
  generated material regions for the fullscreen portal room, not only the
  native overlay. Native build, T0010 capture, smoke, readability, taskboard
  validation, and strict product gate ran. Product gate remains FAIL for art
  quality/audience fit: material identity is stronger and no longer procedural,
  but the aperture still reads too dark/flat and needs integrated lighting,
  side-wall construction, and depth or T0011 render-target portal lighting.
- 2026-06-19: Integrated light/depth slice added stronger center/floor light
  spill, side-wall bounce, and denser native solid-shell wall returns/soffit/
  threshold geometry while reducing the external ghost-frame alpha. The T0010
  scenario now reports `native_overlay.last_vertex_count = 792`,
  `native_overlay.room_mesh_vertex_count = 708`,
  `native_overlay.solid_pass_vertex_count = 342`, and
  `native_overlay.blended_detail_vertex_count = 450`. Native build, T0010
  capture, smoke, readability, profiler scope, taskboard validation, and strict
  product gate ran. Product gate remains FAIL for art quality/audience fit: the
  portal entrance is cleaner and more physical, but still reads as a hybrid
  fullscreen composite plus native shell rather than production-quality
  render-target or fully native 3D portal-room rendering.
- 2026-06-19: Native opaque-interior slice added opaque side-wall ribs,
  back-wall rails, ceiling light strips, floor light pools, and corrected the
  native light quads to use the existing light material kind instead of an
  unlit shell kind. The T0010 scenario now reports
  `native_overlay.last_vertex_count = 888`,
  `native_overlay.room_mesh_vertex_count = 804`,
  `native_overlay.solid_pass_vertex_count = 438`, and
  `native_overlay.blended_detail_vertex_count = 450`. Native build, T0010
  capture, smoke, readability, profiler scope, taskboard validation, and strict
  product gate ran. Product gate remains FAIL for art quality/audience fit:
  geometry density and light handling improved, but this path is reaching the
  limit of shell decoration; next work should be a more complete opaque native
  portal-room draw path or T0011 render-target-backed portal lighting.
- 2026-06-19: Native-matte responsibility slice reduced the fullscreen
  impossible-room shader from the main portal image into a darker matte/backing
  layer, then raised the native `nt_gfx` portal shader's material brightness,
  light spill, shell opacity floor, and depth/side-shadow readability. This is
  a render-path alignment step rather than new content: the goal is for the
  native room layer to carry more of the room construction before T0011
  render-target support exists. The T0010 scenario still reports
  `native_overlay.last_vertex_count = 888`,
  `native_overlay.room_mesh_vertex_count = 804`,
  `native_overlay.solid_pass_vertex_count = 438`, and
  `native_overlay.blended_detail_vertex_count = 450`. Native build, material
  atlas rebuild, T0010 capture, smoke, readability zoom, taskboard validation,
  profiler-scope check, and strict product gate ran. Product gate remains FAIL
  for art quality/audience fit: the layer split is healthier, but the aperture
  is still a hybrid matte/composite plus native overlay, not yet a production
  opaque native room or render-target portal.
