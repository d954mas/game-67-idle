# 3D Models

Load this when sourcing, converting, pulling, or integrating GLB/GLTF/OBJ/FBX
models, meshes, props, furniture, characters, or model packs.

## Source First

Search Backlog Asset Storage before downloading or generating:

```powershell
node ai_studio/assets/backlog/storage/search.mjs --kind model --query "<need>" --json
```

If a library asset fits, pull it into the game-local asset root:

```powershell
node ai_studio/assets/viewer/pull.mjs --ids <asset-id> --to <game>/assets --apply
```

Only source from free/public sources or generate when the library has no fit.
Store newly accepted models back through Backlog Asset Storage so future games can reuse
them.

## Conversion

Prefer vendor-shipped `.glb` when available. Convert OBJ/MTL/FBX only when the
source license and material data are clear:

```powershell
py -3.12 ai_studio/assets/tools/conversion/obj_to_glb.py ...
```

Record the source file, converted output, license, author/source, and any
normal/material compromises in the manifest or intake notes.

## Texture Dedup

Many model packs share one atlas or material set. Keep shared textures once per
pack when possible. Do not embed a duplicate texture into every model unless
the source already ships that way and the conversion would be riskier than the
duplication.

Color-only models with material colors and no texture map can use a neutral
white texture plus per-instance or material color.

## Engine Pack And Load

Game code uses game-local files, not the global library path.

At build time, pack meshes through the game's pack builder with a stream layout
matching the selected mesh shader. The current template demonstrates both:

- untextured/per-instance-color mesh path;
- textured mesh path with UVs and `u_texture`.

For a single mesh, pack with `nt_builder_add_mesh`. For multi-primitive or
multi-material GLB files, inspect the engine scene import path before claiming
the model is integrated. Keep one runtime resource per primitive/material when
needed.

At runtime, request mesh/shader/texture resources, create the matching material,
create drawable entities, and verify with a real screenshot or viewer capture.

Minimum engine integration checklist:

- Register resource activators for meshes and textures.
- Initialize entity, transform, mesh, material, drawable, and mesh renderer
  systems before drawing.
- Request resource ids from the generated asset header, not string literals in
  gameplay code.
- Use material attribute maps that match the packed stream layout, usually
  position at stream 0 and uv0 at stream 1 for the template mesh shaders.
- For color-only models, bind the neutral texture and set per-instance/material
  color from the source material data.
- On graphics context restore, invalidate mesh/texture resources and restore the
  mesh renderer GPU state.

Do not claim a model is integrated from pack success alone. The acceptance
evidence is a visible model in the game/viewer path that will ship.

## Evidence

Report:

- source route: reused, sourced, generated, or converted;
- asset id and game-local path;
- license/provenance state;
- conversion or texture-dedup decision;
- runtime/preview evidence that the real mesh renders.
