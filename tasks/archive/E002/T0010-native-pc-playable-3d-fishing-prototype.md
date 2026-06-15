---
id: T0010
title: Native PC playable 3D fishing prototype
status: done
epic: E002
priority: P1
tags: [native, 3d, prototype, gameplay, visuals]
created: 2026-06-15
updated: 2026-06-15
---

## What

Implement the first native PC playable 3D fishing prototype after the reference
and GDD gates are ready. The prototype should show a readable 3D fishing loop
with bright visuals and screenshot/input proof.

## Done when

- [x] Native build starts as the primary harness; no web prototype is used
      without explicit lead approval.
- [x] Player can cast, wait for bite, play a simple reel minigame, catch a fish,
      receive reward feedback, sell/collect, and buy at least one upgrade.
- [x] Scene shows 3D island/water/dock/avatar/fishing rod/fish or equivalent
      runtime assets with juicy feedback.
- [x] DevAPI or equivalent automation can drive the first loop and capture
      screenshot evidence.
- [x] Profiling records slow/failing commands and repeated friction.

## Open questions

- Which first-slice scope is accepted by the lead after the GDD gate?

## Log

- 2026-06-15: Implemented first native fishing loop in state/actions:
  cast -> bite -> reel -> catch -> sell -> Better Line upgrade.
- 2026-06-15: Added `tools/playtest/roblox_fishing_probe.py`; it drives the
  native DevAPI loop, validates catch/sell/upgrade, and captures
  `tmp/roblox_fishing/native_first_slice.png`.
- 2026-06-15: First procedural 2D/flat visual pass failed lead review: not
  enough 3D, not generated-art aligned, and not close enough to fake shot.
- 2026-06-15: Replaced the runtime scene with a perspective/depth 3D shape
  renderer scene: water plane, dock, island/gate, boat, blocky avatar, rod,
  bobber, jumping fish, bubbles, and coin burst.
- 2026-06-15: Generated and saved reusable UI/icon source sheet at
  `gamedesign/projects/roblox-fishing/art/source_sheets/splash-rods-ui-icons-source-v1.png`.
  Runtime UI crop/codegen is still the next visual asset step.
- 2026-06-15: Re-generated/normalized the UI sheet as
  `gamedesign/projects/roblox-fishing/art/source_sheets/splash-rods-ui-icons-source-v2-magenta-clean.png`,
  cropped it into `assets/runtime/roblox-fishing-ui-v1/`, and wired the native
  HUD, buttons, catch card, reel meter, and icons through generated texture
  assets.
- 2026-06-15: Fixed generated UI draw-order friction: shape-renderer shadow
  rects were batched after texture draws and hid the PNG colors. Removed those
  procedural overlays and rebalanced desktop controls. Final native proof:
  `tmp/roblox_fishing/native_first_slice.png`.
- 2026-06-15: Verified the engine does support GLB mesh packs through
  `nt_builder_add_mesh`, `nt_resource`, and `nt_mesh_renderer`. Added
  `tools/roblox_fishing/build_packs.c`, CMake pack generation, and a minimal
  `roblox_fishing_models.ntpack` runtime proof using the engine `cube.glb` as
  GLB-backed scene props. This proves the model path; final fishing/location
  model selection remains a separate art pass.
- 2026-06-15: Improved the native 3D screen against the fake shot: denser dock
  details, water hotspot/ripples, palm/flower/rock/shop/boat/goal props,
  clearer blocky avatar face/hair/shirt, extra fish silhouettes, and stronger
  reward/splash FX. Latest proof remains
  `tmp/roblox_fishing/native_first_slice.png`.
- 2026-06-15: Replaced the cube-only proof with generated low-poly GLTF source
  meshes for fish trophy, toy boat hull, shop sign, palm leaf chunk, and bobber
  diamond under `gamedesign/projects/roblox-fishing/art/models/`. The native
  pack builder now emits 6 mesh assets into `roblox_fishing_models.ntpack`.
- 2026-06-15: Fixed native pack loading under the DevAPI root cwd by compiling
  the absolute native pack path into the game target. The playtest probe now
  gates on `glb_props_ready`, `mesh_instances >= 16`, and
  `mesh_draw_groups >= 6`, proving the GLTF/GLB model path renders during the
  captured gameplay screenshot.
- 2026-06-15: product gate FAIL (desktop); review: gamedesign/projects/roblox-fishing/reviews/product_read_gate_2026-06-15T09-20-35-536Z_desktop.md; screenshot: tmp/roblox_fishing/native_first_slice.png; next: Freeze feature expansion; run a visual rescue pass from fake shot to art bible, separate UI/world asset families, and require a new native screenshot gate before claiming visual progress.
- 2026-06-15: Closed by lead direction as a completed playable/profiling test
  iteration despite the red visual product gate. The failure is preserved as
  pipeline evidence; no further fishing prototype product work will be done.
- 2026-06-15: Closed by lead direction as completed playable/profiling prototype evidence; red product gate retained for pipeline review, not product continuation.
