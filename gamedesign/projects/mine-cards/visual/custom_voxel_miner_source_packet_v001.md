# Custom Voxel Miner Source Packet v001

Date: 2026-06-18
Task: `T0010`
Status: gated prep; do not start runtime integration until T0001 is accepted or
the lead explicitly prioritizes custom character production.

## Visual Session Contract

- Goal: define the production source requirements for an original Mine Cards
  voxel/low-poly miner that replaces the KayKit placeholder while preserving the
  accepted Ozz/skinned runtime path.
- Non-goal: change the current T0001 playable screen, add equipment mechanics,
  or edit `external/neotolis-engine`.
- Proof: accepted source/provenance records, conversion evidence, native
  screenshot/capture with the custom miner mining, and a lead/product visual
  review.
- Stop condition: if the source or native proof reads as debug/blockout,
  Minecraft-adjacent clone art, or incompatible with the socket/skinning
  contract, freeze integration and repair the art packet before gameplay
  expansion.
- Likely files: this packet, future `art_requests/` or `visual/character/`
  records, generated model source files, conversion logs, and native captures.

## Current Baseline

Accepted runtime path:

```text
KayKit Rig_Medium_Tools.glb + kit pickaxe
-> Ozz skeleton/clip archives
-> reusable extensions/skeletal_animation module
-> CPU-skinned native Mine Cards proof with a tool socket
```

Current evidence:

- `gamedesign/projects/mine-cards/visual/3d_character_direction.md`
- `gamedesign/projects/mine-cards/visual/ozz_kaykit_runtime_proof_2026-06-17.md`
- `extensions/skeletal_animation/docs/skinned_mesh_renderer_contract_v001.md`
- `build/captures/mine_cards_t0007_skeletal_game_proof.png`
- `build/captures/mine_cards_t0009_skeletal_budget_actor_zoom.png`
- Machine-readable contract:
  `gamedesign/projects/mine-cards/data/custom_voxel_miner_source_pack_v001.json`

This baseline is accepted as a production path proof, not final Mine Cards
character art.

## Production Target

Create one original miner character that is readable in the fixed top action
stage:

- squat, sturdy mining silhouette;
- broad torso, thick gloves/boots, compact head, strong pickaxe read;
- helmet or hair/beard shape that is Mine Cards-specific;
- warm mine-workshop palette with copper accents for future upgrades;
- simple broad texture areas that survive small UI scale;
- not Minecraft Steve proportions, not Minecraft textures, not iconic Minecraft
  pickaxe shapes.

The first custom character must prove:

- idle/mining animation remains alive on the stage;
- pickaxe is attached through a named socket;
- gear direction can later support visible helmet/chest/tool upgrades;
- screenshot feels like a game character, not debug geometry.

## Source Lanes

Chosen first lane:

```text
hand-authored Blockbench/Blender source
```

Reason:

- precise pivots and Y-up source control;
- stable socket/skinning compatibility with the accepted Ozz path;
- strongest public-safety distance from Minecraft-adjacent or unlicensed kit art;
- generated raster concepts can still help the silhouette, but only as reference.

Other lanes remain allowed later, but not as the first accepted candidate:

1. Hand-authored Blockbench/Blender source.
   Best for precise pivots, cuboid style, and public-safe ownership.
2. Generated concept/fake shot -> modeled cleanup.
   Best for exploring silhouette and material, but the raster output is
   reference only until converted into clean 3D source.
3. Public-safe kitbash -> original Mine Cards identity pass.
   Fastest, but requires license/provenance records and stronger style review
   so it does not stay generic.

Do not accept any lane without a provenance record.

## First Candidate Policy

Body/gear split:

- skinned: body, head, arms, hands/gloves, legs/boots;
- rigid socketed: worn pickaxe, copper pickaxe, helmet lamp if authored as a
  separate mesh;
- future slots reserved: head, chest, hands, boots, tool, back.

Public-safety distance:

- the old Mine Cards PSD hero may inform only the mining fantasy;
- do not copy its silhouette directly;
- avoid Minecraft Steve proportions, Minecraft texture cadence, and iconic
  Minecraft pickaxe proportions;
- make the miner squat, broad, glove/boot-heavy, and Mine Cards-specific.

Validation command for the contract:

```powershell
py -3.12 tools/assets/validate_character_source_pack.py --pack gamedesign/projects/mine-cards/data/custom_voxel_miner_source_pack_v001.json
```

## Required Source Records

Each accepted candidate needs:

- source author/workflow/provider;
- license or ownership note;
- prompt/workflow files when generated or AI-assisted;
- source model path and exported runtime model path;
- rejected candidate notes;
- conversion command/log;
- texture/material source paths;
- known limitations.

Generated raster images may be saved as reference/fake-shot material, but they
are not runtime 3D assets.

## Runtime Contract

Coordinate and scale:

- source content is authored as Y-up;
- source forward axis must be recorded;
- origin at character feet/ground center unless a conversion note says
  otherwise;
- height target: fits the existing T0001 top action stage without hiding HUD or
  lower mechanics board;
- `asset_to_model` or equivalent conversion matrix must be explicit.

Skeleton and animation:

- compatible with the reusable `extensions/skeletal_animation` Ozz path;
- first proof may reuse the current `Pickaxing` clip if the retarget is clean;
- missing or broken elbow/knee skinning is a rejection issue, not polish;
- if retargeting fails, record whether the fallback is repaired rigging or a
  scoped rigid-attachment proof.

Sockets and attachments:

- `tool` or `handslot.l`: worn/copper pickaxe attachment;
- `head`: helmet/lamp attachment;
- `chest`: future armor/chest piece;
- `back` optional: future backpack/tool carry;
- every socket needs joint name, local offset, rotation, and scale.

Materials:

- prefer a small texture set or vertex colors with deliberate art palette;
- no baked labels, UI text, watermark, or random letters;
- no tiny noisy voxel texture that collapses at stage size;
- future gear should be separable by mesh/material slot or attachment.

## Minimum Asset Set For First Candidate

- `miner_body_custom_v001`
- `miner_head_custom_v001`
- `miner_hands_gloves_custom_v001`
- `miner_boots_custom_v001`
- `miner_helmet_or_hair_custom_v001`
- `pickaxe_worn_custom_v001`
- optional `pickaxe_copper_custom_v001` if the first proof includes the upgrade
  state

## QA And Rejection Rules

Reject the candidate if:

- it looks like Minecraft Steve or uses Minecraft-like textures/proportions;
- it reads as procedural/debug/blockout art;
- silhouette is unclear in the T0001 top stage;
- pickaxe socket drifts, clips badly, or points the wrong way during mining;
- skinning visibly collapses at elbows/knees;
- source license/provenance is missing;
- model cannot be reproduced from recorded source/conversion steps;
- native screenshot/capture makes the current first screen worse.

## Validation Packet

Before integration is called done:

- source/provenance record exists;
- model inspection records mesh count, joint count, material slots, sockets,
  source axes, and triangle/vertex counts;
- build uses `extensions/skeletal_animation`, not engine edits;
- native capture shows idle/mining motion with the custom miner and attached
  pickaxe;
- CPU skinning remains within the established one-actor budget or records a
  measured remediation;
- `external/neotolis-engine` has no diff;
- screenshot review compares against T0001 direction and lead feedback.

Draft contract validation must pass before candidate production starts:

```powershell
py -3.12 tools/assets/validate_character_source_pack.py --pack gamedesign/projects/mine-cards/data/custom_voxel_miner_source_pack_v001.json
```
