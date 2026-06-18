# Skeletal Model And Animation Research - 2026-06-17

Status: `active spike / partial proof`.

## Goal

Evaluate whether Mine Cards can move beyond modular mesh-part animation toward
a real skeletal/Mixamo-style path:

```text
rigged humanoid model -> animation clip -> GLB/runtime data -> native playback
```

The product reason remains strong: a Melvor-like idle game with a visible
animated miner, gear attachments, and later armor/clothing can differentiate
itself from static panel-first idle games.

## Local Tooling Check

Blender is available locally:

```text
C:\Program Files\Blender Foundation\Blender 4.3\blender.exe
Blender 4.3.2
```

Older Blender 3.2 is also installed, but 4.3.2 is sufficient for this spike.

No callable Blender MCP tool was exposed in the current Codex toolset after
tool discovery. Current path is therefore headless Blender CLI plus project
scripts. Updating Blender/MCP is not a blocker for this spike unless we need
interactive scene editing from Codex.

## External Candidate Sources

### Candidate A - KayKit Adventurers

Source:

https://kaylousberg.itch.io/kaykit-adventurers

Observed facts:

- Stylized low-poly fantasy characters.
- Fully rigged and animated.
- Comes with accessories.
- Includes `.FBX` and `.GLTF`.
- Listed as CC0 / free for personal and commercial use with no attribution
  required.

Fit:

- Good short-term art/source candidate.
- Style is less blocky than the fake shot but fits a friendly idle RPG.
- Engineer/barbarian-type silhouette could be kitbashed into a miner if source
  files are purchased or edited in Blender.

Risk:

- Itch download may need user/session interaction.
- Not voxel enough by default; should be treated as placeholder or kitbash
  source, not final Mine Cards identity.

### Candidate B - Quaternius Universal Base Characters

Source:

https://quaternius.com/packs/universalbasecharacters.html

Observed facts:

- 6 base character models.
- Game-ready topology optimized for animation.
- Humanoid rig compatible with retargeting.
- Compatible with Quaternius Universal Animation Library.
- `.FBX` and `.glTF` formats.
- CC0 for personal, educational, and commercial projects.
- Itch page exposes a free Standard zip, but direct CDN download was not
  completed in this pass.

Fit:

- Strong technical candidate for retargeting and runtime skeleton tests.
- Better for pipeline proof than final blocky miner art.

Risk:

- Not miner-specific.
- Base proportions may need custom helmet/beard/pickaxe silhouette to avoid
  looking generic.

### Candidate C - Quaternius Universal Animation Library

Source:

https://quaternius.com/packs/universalanimationlibrary.html

Observed facts:

- 120+ humanoid animations.
- Universal humanoid rig.
- Retargeting-ready.
- GLB/FBX/Blend formats listed.
- CC0 for personal, educational, and commercial projects.
- itch page currently shows v3.0 from 16 June 2026, with root motion added to
  locomotion/movement animations and GLB export called out for Unreal.

Fit:

- Best open-license animation candidate found for the first non-Mixamo pass.
- Stronger than Mixamo for the next technical step because it advertises GLB,
  root motion variants, and CC0 provenance without an Adobe account flow.
- Good for validating clip import/retarget without Adobe account flow.

Risk:

- A mining/pickaxe swing may still need custom authoring.
- Retargeting still requires Blender/export cleanup and engine runtime support.

### Candidate D - Mixamo

Source:

https://www.mixamo.com/

Fit:

- Strong future source for ready humanoid clips and auto-rigging tests.
- Good for idle, walk, combat, hit, gesture, and celebration clips.

Risk:

- Requires Adobe/Mixamo account flow and manual download.
- License/current terms must be verified before committed use.
- FBX clips still need conversion to GLB or a runtime format.
- Motion may be too realistic for a chunky miner without timing/exaggeration
  edits.

### Selected Ready Clip Source - KayKit Character Animations

Source:

https://kaylousberg.itch.io/kaykit-character-animations

Observed facts:

- Free 1.1 download is available on itch.
- Local download saved to
  `tmp/mine-cards/external/kaykit/KayKit_Character_Animations_Free_1_1.zip`.
- Pack license file states Creative Commons Zero / CC0 and permits personal,
  educational, and commercial use.
- Pack includes `FBX` and `GLTF`.
- `Rig_Medium_Tools.glb` contains 29 tool/action clips.
- Mining-adjacent clips include `Pickaxe`, `Pickaxing`, `Dig`, `Digging`,
  `Chop`, `Chopping`, `Hammering`, and `Working_*`.

Fit:

- Best first ready-animation source because it already contains a pickaxe loop.
- GLB import avoids the FBX/Mixamo conversion step for the first native data
  proof.
