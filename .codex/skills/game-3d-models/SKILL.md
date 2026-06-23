---
name: game-3d-models
description: "Use when adding, converting, sourcing, or rendering 3D models/meshes for a game: glb/gltf/obj/fbx, furniture, props, characters, model packs (Kenney, Quaternius, Poly Pizza), or replacing debug shape-renderer/baked geometry with real meshes. New games load real meshes via the engine, not the debug renderer. Source first; convert to glb; dedup shared textures."
---

# Game 3D Models

Real meshes for product visuals — not the debug shape renderer. The debug
`nt_shape_renderer` and geometry baked into a C header are debug debt only.

## Source First

Search before you create: `node tools/assets/source/find_assets.mjs --kind model`.
Free glTF/glb model sources: Quaternius, Kenney, Poly Pizza (all CC0). Generate a
model only if nothing fits. Catalog reusable ones in the shared library with
license + `origin`, then copy project-local (see game-asset-pipeline).

## Convert to glb

`tools/assets/obj_to_glb.py` (Blender headless) converts obj(+mtl)/fbx → glb:

    "<blender>" --background --python tools/assets/obj_to_glb.py -- <outdir> a.obj b.obj

glb is self-contained; the engine + `<model-viewer>` browser preview both read it.

## Texture dedup (important)

A model pack often shares ONE atlas/colormap across all models. Keep it ONCE:
prefer glTF-separate (one external texture referenced by every model) or a single
shared texture — do NOT embed a copy per model (GLB embeds textures, so N models
become N copies). Color-only packs (mtl `Kd` colors, no `map_Kd`) have no textures
— glb is fine, no dedup needed (check `map_Kd` in the .mtl / image markers in glb).

## Load real meshes (engine path)

Do NOT use `nt_shape_renderer` or bake vertices into a header for product builds.
Real path: build the glb into the engine mesh pack (engine builder
`tools/builder/nt_builder_mesh`), load at runtime with `nt_gfx_activate_mesh(data,
size)`, draw via `nt_mesh_renderer` (+ `nt_mesh_comp` for entities). Canonical
example: `external/neotolis-engine/examples/sponza`. Engine is read-only; use its
public API + cgltf (already a dep).

## Report

State source (reused/sourced/generated), glb paths, texture-dedup decision,
runtime load path (mesh pack + nt_mesh_renderer), and a screenshot of real meshes.
