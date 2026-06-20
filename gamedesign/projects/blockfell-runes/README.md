---
type: Project Wiki
game_id: blockfell-runes
title: Blockfell Runes
status: first-playable-slice
created: 2026-06-20
updated: 2026-06-20
---

# Blockfell Runes

Updated reference direction: offline isometric fantasy sandbox/ARPG readability
in the broad style lane of Albion Online, without copying its brand, UI, icons,
classes, economy, MMO model, or assets.

Native PC 3D block-fantasy action RPG slice from the user's request:
"3д скайрим в мире роблокса".

## First Slice

The first route is intentionally small but complete:

- explore one compact valley;
- claim an easy rune;
- clear a camp of blocky enemies;
- open a ruin chest for loot;
- claim combat-gated and loot-gated runes;
- open the mountain gate.

Runtime implementation: `src/clean_seed_main.c`.

Automation proof: `tools/devapi/smoke.py`.

Screenshot evidence: `build/captures/smoke.png`.

Combat/showcase evidence: `build/captures/blockfell_visual_pass.png`.

Material-pass evidence: `build/captures/blockfell_material_pass.png`.

Model-pass evidence: `build/captures/blockfell_model_pass.png`.

Quest/readability evidence: `build/captures/blockfell_quest_pass.png`.

Lighting/material-depth evidence:
`build/captures/blockfell_lighting_pass.png`.

Authored asset pass evidence:
`build/captures/blockfell_authored_asset_pass.png`.

Authored prop pass evidence:
`build/captures/blockfell_prop_asset_pass.png`.

Authored environment pass evidence:
`build/captures/blockfell_environment_asset_pass.png`.

Offline reference pass evidence:
`build/captures/blockfell_offline_reference_pass.png`.

Art cleanup pass evidence:
`build/captures/blockfell_art_cleanup_pass.png`.

## Current Debt

- Visuals are procedural shape-rendered models. This is acceptable for the first
  route proof but must be replaced or augmented with legal project-local assets
  before treating the art as accepted.
- HUD uses symbols and bars. Add engine-font labels once the first playable loop
  is stable.
- The 2026-06-20 visual pass adds shadow/glow/material cues but still does not
  satisfy final "models, textures, shadows, light" quality. It is the next
  technical step, not the final visual target.
- The 2026-06-20 material pass adds engine-GFX procedural raster textures on
  top of the shape layer. This improves current readability but is still not a
  substitute for accepted project-local 3D models and authored texture assets.
- The 2026-06-20 model pass improves silhouettes with faceted capes, helmets,
  horns, shields, weapons, and longer directional shadows. It is still a
  procedural model layer, not final authored 3D assets.
- The 2026-06-20 quest/readability pass adds a six-step HUD chain plus world
  objective markers, route line, camp ring, and chest glow so the first route is
  readable without adding new content.
- The 2026-06-20 lighting/material-depth pass adds normal-aware shader lighting
  and fog to the runtime material layer, denser procedural textures, local
  light pools, sun rays, and character rim/decal accents. It improves the
  shipped first-playable image but still remains procedural art.
- The 2026-06-20 authored asset pass adds a project-local mesh manifest and
  first runtime mesh overlays for hero armor/cape/crest and enemy masks/horns.
  This starts replacing placeholder visuals, but the base world and many props
  still need a full accepted asset set.
- The 2026-06-20 authored prop pass expands the same project-local mesh kit to
  route-critical world objects: rune spires/glyphs, gate keystone, chest lock
  plate, and camp standards.
- The 2026-06-20 authored environment pass adds project-local pine crowns, rock
  shards, ruin trims, and path stones to make the first route read less like
  pure shape-rendered terrain.
- The 2026-06-20 offline reference pass responds to the Albion-like reference
  by changing presentation grammar: more isometric camera framing and a
  five-slot action belt, while keeping the game local/offline and original.
- The 2026-06-20 art cleanup pass responds to the lead rejection that the visual
  still looked like debug by removing global wireframes, the visible ground
  grid, and top debug-style status panels.
