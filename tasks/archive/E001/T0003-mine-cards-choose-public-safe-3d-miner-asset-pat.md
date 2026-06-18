---
id: T0003
title: Mine Cards choose public-safe 3D miner asset path
status: done
epic: E001
priority: P1
tags: [mine-cards, art, assets, 3d, visual]
created: 2026-06-17
updated: 2026-06-17
---

## What

Choose the first public-safe runtime source path for the Mine Cards 3D miner and
required v0.01 mining props.

The choice must avoid Minecraft-derived silhouettes/assets and must fit the
engine's real asset path. The goal is not final character art; the goal is a
source path that can produce a native screenshot with a readable original miner.

Candidate paths:

- procedural/blockout GLB parts generated locally;
- license-clean ready low-poly/voxel model used as placeholder or kitbash;
- custom modelled/generated asset through Blockbench/Blender or another
  controlled workflow.

## Done when

- [x] First source path is chosen and recorded in
      `gamedesign/projects/mine-cards/visual/3d_character_direction.md` or a
      linked asset plan.
- [x] Public-safety/IP constraints are listed for the miner silhouette,
      textures, pickaxe, and blocks.
- [x] Required v0.01 asset list is locked: body/head/arms, worn pickaxe, copper
      pickaxe, stone node, copper node.
- [x] Provenance/licensing expectations are recorded for any ready assets.
- [x] The decision states whether T0001 may proceed with blockout GLB parts if
      polished assets are not ready.

## Open questions

- Do we prioritize fastest native proof with procedural GLB parts, or spend a
  pass on a stronger custom miner before the first screen?
- Is Blockbench the preferred authoring source for the first modular miner?
- How far should the silhouette move from the fake shot to avoid
  Minecraft-like proportions?

## Log

- 2026-06-17: Created from base GDD review. Finding: v0.01 needs a chosen
  public-safe 3D miner source path before runtime work.
- 2026-06-17: Chosen first runtime source path: project-owned procedural/
  blockout GLB mesh parts. Plan saved in
  `gamedesign/projects/mine-cards/visual/runtime_asset_plan_v001.md`; T0001 may
  proceed with blockout GLB parts, but the proof must not be called final art.
- 2026-06-18: Replaced the rejected blockout path with the accepted public-safe
  KayKit/Ozz path. Updated `visual/3d_character_direction.md` and
  `visual/runtime_asset_plan_v001.md`; provenance and runtime proof live in
  `visual/ozz_kaykit_runtime_proof_2026-06-17.md`. T0006/T0001 evidence shows
  the KayKit miner and local kit pickaxe rendered in the native Mining screen.