- The rig exposes `handslot.l` and `handslot.r` nodes, which are useful for
  rigid tool attachments.

Risk:

- This is not final Mine Cards art direction; it is a pipeline source.
- Retargeting onto a custom blocky miner is still separate work.
- Native game rendering/playback is not done yet; this is native data sampling.

## Local Blender Probe

Generated files:

- `gamedesign/projects/mine-cards/visual/skeletal_spike/minecards_skeletal_miner_probe.glb`
- `gamedesign/projects/mine-cards/visual/skeletal_spike/minecards_skeletal_miner_probe.blend`
- `gamedesign/projects/mine-cards/visual/skeletal_spike/minecards_skeletal_miner_probe_preview.png`
- `gamedesign/projects/mine-cards/visual/skeletal_spike/minecards_skeletal_miner_probe_manifest.json`

Generator:

`tools/assets/build_mine_cards_skeletal_probe.py`

Command:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 4.3\blender.exe' --background --python tools/assets/build_mine_cards_skeletal_probe.py
```

GLB structure check:

```json
{
  "meshes": 9,
  "skins": 1,
  "joints": 8,
  "animations": [
    {
      "name": "mine_swing_loop",
      "channels": 25,
      "samplers": 25
    }
  ]
}
```

Pose sampler:

```powershell
node tools/assets/inspect_skeletal_glb.mjs --time 0.5
```

Observed output:

```text
scene: 18 nodes, 9 meshes, 1 skin(s), 1 animation(s)
animation: mine_swing_loop, 25 channels, 0.03333s..2s, sampled at 0.5s
skeleton: MineCardsMinerRig, 8 joints, 8 inverse bind matrices
joints: root, spine, head, left_arm, right_arm, pickaxe, left_leg, right_leg
attachments: head [0,1.27,0], right_arm [0.42,1.07,0], pickaxe [1.05,0.4,0]
```

Native cgltf smoke:

```powershell
cmake --build --preset native-debug --target mine_cards_skeletal_glb_probe
build\tools\native-debug\mine_cards_skeletal_glb_probe.exe gamedesign\projects\mine-cards\visual\skeletal_spike\minecards_skeletal_miner_probe.glb
```

Observed output:

```text
scene: 18 nodes, 9 meshes, 1 skin(s), 1 animation(s)
skeleton: MineCardsMinerRig, 8 joints, inverse_bind_matrices=yes
joints: root, spine, head, left_arm, right_arm, pickaxe, left_leg, right_leg
animation: mine_swing_loop, 25 channels, 25 samplers
attachments: pickaxe=yes, right_arm=yes
```

What this proves:

- Blender 4.3 can generate a project-owned rigged animated GLB.
- GLB export can contain skeleton/skin data and animation clips.
- The project can parse a GLB skin and sample one animated pose outside
  Blender.
- A rigid gear attachment node such as `pickaxe` can be located from sampled
  joint/world transforms in a sidecar tool.
- A native C target can parse the same GLB with the vendored `cgltf` dependency
  and see the skin, animation, joints, inverse bind matrices, and attachment
  nodes.
- A Mine Cards-style blockout miner can be authored locally without external
  licensing.
- This is enough to test import/parsing/runtime conversion work.

## Ready Animation Native Proof

Selected ready clip:

`KayKit Character Animations Free 1.1 / Rig_Medium_Tools.glb / Pickaxing`

Source file:

`tmp/mine-cards/external/kaykit/free_1_1/KayKit_Character_Animations_1.1/Animations/gltf/Rig_Medium/Rig_Medium_Tools.glb`

Native command:

```powershell
cmake --build --preset native-debug --target mine_cards_skeletal_glb_probe
build\tools\native-debug\mine_cards_skeletal_glb_probe.exe "tmp\mine-cards\external\kaykit\free_1_1\KayKit_Character_Animations_1.1\Animations\gltf\Rig_Medium\Rig_Medium_Tools.glb" --animation Pickaxing --time 0.5
```

Observed output summary:

```text
scene: 30 nodes, 6 meshes, 1 skin(s), 29 animation(s)
skeleton: Rig_Medium, 23 joints, inverse_bind_matrices=yes
animation: Pickaxing (#19), 51 channels, 51 samplers
pose: sampled at 0.50000s
attachment.head: node=10 world_position=[0.11573, 1.13575, 0.35777]
attachment.handslot.l: node=0 world_position=[-0.12106, 0.57797, 0.53302]
attachment.handslot.r: node=5 world_position=[-0.09928, 0.60082, 0.61192]
sampled_joint_matrices: yes (23)
```

What this proves:

- A ready external GLB animation pack can be loaded by our native C/cgltf tool.
- We can select a named animation clip instead of only using the first clip.
- The native tool can sample the `Pickaxing` pose and compute joint matrices.
- Rigid tool sockets can be found from the sampled pose via `handslot.l` and
  `handslot.r`.

What this still does not prove:

- Native game playback over time.
- Rendering a skinned mesh in the engine.
- Retargeting this clip onto our blocky Mine Cards miner.
- Final visual quality.

## Ozz Production Runtime Proof

Decision:

```text
Adopt production skeletal path.
Use ozz-animation for runtime sampling/blending.
Use GLB -> gltf2ozz -> .ozz archives for conversion.
```

Source:

https://github.com/guillaumeblanc/ozz-animation

Observed official facts:

- ozz-animation is a C++ skeletal animation library and toolset.
- Runtime features include loading, sampling, and blending.
- It is renderer/game-engine agnostic.
- It includes a toolchain for major DCC formats including glTF.
- Runtime code depends on C++17 and standard C/C++ libraries.
- License is MIT.
- Release checked locally: `0.16.0`.

Local dependency check:

```powershell
cmake -S tmp\deps\ozz-animation\ozz-animation-0.16.0 `
  -B tmp\deps\ozz-animation\build-native `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_CXX_FLAGS=-D_CRT_SECURE_NO_WARNINGS `
  -DCMAKE_C_FLAGS=-D_CRT_SECURE_NO_WARNINGS `
  -Dozz_build_samples=OFF `
  -Dozz_build_howtos=OFF `
  -Dozz_build_tests=OFF `
  -Dozz_build_fbx=OFF `
  -Dozz_build_gltf=ON `
  -Dozz_build_tools=ON `
  -Dozz_build_data=OFF `
  -Dozz_build_msvc_rt_dll=ON
cmake --build tmp\deps\ozz-animation\build-native --target gltf2ozz
```

Observed result:

- Configure passed with Clang 19.1.7.
- Build passed after adding `_CRT_SECURE_NO_WARNINGS`.
- Without that define, ozz's `-Werror` turns MSVC CRT deprecation warnings
  around `fopen`/`strcpy` into build failures.

Conversion config:

`gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_pickaxing_import_config.json`

Conversion command:

```powershell
tmp\deps\ozz-animation\build-native\src\animation\offline\gltf\gltf2ozz.exe `
  --file="tmp\mine-cards\external\kaykit\free_1_1\KayKit_Character_Animations_1.1\Animations\gltf\Rig_Medium\Rig_Medium_Tools.glb" `
  --config_file="gamedesign\projects\mine-cards\visual\skeletal_spike\ozz_pickaxing_import_config.json" `
  --log_level=standard
```

Generated runtime archives:

- `gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_runtime/rig_medium_skeleton.ozz`
- `gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_runtime/rig_medium_pickaxing.ozz`

Ozz runtime probe:

- source:
  `extensions/skeletal_animation/tools/skeletal_animation_ozz_probe.c`
- optional CMake target:
  `skeletal_animation_ozz_probe`
- configure command:

```powershell
$ozz = (Resolve-Path 'tmp\deps\ozz-animation\ozz-animation-0.16.0').Path
cmake --preset native-debug "-DSKELETAL_ANIMATION_OZZ_SOURCE_DIR=$ozz"
cmake --build --preset native-debug --target skeletal_animation_ozz_probe
```

Runtime sampling command:

```powershell
build\tools\native-debug\skeletal_animation_ozz_probe.exe `
  --frames 8 `
  --fps 4 `
  --trace-csv gamedesign\projects\mine-cards\visual\skeletal_spike\ozz_runtime\rig_medium_pickaxing_ozz_trace.csv
```

Observed output summary:

```text
skeleton: joints=30 soa_joints=8
animation: duration=3.73333 tracks=30
attachments: head=21 handslot.l=15 handslot.r=20
frame 000 ... handslot.l=[-0.09147, 1.40226, 0.33275]
frame 002 ... handslot.l=[-0.12134, 0.57862, 0.53295]
frame 007 ... handslot.l=[-0.10652, 1.38966, 0.33497]
```

What this proves:

- KayKit `Pickaxing` can be converted to ozz runtime format.
- ozz runtime can load the converted skeleton and animation.
- ozz `SamplingJob` and `LocalToModelJob` produce per-frame model-space joint
  matrices.
- Hand attachment sockets move over time through the production runtime path.

What this still does not prove:

- Native game rendering of those matrices.
- CPU/GPU skinning inside the renderer.
- Retargeting to a Mine Cards custom blocky body.
- Final gear/clothing pipeline.

What this does not prove:

- Native engine playback of skeletal animation.
- Native clip interpolation/sampling.
- GPU or CPU skinning in the renderer.
- Runtime retargeting from KayKit/Quaternius/Mixamo clips.
- Final art quality.
- Correct elbows/knees/clothing deformation.
- Native gear attachment rendering.

## External Download Status

Quaternius Universal Base Characters:

- itch page found: `https://quaternius.itch.io/universal-base-characters`
- free upload found: `Universal Base Characters[Standard].zip`, `122 MB`
- upload id observed from itch page: `15861669`
- attempted automated download URL flow;
- result: itch returned a download landing page, not the final CDN zip;
- saved temporary HTML for inspection at
  `tmp/mine-cards/external/quaternius/universal_base_characters_download_page.html`.

Implication:

- external asset selection is done;
- actual external asset import still needs either an itch session/manual
  download, a proper itch download API flow, or an alternate source mirror;
- this does not block the local Blender/GLB skeletal probe.

Quaternius Universal Animation Library:

- itch page found: `https://quaternius.itch.io/universal-animation-library`
- page lists `Universal Animation Library[Standard].zip`, `15 MB`;
- page says the free standard set includes 45 animations;
- page currently advertises v3.0 / 16 June 2026 root-motion update;
- direct file download still needs the itch purchase/download URL flow or a
  manual session.

KayKit Adventurers:

- itch page found: `https://kaylousberg.itch.io/kaykit-adventurers`
- page lists `Free 2.0`, `12 MB`;
- page links to free KayKit Character Animations;
- direct file download still needs the itch purchase/download URL flow or a
  manual session.

KayKit Character Animations:

- itch page found:
  `https://kaylousberg.itch.io/kaykit-character-animations`;
- free upload downloaded successfully through the itch download flow;
- zip saved under `tmp/mine-cards/external/kaykit/`;
- extracted GLB `Rig_Medium_Tools.glb` was selected for the first ready clip
  proof;
- native `cgltf` proof passed on `Pickaxing`.

Local old Mine Cards art folder:

- accessible at `C:\Users\ROG\YandexDisk\gamedev\assets\my\Mine Cards`;
- contents are mostly PSD screen comps and old card/UI screens;
- useful for visual language, item/icon direction, and fake-shot reference;
- not a ready 3D model/skeleton source.

## Engine Reality

Current engine path already supports GLB mesh import and static mesh rendering.
The builder currently decodes common mesh attributes such as positions, normals,
UVs, and tangents. The current game layer does not yet have a proven runtime
path for:

- glTF animation sampling;
- joint hierarchy playback;
- inverse bind matrix use;
- JOINTS_0 / WEIGHTS_0 vertex streams;
- CPU/GPU skinning;
- skinned gear/clothing overlays.

The new native smoke target proves `cgltf` can reach the relevant data, but it
does not yet evaluate the clip or render a skinned pose.

Therefore the next technical decision is not "can Blender export a rigged GLB?"
It can. The decision is where to implement the runtime sidecar.

Boundary correction:

```text
Do not modify external/neotolis-engine for this path.
Build the skeletal runtime as a reusable engine-adjacent extension/module.
Games link the extension beside the engine; the extension depends on the
engine's public composition/rendering/builder seams.
```

## Recommended Path

Use a two-lane plan:

### Lane 1 - Keep T0001 Unblocked

Build the first Mining screen with modular mesh-part animation and procedural
GLB parts. This keeps the core loop moving.

### Lane 2 - Run Skeletal Sidecar

Use the local Blender probe first, then one external CC0 source:

1. Parse GLB skeleton, skins, and animation channels with a small reusable
   extension-side sidecar. Done at tool level with
   `tools/assets/inspect_skeletal_glb.mjs`.
2. Sample one clip into joint transforms. Done at tool level for
   `mine_swing_loop` at `0.5s`.
3. Attach one rigid pickaxe/helmet mesh to a joint. Partially proven at data
   level by sampled `pickaxe` world position; still needs native rendering.
4. Decide CPU skinning vs GPU skinning only after sampled pose data is working.
5. Bring in Quaternius or KayKit assets as the first external model/animation
   proof.
6. Try Mixamo only after the GLB sidecar path works and account/license/manual
   download friction is worth it.

## Decision Draft

For v0.01:

```text
Production skeletal path is selected.
Keep mesh-part animation only as a bounded fallback if renderer integration
blocks the first playable screen.
```

For the strategic differentiator:

```text
Skeletal animation is viable and selected as the production path.
Use ozz-animation for runtime sampling/blending.
Use KayKit Character Animations `Pickaxing` as the first ready-clip proof.
Next task: bridge ozz pose output to native rendering/skinning.
```
