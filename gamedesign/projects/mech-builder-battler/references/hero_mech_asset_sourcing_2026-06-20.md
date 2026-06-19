# Hero Mech Asset Sourcing - 2026-06-20

## Decision

Use `Mech Assault Walker` by Alimayo Arango from Poly Pizza as the current
hero mech source asset.

This is an asset-first visual correction. The previous hero was an improved
but still weak body read; this candidate has a readable cockpit, twin weapon
arms, chunky legs, and a toy-block silhouette that fits the Roblox-like target
better without using Roblox-owned assets.

## Selected Asset

- Source page: `https://poly.pizza/m/6s3_n8xzzvo`
- Source GLB: `https://static.poly.pizza/0f7c451a-e402-4429-b581-9d423de22c8b.glb`
- Preview: `https://static.poly.pizza/0f7c451a-e402-4429-b581-9d423de22c8b.jpg`
- Author: Alimayo Arango
- Displayed license: CC-BY 3.0
- Attribution requirement: required before shipping/public release.
- Local source GLB:
  `assets/source/models/poly_pizza/alimayo_arango/poly_pizza_alimayo_mech_assault_walker_ccby30.glb`
- Local source preview:
  `assets/source/models/poly_pizza/alimayo_arango/poly_pizza_alimayo_mech_assault_walker_ccby30_preview.jpg`
- Runtime extraction: material-split static GLTF files under `assets/meshes/`
  with `poly_pizza_alimayo_mech_assault_walker_*_static_ccby30.gltf`.

## Texture And Material Classification

- Texture class: unique source model material surfaces, not a standalone
  tileable world texture.
- Tiling: no tiling decision needed for the imported hero model because this
  runtime pass uses material-split geometry and authored per-part colors.
- Atlas/trim sheet: out of scope; that is a separate pipeline/skill.
- Runtime material pass: the current native renderer uses the extracted mesh
  parts with toy/plastic colors and solid lighting. A later polish pass can
  add authored metal/plastic textures or normal-like surface detail after the
  silhouette and animation read are accepted.

## Candidate Screening

| Candidate | Source | Displayed License | Decision |
| --- | --- | --- | --- |
| Mech Assault Walker by Alimayo Arango | `https://poly.pizza/m/6s3_n8xzzvo` | CC-BY 3.0 | Selected hero candidate. Strongest cockpit, weapon, and leg read. |
| Giant Mech by Chris Ross | `https://poly.pizza/m/5l-n7PyvqOu` | CC-BY 3.0 | Rejected for hero; boss/anime read and higher copy-risk silhouette. |
| Sentinel Mech by Tekano Bob | `https://poly.pizza/m/aGSpAN8ONud` | CC-BY 3.0 | Backup candidate; good silhouette but less immediate hero read. |
| Mech by Quaternius | `https://poly.pizza/m/D5wW2jDO42` | CC0 1.0 | Rejected for hero; reads more like a fox/vehicle than a strong mech. |
| Little Mech by Riley Florence | `https://poly.pizza/m/7ysTwVKOiMp` | CC-BY 3.0 | Rejected for hero; too small/placeholder. |
| Basic Mech by Mike Rezl | `https://poly.pizza/m/6DWG0Xex7td` | CC-BY 3.0 | Possible enemy or cheap unlock, not hero-quality. |
| Mechsuit by Pepper Media | `https://poly.pizza/m/cQFoSfuK7Tf` | CC-BY 3.0 | Secondary candidate; less direct than Assault Walker. |

## Usage Notes

- Do not use ripped Roblox assets or assets with unclear ownership.
- Keep source GLBs and previews in `assets/source/models/...` and record the
  visible license in this file or a follow-up provenance note.
- If another agent improves this pass, preserve the CC-BY attribution trail and
  update the final credits surface before any public build.
