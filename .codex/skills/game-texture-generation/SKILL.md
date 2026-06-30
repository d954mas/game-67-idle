---
name: game-texture-generation
description: Use when sourcing, generating, downloading, adapting, or reviewing standalone game material textures for world surfaces, ground, terrain, props, assets, armor parts, metal, plastic, painted surfaces, panels, emissive details, decals, normal/roughness maps, stylized studs/baseplate surfaces, stock textures, marketplace/downloaded assets, and texture provenance/licensing decisions. Source first: search the shared library and free CC0/OFL sources before generating. Do not use for atlases, trim sheets, icon sheets, or UI atlases; use nt-asset-workflow for those asset workflows, storage, licensing, and project handoff.
---

# Game Texture Generation

Use before creating/sourcing standalone material textures for world surfaces,
props, assets, armor, metal, plastic, paint, panels, emissive details, decals,
and stylized studs/baseplates.

## Source First (before generating)

Search before you create. Run
`node ai_studio/assets/storage/search.mjs --kind texture --query <need>` (or
`--kind material`): reuse a library hit, or search free CC0/OFL sources
(ambientCG, Poly Haven) and intake one. Generate only what you cannot source,
and record the source decision in the task or art job.

## Workflow

1. Define usage class: tileable material, asset material, painted metal
   material, one-off decal, material map set, or procedural proof.
2. Decide tiling before generation/download: repeat, clamp, repeat-x,
   repeat-y, or unique non-tileable.
3. Record source route and provenance: generated, downloaded, stock,
   marketplace, hand-authored, or procedural.
4. For repeat textures, create a 2x2 preview and seam audit with
   `ai_studio/assets/prep/textures/audit_tileable_texture.py`.
5. Verify scale, seams, mips/zoom, style fit, and game-use evidence before
   claiming integration.
6. Use `nt-asset-workflow` for storage, license/provenance, previews, and
   project handoff after the source is stable.

If the work is an atlas, sprite sheet, icon sheet, UI atlas, UV atlas, or trim
sheet, stop and use `nt-asset-workflow`.

## References

- Read `references/texture-workflow.md` for usage classes, stylized studs,
  downloaded asset rules, and required checks.
- Read `references/texture-brief-template.md` when creating a texture brief or
  recording a generated/downloaded source.
