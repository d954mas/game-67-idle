---
type: Reference Digest
game_id: blockfell-runes
title: Blockfell Runes Reference Digest
created: 2026-06-20
updated: 2026-06-20
---

# Reference Digest

This digest converts the user's shorthand into implementation constraints
without copying protected setting, characters, names, models, or UI.

## User Shorthand

- "Skyrim": northern fantasy adventure mood, mountains, ruins, rune magic,
  hostile camp, loot, and a gate/pass objective.
- "Roblox": readable block-built silhouettes, toy-like scale, simple inputs,
  clear progress icons, and approachable action.
- 2026-06-20 added reference: Albion Online, but offline. Use it as visual/UX
  grammar only: isometric fantasy readability, equipment-forward hero/enemy
  silhouettes, chunky resource/route props, compact action bar, and clear
  sandbox route objectives. Do not copy Albion's brand, UI, icons, classes,
  economy, MMO/network model, or exact assets.

## Applied Direction

- Original game name: Blockfell Runes.
- World: compact blocky mountain valley with ruins, camp, shrine stones, chest,
  and a rune gate.
- First playable route: exploration -> combat -> loot -> rune progress -> gate.
- Visual rule: use block-fantasy readability, not direct clones of named
  references.
- Offline rule: Blockfell Runes remains a local/native single-player vertical
  slice. Reference-inspired systems must not imply online economy, guilds,
  multiplayer zones, or server dependence.

## Current Build Mismatch

- Latest combat/showcase capture:
  `build/captures/blockfell_art_cleanup_pass.png`.
- Improved: readable camp composition, enemy silhouettes, rune/gate targets,
  torch glows, terrain patches, crystals, banners, ground shadows, and
  procedural raster texture detail on grass/path/stone/wood/cloth/rune surfaces.
  The model pass also adds capes, helmets/horns, shields, weapon triangles, and
  longer directional shadows. The quest pass adds an objective route line,
  current-target beam/diamond, and a six-step HUD chain for route readability.
  The lighting pass adds normal-aware material lighting/fog, denser procedural
  texture detail, local light pools, sun rays, and character rim/decal accents.
  The authored asset pass adds a project-local mesh manifest and runtime mesh
  overlays for hero armor/cape/crest and enemy masks/horns. The authored prop
  pass extends that kit to rune spires/glyphs, gate keystone, chest lock plate,
  and camp standards. The authored environment pass adds pine crowns, rock
  shards, ruin trims, and path stones to the first route view.
- After the Albion-like offline reference, the biggest mismatch is presentation
  grammar: camera/HUD should read more like an isometric offline fantasy
  sandbox with an action belt and equipment/route affordances, while the world
  still keeps original Blockfell Runes content.
- Offline reference pass improved: camera now frames the valley more like an
  isometric fantasy sandbox and HUD includes a five-slot action belt. Remaining
  mismatch is still asset depth, animation/combat feel, and authored textures.
- Art cleanup pass removed global wireframes, visible grid lines, and top
  debug-style status panels. Remaining mismatch: forms are still too primitive
  and need a fuller authored art kit and polished UI treatment.
- Still missing: accepted project-local character/prop models, real texture or
  material maps, proper dynamic lighting/shadows, and engine-font objective
  labels.

## Next Reference Need

Before replacing the remaining procedural art, choose one accepted art lane:

- bright toy-fantasy;
- darker low-poly fantasy;
- chunky diorama RPG.
