# Asset-First Mech/World Sourcing

Date: 2026-06-20

## Direction

Use ready permissively licensed models first for the next Roblox-like visual
slice. Procedural cubes are allowed only as connectors, mounts, collision
helpers, or temporary debug shapes.

## Current Candidate Read

| Candidate | URL | Source/author | License observed | Fit | Risk |
| --- | --- | --- | --- | --- | --- |
| Mech Assault Walker | https://poly.pizza/m/6s3_n8xzzvo | Poly Pizza / Alimayo Arango | Creative Commons Attribution | Already selected hero; strong silhouette, cockpit, twin guns, and walker read. | Attribution required; motion still kitbashed rather than authored rig. |
| Sentinel Mech | https://poly.pizza/m/aGSpAN8ONud | Poly Pizza / Tekano Bob | Creative Commons Attribution | Toy/figurine tags make it closer to Roblox-like block play than realistic hard sci-fi. | Selected for T0035 as a showroom/rival mech; still needs attribution UI before public release. |
| Giant Mech | https://poly.pizza/m/5l-n7PyvqOu | Poly Pizza / Chris Ross | Creative Commons Attribution | Bigger chunky robot candidate for boss/readable scale fantasy. | Needs download/import check and attribution; silhouette may be less modular. |
| Basic Mech | https://poly.pizza/m/6DWG0Xex7td | Poly Pizza / Mike Rezl | Creative Commons Attribution | Simple low-poly body can become a modular starter or NPC mech. | Likely less impressive than Assault Walker; use only if kitbashable. |
| Quaternius Animated Mech Pack | https://quaternius.com/ | Quaternius | License not visible on crawled homepage; verify in downloaded package before integration. | Homepage lists animated, textured mech/sci-fi robots; thumbnail direction is closer to the juicy mech expectation. | Do not ship until license file and redistribution rights are confirmed. |
| Quaternius Ultimate Space Kit | https://quaternius.com/ | Quaternius | License not visible on crawled homepage; verify in downloaded package before integration. | Space kit includes mechs/vehicles/platforms and can improve the arena/world set. | Potentially broad pack; must extract only the needed pieces. |
| Quaternius Cube World Kit | https://quaternius.com/ | Quaternius | License not visible on crawled homepage; verify in downloaded package before integration. | Strong block-world reference for Roblox-like world props and readable toy scale. | Not mech-specific; use for environment dressing only after license check. |
| Kenney 3D / Modular Space Kit / Blocky Characters | https://kenney.nl/assets/category:3D | Kenney | Kenney support states asset pages are Public Domain (CC0). | Good CC0 source for blocky/world props, UI-friendly silhouettes, and non-mech set dressing. | Less mech-specific; best for world kit pieces and placeholder-safe props. |

## Selected T0035 Asset

Sentinel Mech by Tekano Bob was selected as a second ready-made mech display
asset. The source page exposed the resource id
`40d7525e-237c-448b-8b1d-8260bd0b1885`, `OBJ/GLTF format`, tags
`robot`, `toy`, `figurine`, and `Creative Commons Attribution`.

- Source page: `https://poly.pizza/m/aGSpAN8ONud`
- Source GLB:
  `https://static.poly.pizza/40d7525e-237c-448b-8b1d-8260bd0b1885.glb`
- Preview:
  `https://static.poly.pizza/40d7525e-237c-448b-8b1d-8260bd0b1885.jpg`
- Author/source: Tekano Bob / Poly Pizza.
- License observed: Creative Commons Attribution / CC-BY 3.0.
- Attribution required: yes, before any public release.
- Local source GLB:
  `assets/source/models/poly_pizza/tekano_bob/poly_pizza_tekano_sentinel_mech_ccby30.glb`
- Local source preview:
  `assets/source/models/poly_pizza/tekano_bob/poly_pizza_tekano_sentinel_mech_ccby30_preview.jpg`
- Runtime extraction: 10 material-split static GLTF parts under
  `assets/meshes/poly_pizza_tekano_sentinel_mech_*_static_ccby30.gltf`.
- Runtime role: static side-pad showroom/rival mech in the hangar, not the
  playable hero and not final authored animation.

## Next Attempt

1. Try to acquire and inspect Quaternius Animated Mech Pack because it directly
   targets animated/textured mechs and may solve the current authored-animation
   weakness.
2. If license/download verification blocks it, use Poly Pizza `Giant Mech` as a
   boss/showcase candidate and keep Sentinel as the current second mech.
3. For world props, use Kenney CC0 3D packs before inventing more block props.

## Intake Rules

- Record URL, author, license, attribution, redistribution permission, source
  path, runtime path, and any conversion edits before packing.
- Confirm Y-up conversion and native scale at gameplay camera height.
- Reject unclear license, no redistribution rights, ripped Roblox content, or
  assets that only look good in preview but fail at gameplay size.
- Keep generated/procedural pieces as accents around the sourced model, not as
  the core mech body.
