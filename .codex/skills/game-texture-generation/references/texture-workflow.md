# Texture Workflow

## Usage Classes

- **Tileable material**: floors, walls, terrain, baseplates, metal panels, roads,
  fabric, water, lava. Must be seamless if repeated in world space.
- **Asset material**: standalone texture for props, weapons, mech armor, joints,
  plastic blocks, painted metal, rubber, glass, energy cores, or emissive strips.
  Tiling depends on UVs and mesh scale; do not assume it must be seamless.
- **Mech material**: armor paint, panel noise, edge wear, warning stripes, vents,
  cockpit glass, emissive core, dark joint material, or weapon metal. Decide
  whether it is a reusable material or a unique part texture.
- **One-off decal**: logo-free iconography, scorch marks, stickers, posters,
  hazard labels, character face plates. Usually clamp; do not force seamlessness.
- **Material map set**: albedo plus normal/roughness/metalness/emissive. Keep map
  scale and UV assumptions consistent across the set.
- **Procedural proof**: runtime-drawn placeholder for validating style and scale.
  Mark it as proof, not final asset, unless the game will intentionally ship it.

## Stylized Studs Texture

Use this for Roblox-like block-world surfaces without copying Roblox branding or
specific protected assets.

1. Generate or source the base stylized texture first: grass, metal, slime, snow,
   sand, candy, sci-fi paneling, etc.
2. Make the base seamless only if it will repeat on large surfaces.
3. Overlay a studs grid as a secondary relief/pattern layer.
4. Break or erase studs where silhouettes, leaf shapes, cracks, paint marks, or
   motifs need to read.
5. Keep studs about 40-60% visual strength for playful surfaces, stronger only
   for explicit baseplate blocks.
6. Check the texture in perspective at gameplay camera height, not only flat.

## Downloaded Assets

For internet/stock/marketplace assets, do not treat download availability as
permission to ship. Record:

- URL and author/store/source name.
- License and allowed use, including commercial and modification rights.
- Whether attribution is required.
- Whether redistribution inside the repository/build is allowed.
- Local source path and runtime output path.
- Any edits made: resize, crop, seam fix, color grade, normal generation, packing.

If the license is unclear, use the asset only as a non-shipping reference and
create a replacement source.

## Required Checks

- **Tiling**: inspect all borders; offset-preview or repeat a 2x2 grid for world
  materials.
- **Scale**: verify studs, grain, leaves, bolts, scratches, or fabric weave read
  at gameplay camera distance.
- **Mips/zoom**: ensure the texture does not shimmer, disappear, or become muddy
  when minified.
- **Style fit**: compare against the current art direction and screenshots.
- **Runtime path**: confirm engine packer/loader supports the required texture
  type before promising it as implemented.
- **Provenance**: keep generation prompt, source URL, no-seed reason, or license
  note near the project art/source record.
