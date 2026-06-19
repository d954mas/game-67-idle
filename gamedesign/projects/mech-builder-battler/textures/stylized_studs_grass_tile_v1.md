# Stylized Studs Grass Tile V1

## Identity

- Texture name: `stylized_studs_grass_tile_v1`
- Project/game: `Mech Builder Battler`
- Usage class: tileable material
- Runtime target path:
  `assets/textures/mech_builder_battler_stylized_studs_grass_tile_v1.png`
- Source path:
  `assets/source/textures/mech-builder-battler/stylized_studs_grass_tile_v1.png`
- 2x2 seam preview:
  `gamedesign/projects/mech-builder-battler/art/texture_previews/stylized_studs_grass_tile_v1_2x2.png`

## Tiling Decision

- Wrap mode: repeat
- Must be seamless: yes
- Expected world scale: one 1024 px source tile represents a playful baseplate
  material repeated across large grass/studs surfaces.
- Camera distance where it must read: native three-quarter gameplay camera at
  1280x720.
- Seam validation method: 2x2 grid preview plus native screenshot comparison.

## Source

- Source route: generated/procedural
- URL or tool: `tools/mech-builder-battler/generate_stylized_studs_texture.py`
- Author/store/source name: project-authored generator
- License status: project-owned generated source
- Attribution required: no
- Can ship in repository/build: yes
- Prompt or edit recipe: saturated stylized grass tile with repeated leaf/grass
  motifs, semi-visible studs at roughly 50% visual strength, and erased/gapped
  studs where motifs need to read.

## Maps

- Albedo: `assets/textures/mech_builder_battler_stylized_studs_grass_tile_v1.png`
- Normal: deferred
- Roughness: deferred
- Metalness: not applicable
- Emissive: not applicable
- Alpha/mask: not applicable

## Runtime Integration Note

The current native floor renderer is shape-based, not textured mesh-based. T0033
therefore integrates the visual language directly in runtime floor geometry
while also saving the tileable PNG source/runtime candidate and seam preview for
the future textured-material path.

## Acceptance

- Borders are clean for repeat wrapping in the 2x2 preview.
- Stud density and motif gaps match the lead's stylized-studs direction.
- Motifs remain visible at gameplay camera height.
- Runtime screenshot uses the same visual language without overwhelming the
  mech silhouette.
- Provenance is enough to regenerate or replace the texture later.
