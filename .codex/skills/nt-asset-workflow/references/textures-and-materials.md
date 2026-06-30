# Textures And Materials

Load this when sourcing, generating, downloading, adapting, or reviewing
standalone textures, material maps, tileable surfaces, decals, or model-specific
material textures.

Use `generated-assets-and-cutouts.md` instead for UI atlases, icon sheets,
source-sheet crop/cutout work, or slice9 UI art.

## Source First

Search Asset Storage before generating:

```powershell
node ai_studio/assets/storage/search.mjs --kind texture --query "<need>" --json
node ai_studio/assets/storage/search.mjs --kind material --query "<need>" --json
```

If no library asset fits, use practical free sources such as CC0/OFL texture
libraries or generate a new source, then record the route through Asset Storage.

## Usage Class

Decide the usage class before accepting the texture:

- tileable material: floors, walls, terrain, roads, panels, fabric, water;
- asset material: prop, weapon, armor, rubber, glass, energy core, paint;
- one-off decal: scorch mark, label, poster, face plate, logo-free mark;
- material map set: albedo, normal, roughness, metalness, emissive, alpha;
- procedural proof: temporary validation source, not a final asset claim.

Do not assume model textures must be tileable. State the UV assumption when a
texture is meant for a model.

## Required Checks

- Tiling: inspect borders and a 2x2 repeat preview for repeated surfaces.
- Scale: check that grain, studs, bolts, scratches, or motifs read at gameplay
  camera distance.
- Mips/zoom: check that minified texture does not shimmer, disappear, or become
  muddy.
- Style fit: compare against current art direction or player-facing screenshot.
- Runtime path: confirm the packer/loader supports the required texture type
  before claiming integration.
- Provenance: keep source URL, author, license, prompt/edit recipe, or no-seed
  reason near the asset record.

For repeated textures, run the tile audit helper when useful:

```powershell
py -3.12 ai_studio/assets/prep/textures/audit_tileable_texture.py --source <texture.png> --preview <texture_2x2.png> --json-output <audit.json> --report <audit.md>
```

Numeric seam checks catch obvious edge mismatches. A visual 2x2 preview is still
needed because a technically seamless texture can be visually repetitive, noisy,
or wrong scale.

## Downloaded Assets

Download availability is not permission to ship. Record:

- URL and author/store/source name;
- license and allowed use, including commercial and modification rights;
- whether attribution is required;
- whether redistribution inside this public repository/build is allowed;
- local source path and runtime output path;
- edits made: resize, crop, seam fix, color grade, normal generation, packing.

If the license is unclear, keep it as non-shipping reference or replace it with
a usable source.

## Minimum Source Record

For generated or downloaded textures, keep at least:

- texture name and project/game;
- usage class and runtime target path;
- source path and source route;
- wrap mode and whether seamless tiling is required;
- expected world scale or UV assumption;
- source URL/tool, author/source name, license, and attribution requirement;
- prompt or edit recipe when generated/adapted;
- map roles when using albedo/normal/roughness/metalness/emissive/alpha;
- seam audit, 2x2 preview, runtime screenshot, or reason a check is not
  applicable.
