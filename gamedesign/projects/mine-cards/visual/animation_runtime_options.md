# Animation Runtime Options

Status: `technical direction note`.

Question:

Can we take a ready skeletal animation/skin solution and connect it beside the
engine?

Short answer:

Yes, but it should not be treated as a drop-in feature. A ready animation
library can sample skeleton poses, but the game/engine still needs asset
conversion, skeleton/clip storage, attachment rules, and a render path that
uses the pose.

## Current Recommendation

Use the production skeletal path:

```text
runtime sampling/blending: ozz-animation
offline conversion: GLB -> gltf2ozz -> .ozz skeleton/animation
first native renderer proof: ozz pose -> joint matrices -> visible attached
tool or skinned character
```

Mesh-part animation remains a bounded fallback only if renderer integration
blocks the first playable screen. It is no longer the target direction.

Current spike evidence:

- Blender 4.3.2 is installed locally and works in headless CLI mode.
- `tools/assets/build_mine_cards_skeletal_probe.py` generates a Mine Cards
  rigged/animated GLB probe.
- `tools/assets/inspect_skeletal_glb.mjs` parses the generated GLB, samples the
  animation pose, reads inverse bind matrices, and reports attachment node
  world positions. This is an offline asset-pipeline diagnostic tool, not game
  runtime.
- `tools/assets/inspect_skeletal_glb_native.c` builds as
  `mine_cards_skeletal_glb_probe` and proves the native C toolchain can parse
  and sample the same GLB with vendored `cgltf`.
- The generated GLB contains `skins: 1`, `joints: 8`, and animation clip
  `mine_swing_loop`.
- The sampled pose reports 18 nodes, 9 meshes, 1 skin, 8 joints, 8 inverse bind
  matrices, and attachment nodes `head`, `right_arm`, and `pickaxe`.
- The native smoke reports the same 18 nodes, 9 meshes, 1 skin, 8 joints, and
  25 animation channels/samplers.
- The first ready external clip is now imported as a GLB-only data proof from
  KayKit Character Animations Free 1.1:
  `tmp/mine-cards/external/kaykit/free_1_1/KayKit_Character_Animations_1.1/Animations/gltf/Rig_Medium/Rig_Medium_Tools.glb`.
- Native `cgltf` sampling of KayKit `Pickaxing` passes: 30 nodes, 6 meshes, 1
  skin, 29 animations, 23 joints, 51 channels/samplers, 23 sampled joint
  matrices, and attachment sockets `head`, `handslot.l`, `handslot.r`.
- Native `cgltf` playback trace passes over 8 frames and shows hand sockets
  moving over time. CSV:
  `tmp/mine-cards/external/kaykit/pickaxing_native_trace.csv`.
- `ozz-animation` 0.16.0 was downloaded and built locally in `tmp` with
  samples/tests/howtos/fbx disabled and glTF tooling enabled.
- `gltf2ozz` converts KayKit `Pickaxing` into runtime archives:
  `visual/skeletal_spike/ozz_runtime/rig_medium_skeleton.ozz` and
  `visual/skeletal_spike/ozz_runtime/rig_medium_pickaxing.ozz`.
- `extensions/skeletal_animation/tools/skeletal_animation_ozz_probe.c` builds as
  `skeletal_animation_ozz_probe` when `SKELETAL_ANIMATION_OZZ_SOURCE_DIR` is
  set. It
  loads the `.ozz` skeleton/animation and runs ozz `SamplingJob` +
  `LocalToModelJob` over 8 frames.
- Ozz runtime trace reports 30 joints, 30 animation tracks, duration
  `3.73333s`, and attachment joints `head`, `handslot.l`, `handslot.r`.
  Durable CSV:
  `visual/skeletal_spike/ozz_runtime/rig_medium_pickaxing_ozz_trace.csv`.
- Durable proof and source matrix live in
  `visual/skeletal_spike/skeletal_model_animation_research_2026-06-17.md`.

## Why Skeletal Animation Is Worth Considering

The lead's product argument is strong:

- ready humanoid animation libraries such as Mixamo can provide idle, walk,
  gesture, hit, celebration, and other human motion clips;
- elbows and knees deform properly with skinning instead of feeling like rigid
  toy parts;
- clothing, armor, gloves, boots, and held weapons can follow the body;
- future combat can reuse the same rig and animation library;
- gear progression becomes more satisfying when the hero actually wears it.

This could become a real differentiator:

```text
Melvor-like systems + visible animated geared-up avatar
```

The cost is that it changes the engine/asset pipeline scope.

## Option A - Modular Mesh-Part Animation

Use separate mesh entities for body parts and equipment, then animate their
transforms.

Examples:

- torso bob;
- head tilt;
- arm swing;
- pickaxe swing;
- pickaxe hit recoil;
- helmet/pickaxe equip pop.

