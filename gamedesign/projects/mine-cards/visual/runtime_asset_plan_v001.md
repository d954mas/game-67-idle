# Mine Cards Runtime Asset Plan v0.01

Status: `redirected after lead review`.

2026-06-17 redirect:

The procedural/blockout GLB path below was rejected by the lead as debug art.
Do not use it as the first accepted visual path. The current review path is the
Ozz-driven KayKit/kit proof recorded in
`visual/ozz_kaykit_runtime_proof_2026-06-17.md`.

## Visual Session Contract

- Goal: prove the Mining v0.01 screen with a living original 3D miner and real
  runtime assets.
- Non-goal: final character art, skeletal animation, marketplace asset
  integration, or public release art quality.
- Proof: native Mining screenshot, and ideally a short capture, showing the
  miner, active node, pickaxe, progress UI, and reward feedback.
- Stop condition: if the native screenshot reads as debug geometry rather than
  a game screen, feature expansion freezes and the next task becomes an art
  replacement pass.
- Likely files: `visual/3d_character_direction.md`, this plan, future
  project-specific asset generator/build files, and T0001 runtime files.

## Decision

The original procedural/blockout GLB decision below is superseded.

Current accepted first runtime miner path:

```text
KayKit Rig_Medium_Tools.glb + local kit Pickaxe_Stone.gltf -> generated C mesh
header + Ozz skeleton/clip archives -> game-side CPU-skinned render proof
```

This path is:

- public-safe with recorded KayKit/local kit provenance;
- connected to the reusable `extensions/skeletal_animation/` module;
- accepted for the first native Mining screen production path;
- replaceable by custom Mine Cards voxel character art later.

This is not final custom character art. It is the first accepted runtime model
and animation path after the lead rejected the blockout/procedural proof.

## Runtime Source Path

Current source type:

```text
public-safe ready rigged kit character and local kit pickaxe
```

Current conversion/build path:

```text
tools/assets/generate_kaykit_skinned_mesh_header.py
extensions/skeletal_animation/
gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_runtime/
```

Expected future runtime output family:

```text
src/mine_cards_kaykit_mesh.gen.h
game-side CPU-skinned model proof until a packed mesh path replaces the header
```

The exact generator and output paths can change during T0001 if the current
engine asset conventions require a different folder. The rule is that source,
generated outputs, and runtime ids must stay project-specific and reproducible.

## Locked v0.01 Asset Set

Required miner parts:

- `miner_body_blockout`
- `miner_head_blockout`
- `miner_left_arm_blockout`
- `miner_right_arm_blockout`
- `miner_left_leg_blockout` if full body is visible
- `miner_right_leg_blockout` if full body is visible
- `miner_helmet_blockout` or original hair/beard silhouette

Required equipment and props:

- `pickaxe_worn_blockout`
- `pickaxe_copper_blockout`
- `node_surface_stone_blockout`
- `node_copper_vein_blockout`
- `node_iron_locked_preview_blockout` if the native screen displays the locked
  preview as a 3D object

Optional first-pass support meshes:

- small ore chunks for reward pop;
- geode block for the rare event;
- compact mine backdrop props if the screen needs depth behind the miner.

## Public-Safety / IP Constraints

### Silhouette

- Do not use Minecraft Steve proportions.
- Avoid equal-width cube head/torso/limb proportions.
- Prefer a squat miner silhouette: wider torso, shorter head, thick gloves,
  bulky boots, and an exaggerated pickaxe.
- Add an original helmet/hair/beard/face shape so the character is not a
  generic voxel avatar.

### Textures And Materials

- Do not copy Minecraft block textures, palette, grid cadence, or tool shapes.
- Use simple mine-workshop colors: warm helmet/cloth, dark gloves/boots, copper
  accents after upgrade, and readable ore colors.
- Keep texture detail broad; tiny noisy voxel pixels will collapse at UI size.
- Procedural flat colors are allowed for v0.01, but must be chosen as art
  colors, not debug random colors.

### Pickaxes

- Worn Pickaxe should read as crude and small.
- Copper Pickaxe should change silhouette and color enough to prove upgrade
  value.
- Do not copy iconic Minecraft pickaxe proportions.

### Blocks And Nodes

- Stone and Copper must read as mine nodes, not as Minecraft blocks.
- Prefer irregular cuboid clusters, inset ore faces, and mine-rock silhouettes
  instead of perfect 1x1 block cubes.

## Provenance Rules

- Procedural GLB parts are repo/project-owned source and require a procedural
  exception in task evidence.
- They do not close a final-art task.
- Any later ready model must include license/provenance notes before runtime
  integration.
- Any later generated/custom model must record source workflow, accepted source
  files, and runtime conversion path before replacing the blockout set.

## T0001 Permission

T0001 must use the KayKit/Ozz production path unless it records a bounded
fallback with evidence. Procedural/blockout GLB parts are no longer accepted as
the first visual proof after lead review.

The native screen may be called a first proof only if it is labeled as using
blockout assets and passes:

- native mesh render/import proof;
- visible miner idle/mining transform motion;
- pickaxe mesh swap or clear Copper Pickaxe visual state;
- screenshot-vs-fake-shot direction review;
- UI readability/product gates.

It may not be called final visual art until the blockout set is replaced or the
lead explicitly accepts it as the public style.
