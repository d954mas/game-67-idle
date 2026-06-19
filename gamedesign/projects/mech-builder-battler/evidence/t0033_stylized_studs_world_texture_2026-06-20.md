# T0033 Stylized Studs World Texture Pass

## Result

The Roblox-like world surface now follows the lead's stylized-studs texture
direction more closely: denser semi-visible studs, grass/leaf motif gaps, and a
saved tileable texture source with a 2x2 seam preview.

## What Changed

- Added a project-authored texture generator:
  `tools/mech-builder-battler/generate_stylized_studs_texture.py`.
- Generated source texture:
  `assets/source/textures/mech-builder-battler/stylized_studs_grass_tile_v1.png`.
- Generated runtime candidate texture:
  `assets/textures/mech_builder_battler_stylized_studs_grass_tile_v1.png`.
- Generated 2x2 seam preview:
  `gamedesign/projects/mech-builder-battler/art/texture_previews/stylized_studs_grass_tile_v1_2x2.png`.
- Added texture brief/provenance:
  `gamedesign/projects/mech-builder-battler/textures/stylized_studs_grass_tile_v1.md`.
- Updated native floor shape layers to match the texture direction: denser
  studs, softer stud highlights, motif shadow panels, and explicit gaps around
  motifs.
- Added T0033 hangar and battle screenshots to the DevAPI smoke.

## Screenshot Evidence

- Hangar world texture read:
  `build/captures/mech_t0033_stylized_studs_world_hangar_smoke.png`
- Battle world texture read:
  `build/captures/mech_t0033_stylized_studs_world_battle_smoke.png`
- Texture seam preview:
  `gamedesign/projects/mech-builder-battler/art/texture_previews/stylized_studs_grass_tile_v1_2x2.png`
- Product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T01-42-10_desktop-stylized-studs-world-texture.md`

## Texture Contract

- Usage class: tileable material.
- Wrap mode: repeat.
- Must be seamless: yes.
- Source route: project-authored generated/procedural.
- Atlas/trim sheet: out of scope for this pass.
- Current runtime note: the native floor path is shape-based, so T0033 matches
  the generated texture language in runtime geometry while preserving the PNG
  for a future textured mesh/material path.

## Validation

```powershell
py -3.12 tools\mech-builder-battler\generate_stylized_studs_texture.py
cmake --build --preset native-debug --target game_seed
py -3.12 tools\mech-builder-battler\devapi_playable_smoke.py 9124
node tools\taskboard\cli.mjs validate
node tools\product_gate\review.mjs --project mech-builder-battler --task T0033 --surface desktop-stylized-studs-world-texture --screenshot build\captures\mech_t0033_stylized_studs_world_hangar_smoke.png --verdict pass --strict --visual-strict ...
```

## Review Notes

- Strict visual gate passed with all rubric scores at 4.
- Remaining debt: final material polish should move from shape-layer recreation
  to true textured mesh/material integration with optional normal/roughness maps.