Pros:

- fits voxel/blocky art;
- works with current mesh rendering path;
- makes equipment swaps easy;
- no skinned mesh shader required;
- fastest route to a living Mining screen.

Cons:

- less suitable for organic motion;
- animation authoring is manual unless we build an exporter/importer;
- too many parts can create draw-call/entity overhead if unmanaged.

Verdict:

Keep only as fallback for the first playable screen if skeletal renderer
integration blocks.

## Option B - glTF Animation Sidecar With cgltf

The engine builder already uses `cgltf` for GLB parsing. `cgltf` supports glTF
2.0 meshes, scenes/nodes, skins, and animations. We could add a sidecar
converter/evaluator that extracts:

- skeleton/joint hierarchy;
- animation channels;
- keyframes;
- inverse bind matrices;
- mesh joint/weight streams.

Pros:

- stays close to current GLB path;
- C-friendly;
- minimal new dependency;
- good stepping stone for custom runtime formats.

Cons:

- cgltf parses data; it does not provide a full animation playback/skinning
  engine;
- we must implement interpolation, pose evaluation, clip time, blending, and
  skinning/render integration;
- current mesh renderer/material path would still need joint-weight/shader or
  CPU-skinning support for skinned meshes.

Verdict:

Best custom-engine path after the modular proof, if we want to own the pipeline.

## Option C - ozz-animation Sidecar

`ozz-animation` is an open-source C++ skeletal animation runtime and toolset. It
handles loading, sampling, blending, optimized runtime structures, and has
offline conversion tooling.

Pros:

- real animation runtime instead of writing everything ourselves;
- renderer-agnostic and engine-agnostic;
- performance-oriented;
- supports major authoring formats through its toolchain;
- MIT license.

Cons:

- C++17 dependency in a C-first codebase;
- still does not render the character for us;
- still needs mesh skinning integration in our renderer;
- adds build/CMake/platform surface;
- may be overkill for voxel Mining v0.01.

Verdict:

Selected production runtime path.

Integration details:

- use ozz `gltf2ozz` for offline conversion;
- use ozz `SamplingJob` for clip sampling;
- use ozz `LocalToModelJob` for model-space joint matrices;
- keep renderer/skinning implementation inside a reusable engine-adjacent
  extension/module until the boundary is proven;
- do not modify `external/neotolis-engine`; treat engine integration as a
  consumer/link boundary, not an in-place engine patch;
- on Windows/LLVM, add `_CRT_SECURE_NO_WARNINGS` to ozz targets because ozz
  builds with `-Werror` and uses standard CRT calls that MSVC headers mark
  deprecated.

## Option D - Assimp Offline Importer

Assimp can load many 3D formats into a shared in-memory format and includes
post-processing tools. It is useful as a broad offline importer/converter.

Pros:

- broad format support;
- C/C++ APIs;
- permissive BSD-style license;
- useful if assets arrive as FBX/DAE/other formats.

Cons:

- heavy dependency compared with our current GLB-first path;
- best used offline, not as game runtime;
- broad import does not remove the need for our runtime animation/skin format.

Verdict:

Use only if asset sourcing forces non-GLB formats. Prefer GLB/cgltf first.

## Option E - Blockbench / Bedrock-Style Bone Animation

Blockbench is attractive for this project because it supports blocky model
authoring, bones/parenting, pivot points, paint mode, and animation authoring.

Instead of importing skinned meshes, we can import or export a simpler bone
animation format and apply it to modular mesh parts.

Pros:

- excellent fit for blocky models;
- animation can drive cuboid parts directly;
- easier than GPU skinning;
- good authoring workflow for pickaxe swings and idle loops.

Cons:

- requires a small importer/converter;
- format details must be locked;
- not a generic humanoid animation solution.

Verdict:

Potentially the best middle path for Mine Cards: authored bone transforms,
rendered as modular mesh parts.

## Proposed Production Extension API

Keep the first animation module engine-adjacent and reusable, not in the engine
submodule:

`extensions/skeletal_animation/` or a sibling reusable module repo

Minimal production concepts:

```text
Skeleton: ozz runtime skeleton archive + joint name lookup
Clip: ozz runtime animation archive
Pose: sampled ozz local transforms and model-space joint matrices
Attachment: gear mesh bound to a joint/part
```

Runtime flow:

```text
load .ozz skeleton/clip -> sample pose at time -> produce joint matrices ->
draw rigid attachments and/or skinned body
```

The first visible proof can use rigid attachments to avoid solving full mesh
skinning and renderer changes in the same step.

Layering:

```text
external/neotolis-engine: read-only engine dependency
reusable skeletal extension: ozz/C++ runtime, C-facing API, conversion bridge
Mine Cards game code: content, balance, screen logic, asset choices
```

Current implementation:

