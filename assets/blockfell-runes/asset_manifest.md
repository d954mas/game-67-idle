---
game_id: blockfell-runes
type: authored-asset-manifest
created: 2026-06-20
license: project-original
---

# Blockfell Runes Authored Asset Kit

These meshes are project-original authored geometry for the first playable
slice. They are not third-party downloads and do not copy protected characters,
names, or models.

Runtime source:

- `src/blockfell_authored_assets.h`
- `src/blockfell_texture_assets.h`

Imported source textures:

- `assets/blockfell-runes/source/textures/polyhaven_brown_mud_leaves_01/brown_mud_leaves_01_diff_1k.jpg`
  - source: https://polyhaven.com/a/brown_mud_leaves_01
  - license: CC0, https://polyhaven.com/license
  - provenance record: shared asset library
    `polyhaven__brown-mud-leaves-01-diffuse__cc0`
  - sha256:
    `c598c555f13cd5328409fd5e2a45496a26e319e52299e3959042f1449c2f1b06`
  - runtime derivative: `BF_TEX_GRASS_CC0` in
    `src/blockfell_texture_assets.h`, resized to 64x64 RGBA for the native
    material pass

Included first-pass meshes:

- hero faceted body
- hero head
- hero faceted cuirass
- hero cape panel
- hero helmet crest
- hero sword blade
- enemy faceted body
- enemy head
- enemy mask
- enemy horns
- rune spire
- rune glyph face
- gate keystone
- chest lock plate
- chest body
- chest lid
- camp standard
- camp dais
- camp canopy
- pine crown
- pine trunk
- mountain body
- mountain snow cap
- rock shard
- ruin trim
- path stone

Current limitation: the texture import is one real CC0 diffuse ground source,
not a full PBR material set. These are code-authored project-original meshes,
not an imported DCC/model-pack pipeline. The character center of the combat
view now uses authored meshes for bodies/heads/weapons, and the first-view
tree, mountain, camp, and chest silhouettes render through the authored mesh
pass. Some rune details, gate bars, small props, and VFX still use procedural
runtime geometry.
