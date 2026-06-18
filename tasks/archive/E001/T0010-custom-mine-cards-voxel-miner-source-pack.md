---
id: T0010
title: Custom Mine Cards voxel miner source pack
status: dropped
epic: E001
priority: P2
tags: [mine-cards, art, character, skeletal, source-pack, gated]
created: 2026-06-17
updated: 2026-06-18
---

## What

Prepare the production source packet for replacing the KayKit placeholder miner
with an original Mine Cards voxel/low-poly miner while preserving the accepted
Ozz/skinned-mesh runtime path.

This is gated behind T0001 lead acceptance or an explicit lead decision to
prioritize character production before accepting the first Mining slice. Until
then this task stays `idea` and must not change the playable screen.

Current inputs:

- Character direction:
  `gamedesign/projects/mine-cards/visual/3d_character_direction.md`
- Runtime asset plan:
  `gamedesign/projects/mine-cards/visual/runtime_asset_plan_v001.md`
- KayKit/Ozz proof:
  `gamedesign/projects/mine-cards/visual/ozz_kaykit_runtime_proof_2026-06-17.md`
- Reusable skinned mesh contract:
  `extensions/skeletal_animation/docs/skinned_mesh_renderer_contract_v001.md`
- Source packet:
  `gamedesign/projects/mine-cards/visual/custom_voxel_miner_source_packet_v001.md`
- Machine-readable character source pack:
  `gamedesign/projects/mine-cards/data/custom_voxel_miner_source_pack_v001.json`

Scope:

- original Mine Cards miner art direction and production source requirements;
- provenance/licensing rules for any generated, kitbashed, Blockbench, Blender,
  or AI-assisted 3D source;
- scale/origin/Y-up contract;
- skeleton/skin/socket compatibility with the reusable extension;
- starter pickaxe and future helmet/chest/tool attachment naming;
- validation proof list for source intake and native screenshot review.

Out of scope:

- changing `src/clean_seed_main.c` or the current native first screen;
- adding equipment/inventory mechanics;
- editing `external/neotolis-engine`;
- accepting debug/blockout art as final character art;
- promoting this task before T0001 is accepted unless the lead explicitly
  changes priority.

## Done when

- [ ] Lead accepts T0001 or explicitly prioritizes custom character production
      before T0001 acceptance.
- [x] The source packet has a selected production lane: Blockbench/Blender
      authored model, generated-to-retopology workflow, or public-safe kitbash
      with original Mine Cards identity pass.
- [ ] Accepted source files include provenance records, license/source notes,
      and a reproducible conversion path.
- [ ] The model contract defines Y-up source orientation, scale/origin,
      material/texture rules, skeleton expectations, joint/socket names, and
      attachment pivots.
- [ ] Native proof shows the custom miner mining with a pickaxe through the
      reusable skeletal extension, with `external/neotolis-engine` unchanged.
- [ ] Product gate or lead screenshot review confirms the character no longer
      reads as placeholder/debug art.

## Open questions / lead confirmations

- Production lane recommendation is now hand-authored Blockbench/Blender source.
  Lead can override this after T0001 acceptance, but the default is no
  unretopologized generated 3D and no unlicensed kitbash.
- First custom miner policy is hybrid: body/head/limbs skinned, tools and
  selected gear rigid-socketed.
- Old Mine Cards PSD hero is mining-fantasy reference only, not silhouette
  source. Keep strong distance from Minecraft-adjacent proportions/textures.

## Log

- 2026-06-18: Created as a gated prep task while T0001 remains in lead review.
  Added `custom_voxel_miner_source_packet_v001.md` so the next production art
  pass has source/provenance/runtime/validation requirements without changing
  the playable slice.
- 2026-06-18: Added machine-readable contract and validator:
  `gamedesign/projects/mine-cards/data/custom_voxel_miner_source_pack_v001.json`
  and `tools/assets/validate_character_source_pack.py`. The default first
  production lane is hand-authored Blockbench/Blender source; generated raster
  concepts are reference-only, body/head/limbs are skinned, and tools/selected
  gear are rigid-socketed. T0010 remains gated `idea` until T0001 is accepted
  or the lead explicitly changes priority.
- 2026-06-18: Closed with Mine Cards test run. Custom miner source prep is not active until a future game explicitly reopens it.