- reusable module root: `extensions/skeletal_animation/`;
- public API: `skeletal_animation/nt_skeletal_animation.h`;
- ozz backend: `src/nt_skeletal_animation_ozz.cpp`;
- game connection: `game_seed` links the extension behind
  `SKELETAL_ANIMATION_EXTENSION_ENABLED`;
- current visual proof: debug overlay attachment markers/tool in
  `build/captures/mine_cards_skeletal_extension_overlay.png`.

For the skeletal spike, the sidecar expands to:

```text
Skeleton: joints, parents, inverse bind matrices
SkinnedMesh: mesh + JOINTS_0/WEIGHTS_0 or converted runtime equivalent
Clip: translation/rotation/scale channels
Pose: sampled joint local/world matrices
Skinning: CPU or GPU path that deforms vertices
Attachments: rigid gear bound to a joint, or skinned gear using same skeleton
```

## Mixamo-Style Pipeline Target

Goal:

Take one ready humanoid animation clip and play it on one original Mine Cards
miner in the native game.

Likely authoring/conversion flow:

```text
original voxel/low-poly humanoid model ->
auto-rig or author humanoid skeleton ->
apply/download animation clip ->
convert to GLB/runtime skeleton+clip format ->
sample pose in sidecar ->
skin character or apply bone transforms ->
draw in native Mining screen
```

Important constraints:

- current engine GLB mesh import does not equal animation import;
- if source animations are FBX, we need an offline converter step before the
  current GLB-first builder can use them;
- ready humanoid motion may look too realistic for a chunky miner, so timing and
  exaggeration need review;
- mining/pickaxe swing may still need a custom authored clip if the ready
  library does not contain the exact action.

## Current External Source Preference

Prefer CC0, GLB/Blend-friendly sources before Mixamo:

1. KayKit Character Animations for the first ready mining-adjacent clip.
   `Rig_Medium_Tools.glb` contains `Pickaxe`, `Pickaxing`, `Dig`, `Digging`,
   `Chop`, `Chopping`, `Hammering`, and `Working_*` clips and has passed the
   native `cgltf` probe.
2. Quaternius Universal Base Characters for a retargetable humanoid model.
3. Quaternius Universal Animation Library for open-license humanoid clips. Its
   itch page currently shows a 16 June 2026 v3.0 update with root motion and
   GLB export notes.
4. KayKit Adventurers for stylized rigged/animated characters and accessories.
5. Mixamo after account/license/manual-download friction is worth paying.

Reason:

The first sidecar test needs repeatable asset provenance and GLB/Blend
conversion more than it needs the largest possible animation library.

## Armor And Clothing Model

There are two practical gear categories.

### Rigid Attachments

Examples:

- helmet;
- pickaxe;
- sword;
- shield;
- backpack;
- lantern.

Implementation:

- attach mesh to a named joint such as `head`, `right_hand`, `spine`, or
  `back`;
- no skinning needed for that gear mesh;
- good for v0.01 and early combat.

### Skinned Wearables

Examples:

- shirt/coat;
- pants;
- sleeves;
- gloves that bend;
- boots if ankles/feet animate;
- armor that should follow elbows/knees/torso.

Implementation:

- wearable mesh uses the same skeleton and weights as the body;
- swap or overlay skinned mesh parts by slot;
- no cloth simulation in early scope.

Rule:

Use rigid attachments first. Add skinned wearables once the base body skinning
path is proven.

## Spike Plan

### Spike 1: Mesh-Part Animation

Goal:

Show a miner with body/head/arm/pickaxe meshes and a mining swing.

Status:

Fallback only.

### Spike 2: Blockbench Authored Animation

Goal:

Create one Blockbench-style miner with bones/pivots and import a simple idle or
mining clip into our sidecar format.

Pass:

- clip data is not hand-coded;
- pivot positions are correct;
- equipment attachment still works.

### Spike 3: Real Skeletal Path

Goal:

Evaluate `ozz-animation` or a `cgltf`-based custom evaluator.

Status:

- ozz runtime sampling proof passed;
- renderer/skinning proof is next.

Additional pass:

- one rigid gear attachment follows a bone correctly;
- one wearable/armor strategy is chosen: skinned overlay now, or rigid-only
  until later.

## Recommendation For The GDD Base

State the feature as:

```text
living skeletal 3D voxel avatar with visible animated equipment
```

Do not promise yet:

```text
full skinned armor/clothing system at v0.01 launch
```

Base body animation and rigid gear attachment are now the production target.
Skinned wearables become the next layer after renderer proof.

Updated product stance:

```text
Target feature: animated skeletal 3D avatar with visible gear.
First proof: ozz-sampled `Pickaxing` driving a visible native miner/tool.
Strategic path: ozz runtime + GLB/gltf2ozz asset conversion + skinned/attached
equipment pipeline.
```
