---
id: T0108
title: "M1 follow-up: real textured meshes for sims/furniture/room (replace shape-renderer debug art)"
status: todo
epic: E007
priority: P2
tags: [little-lives, art, meshes]
created: 2026-06-22
updated: 2026-06-22
---

## What

Replace shape-renderer debug art with real textured 3D meshes for Sims,
furniture, and the room/house.

## Done when

- [ ] Sims render as real character meshes (lit, textured).
- [ ] Furniture + room use real meshes/materials via the ntpack mesh pipeline.
- [ ] Product visual gate passes for art_quality with the new assets.

## Open questions

- Source of CC0 character + furniture models? Only space-corridor + robot CC0
  meshes are vendored today (`assets/meshes/`), which do not fit a cozy home.

## Log

- Interim (this pass): upgraded the procedural Sims from a capsule+sphere to a
  readable blocky humanoid (legs/torso/arms/head/hair with a walk swing) in
  `draw_sim` — much more "little person", but still shape-renderer art (debug
  debt remains). Evidence: `gamedesign/projects/little-lives/reviews/sims_people.png`.
- BLOCKER for real meshes: no suitable CC0 art on hand. The image pipeline makes
  raster art, not 3D meshes; character/furniture GLBs must be sourced (e.g.
  Kenney Furniture Kit + Mini Characters, CC0) — moved to **T0111**.
- Mesh pipeline is mapped and ready: builder `nt_builder_add_mesh` +
  `nt_builder_parse_glb_scene` -> ntpack; runtime `nt_mesh_renderer_draw_list`
  with a lit material (Blinn-Phong lighting UBO). Reference:
  `external/neotolis-engine/examples/{textured_quad,sponza}`. The text pack
  pipeline + frame UBO (added in T0107) already prove ntpack/resource wiring.
- Next: T0111 sources the models, then this task swaps draw_object/draw_sim to
  mesh draws and adds a lit material.
- UPDATE: furniture now renders as **real CC0 Kenney meshes** (bed/fridge/shower/
  toilet/sofa/desk) via the shape-renderer mesh path (T0111) — real silhouettes,
  flat-tinted. Remaining for this task: Sim character meshes + a textured/lit
  pipeline (PBR look). Evidence: reviews/furniture_meshes.png.
