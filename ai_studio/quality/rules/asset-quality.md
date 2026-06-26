# Asset Quality Rule

Apply when adding, moving, generating, converting, sourcing, or claiming assets
as ready.

## Required Evidence

- license;
- provenance and source path/page;
- publishability or restricted routing;
- integrity where available;
- runtime-ready path and format;
- visual/material proof when the asset is visible in-game.

## 3D Material Floor

For active 3D games that claim sourced or ready GLB/GLTF models, prove source
materials, textures, UVs, per-primitive colors, or equivalent material data.
Flat color-only rendering is not a ready-asset proof.

```powershell
node tools/product_gate/visual_material_floor.mjs
```

If the floor fails, stop content expansion and create a material/texture pass.
