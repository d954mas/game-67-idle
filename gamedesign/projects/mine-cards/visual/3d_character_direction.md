# 3D Character And Equipment Direction

Status: `accepted direction / first runtime path chosen`.

## Decision

Use a living 3D voxel character as a product differentiator against Melvor
Idle's mostly static drawn/app-like presentation.

Current first runtime asset path:

```text
KayKit Rig_Medium_Tools.glb + local kit Pickaxe_Stone.gltf, converted to
runtime Ozz skeleton/clip archives and CPU-skinned in game code
```

Detailed runtime proof:

`visual/ozz_kaykit_runtime_proof_2026-06-17.md`

The original procedural/blockout mesh-part plan was rejected by the lead as
debug art. The first accepted path is a production skeletal path using the
reusable `extensions/skeletal_animation/` module beside the engine. It is still
not final Mine Cards character art; it is the accepted public-safe kit/Ozz path
until custom voxel character art replaces the placeholder character.

Future custom Mine Cards character art should still be modular and equipment
aware:

- body;
- head;
- arms;
- legs/boots if visible;
- helmet;
- chest/coat;
- gloves;
- pickaxe/held tool;
- later weapon/shield/armor variants.

Animation is currently driven by the Ozz `Pickaxing` clip. Future custom gear
and clothing should remain compatible with skeleton attachments/skinning:

- idle bob;
- breathing/weight shift;
- arm pickaxe swing;
- pickaxe hit recoil;
- head look/turn;
- gear equip pop;
- ore/geode reward bounce.

## Why This Fits

Melvor's strength is systems depth. Its visual weakness is that the player
mostly reads panels. Our differentiator can be:

```text
Melvor-like depth, but the hero and equipment are visibly alive.
```

This also turns future equipment into a visual reward:

- new helmet appears on the miner;
- better pickaxe changes silhouette;
- armor upgrades show on the body;
- weapons matter when combat arrives later.

## Current Engine Reality

Current repo/engine evidence:

- `external/neotolis-engine/engine/renderers/nt_mesh_renderer.h` exposes a mesh
  renderer.
- `external/neotolis-engine/examples/sponza/` demonstrates GLB scene import,
  mesh resources, materials, textures, and mesh rendering.
- `external/neotolis-engine/tools/builder/nt_builder.h` and related builder
  code support parsing `.glb` scenes and extracting meshes/textures.
- I did not find a current gameplay-ready skeletal animation/skinning pipeline
  in the repo.

Implication:

- static or modular GLB meshes are viable;
- skeletal animation import/playback is not a safe v0.01 dependency;
- transform-driven modular animation is the pragmatic path.

## Recommended Technical Path

### Phase A - Reusable Skeletal Runtime Path

Goal:

Show a miner in the Mining screen with real 3D assets and production skeletal
motion without editing `external/neotolis-engine`.

Runtime model:

- `extensions/skeletal_animation/` owns Ozz loading/sampling behind a C-facing
  API;
- game code consumes model-space joint matrices and attachment transforms;
- the first renderer path CPU-skins the KayKit mesh and attaches a pickaxe to
  `handslot.l`;
- equipment later swaps skinned/attached mesh ids or visibility.

Required v0.01 parts:

- miner body;
- miner head/face;
- left arm;
- right arm;
- starter helmet or hair/beard;
- worn pickaxe;
- copper pickaxe;
- stone node;
- copper node.

Required v0.01 animations:

- idle loop;
- mining swing loop;
- hit impact/recoil;
- upgrade/equip pop.

### Phase B - Modular Gear Visuals

Goal:

Make equipment progression visible without combat.

Add:

- helmet slot;
- chest/torso overlay;
- gloves;
- boots if full body is visible;
- pickaxe variants.

Rule:

Every visible gear mesh must correspond to an actual progression item or future
slot. No decorative gear that implies a missing system.

### Phase C - Custom Mine Cards Character Art

Goal:

Replace the KayKit placeholder with original Mine Cards voxel/low-poly character
art while preserving the production skeletal path:

- custom miner body/head/gear silhouette;
- starter and copper pickaxe variants;
- armor/clothing attachments;
- validation on native screenshot/video.

This may become the preferred production path because ready humanoid animation
libraries can provide reusable motion, proper elbow/knee bending, and a better
base for armor/clothing. It is still a separate spike because the current engine
does not yet have the full skeleton/skin pipeline.

Detailed options:

`visual/animation_runtime_options.md`

Spike target:

- one original miner rigged as a humanoid;
- one ready animation clip playing in native;
- one rigid gear attachment following the hand/head/back;
- clear decision on CPU skinning, GPU skinning, or mesh-part fallback.

## Asset Sourcing Strategy

Use three lanes, in this order:

1. **Ready public-safe kit path.** Current v0.01 proof uses KayKit Character
   Animations `Rig_Medium_Tools.glb` plus a local kit pickaxe, with provenance
   recorded in `visual/ozz_kaykit_runtime_proof_2026-06-17.md`.
2. **Custom/generated art direction.** Generate concept/fake shots, then model
   original assets in Blockbench/Blender or a controlled AI-3D workflow.
3. **Procedural/blockout GLB parts.** Rejected as the accepted first visual path;
   keep only for temporary diagnostics that are clearly labeled debug/blockout.

Generated raster images are useful for visual direction, but they do not become
runtime 3D models by themselves. A 3D asset must still pass mesh import,
scale/origin, material, silhouette, and runtime screenshot checks.

## Modeling Tool Direction

Blockbench is a strong fit for this project because it is built around blocky
models, bones/parenting, pivot points, painting, and animation authoring. Even
if the engine does not ingest Blockbench animations yet, Blockbench can still be
used to create clean modular mesh parts and consistent pivots.

Blender remains useful for:

- GLB export cleanup;
- origin/pivot correction;
- texture/material cleanup;
- baking or simplifying assets;
- scripted generation if we build cuboid meshes ourselves.

## Art Rules

- Keep silhouettes chunky and readable at portrait UI size.
- Avoid Minecraft Steve proportions, textures, block palette, and iconic tools.
- Gear must exaggerate silhouette: pickaxe head, helmet lamp, shoulder/chest
  shape.
- Use consistent scale and attachment pivots across all gear.
- Keep textures simple; small noisy voxel textures will collapse under UI.

## v0.01 Runtime Proof

The first proof should show:

- a 3D miner in the Mining screen;
- an idle/mining transform animation;
- pickaxe mesh swap or visible Copper Pickaxe upgrade;
- stone/copper node mesh;
- native screenshot and, if possible, short capture proving motion.

Pass condition:

The character must make the screen feel more alive than a static Melvor-style
panel without delaying the core Mining loop.

Placeholder honesty rule:

- KayKit/Ozz is accepted as a production-path proof, not final custom character
  art;
- procedural/blockout meshes are diagnostic only unless explicitly reaccepted;
- no proof can be called final public art until custom character art or a
  clearly accepted kitbash style passes native screenshot review.

## Risks

| Risk | Mitigation |
|---|---|
| Animation work swallows the gameplay slice | Use transform animation first, not skeletal |
| Gear variants become content explosion | v0.01 only needs pickaxe swap |
| Ready models drift style or license | Treat as placeholders until provenance is recorded |
| Generated 3D assets have bad topology/pivots | Validate in GLB import and runtime screenshot before accepting |
| 3D scene competes with UI readability | Keep character area secondary to progress/reward UI |
