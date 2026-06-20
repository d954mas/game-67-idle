---
name: game-texture-generation
description: Use when generating, downloading, adapting, reviewing, or integrating standalone game material textures for world surfaces, ground, terrain, props, assets, armor parts, metal, plastic, painted surfaces, panels, emissive details, decals, normal/roughness maps, stylized studs/baseplate surfaces, stock textures, marketplace/downloaded assets, and texture provenance/licensing decisions. Do not use for atlases, trim sheets, icon sheets, or UI atlases; use the existing generated-game-ui-assets or asset pipeline workflows for those. Pair with game-asset-pipeline when packing or runtime-loading the texture.
---

# Game Texture Generation

Use before creating/sourcing standalone material textures for world surfaces,
props, assets, armor, metal, plastic, paint, panels, emissive details, decals,
and stylized studs/baseplates.

## Workflow

1. Define usage class: tileable material, asset material, painted metal
   material, one-off decal, material map set, or procedural proof.
2. Decide tiling before generation/download: repeat, clamp, repeat-x,
   repeat-y, or unique non-tileable.
3. Record source route and provenance: generated, downloaded, stock,
   marketplace, hand-authored, or procedural.
4. For repeat textures, create a 2x2 preview and seam audit with
   `tools/assets/intake/audit_tileable_texture.py`.
5. Verify scale, seams, mips/zoom, style fit, and runtime path before claiming
   integration.
6. Use `game-asset-pipeline` for pack/runtime integration after the source is
   stable.

If the work is an atlas, sprite sheet, icon sheet, UI atlas, UV atlas, or trim
sheet, stop: use UI/icon asset workflow or general asset pipeline.

## References

- Read `references/texture-workflow.md` for usage classes, stylized studs,
  downloaded asset rules, and required checks.
- Read `references/texture-brief-template.md` when creating a texture brief or
  recording a generated/downloaded source.
