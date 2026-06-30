---
name: game-3d-models
description: "Use when adding, converting, sourcing, or rendering 3D models/meshes for a game: glb/gltf/obj/fbx, furniture, props, characters, model packs (Kenney, Quaternius, Poly Pizza), or replacing debug shape-renderer/baked geometry with real meshes. New games load real meshes via the engine, not the debug renderer. Source first; convert to glb; dedup shared textures."
---

# Game 3D Models

Real meshes for product visuals вЂ” not the debug shape renderer (`nt_shape_renderer`
and C-baked geometry are debug debt only).

## Source first (reuse the library)

Engine-ready GLB models sit in the shared library, tagged by
genre/type. Reuse BEFORE sourcing or generating:
- discover: `node ai_studio/assets/storage/search.mjs --kind model --tags <t> --query <q>` (`--json` too).
- browse: `build_review.mjs --mode library --out tmp/lib-gallery --ref`, then `serve_gallery.mjs --gallery tmp/lib-gallery --lib <library>`.
- pull into the game: `ai_studio/assets/viewer/pull.mjs --ids <id> --to assets --apply` (no `--apply` = dry-run).

Only if nothing fits: source free glb (Quaternius/Kenney/Poly Pizza, CC0) or
generate, then store it through `nt-asset-workflow`/Asset Storage so the next game reuses it.

## Convert to glb

`ai_studio/assets/prep/conversion/obj_to_glb.py` (Blender headless): obj(+mtl)/fbx в†’ glb. Prefer a
vendor-shipped glb (obj winding can break normals). glb is self-contained.

## Texture dedup

A pack often shares ONE atlas across all models вЂ” keep it ONCE (glTF-separate or a
single shared texture), never embed a copy per model. Colour-only packs (mtl `Kd`,
no `map_Kd`) need no texture.

## Render a library glb in-engine

Pack-then-load вЂ” not `nt_shape_renderer` or baked headers. PACK the glb via a pack
builder (create `src/build_packs.c` from `examples/atlas/build_packs.c`, wired as a
CMake custom command) using the 2-stream `mesh_inst` layout + a 1x1 white texture;
LOAD at runtime via `nt_resource_request` + `nt_material_create` + `nt_mesh_renderer`.
Multi-primitive furniture needs the scene API (engine #248). Templates:
`examples/atlas` (single + load) and `examples/sponza` (multi-primitive scene).
Full recipe + exact API calls: `references/engine-render.md`.

## Report

Source (reused/sourced/generated), glb paths, texture-dedup decision, engine
integration path (mesh pack + nt_mesh_renderer), screenshot of real meshes.
