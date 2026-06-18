---
id: T0005
title: Reusable skeletal animation extension integration
status: done
epic: E001
priority: P1
tags: [mine-cards, animation, 3d, renderer, ozz, extension]
created: 2026-06-17
updated: 2026-06-17
---

## What

Integrate the selected production skeletal path as a reusable engine-adjacent
extension/module, with Mine Cards as the first consumer/testbed.

Important boundary:

```text
external/neotolis-engine stays read-only.
The skeletal module lives beside the engine and is linked by games/extensions.
If the extension needs an engine change, file a task/issue with evidence
instead of changing the engine submodule.
```

Decision from T0004:

```text
Use ozz-animation for production sampling/blending behind an extension-owned
C-facing boundary.
Use builder/offline conversion for source formats; runtime must load packed
runtime animation assets, not parse GLB.
Use rigid hand/head sockets first; add skinned body/clothing/armor after the
base pose and attachment extension is proven.
```

This task is not Mine Cards-specific. Mine Cards validates the module, but the
API, build wiring, formats, and renderer bridge must be reusable in later games
without modifying the engine repository.

## Done when

- [x] Extension/module shape is designed against the engine's public
      composition/rendering/builder boundaries, including builder/runtime split
      and C17/C++ boundary.
- [x] `external/neotolis-engine` remains unchanged; any engine limitation is
      documented as an extension integration constraint or separate issue/task,
      not patched in-place.
- [x] ozz dependency source strategy is finalized for extension reuse: sibling
      submodule, vendored extension dependency, package, or documented local SDK
      path.
- [x] Game build has a stable optional ozz-enabled extension path that does not
      break the default C17/non-ozz engine/game builds.
- [x] A reusable C-facing extension API exists for skeleton/clip loading, pose
      sampling, joint matrix output, and named attachment lookup.
- [x] Runtime loads packed/runtime animation assets or explicit `.ozz` proof
      assets; no source GLB parsing happens in runtime.
- [x] Builder/offline conversion path is recorded or implemented:
      GLB/gltf2ozz -> extension runtime asset(s)/pack entries.
- [x] The module loads `rig_medium_skeleton.ozz` and
      `rig_medium_pickaxing.ozz` and returns per-frame model-space joint
      matrices through the reusable API.
- [x] A native visual proof renders either a skinned KayKit/mannequin miner or
      a visibly attached rigid pickaxe following `handslot.l/r` through the
      extension API.
- [x] The visual proof captures native screenshot/video evidence.
- [x] Renderer strategy is recorded: CPU skinning first, GPU skinning first, or
      rigid attachments before skinned body, with tradeoffs.
- [x] T0001 first Mining screen plan is updated to use the skeletal path or to
      keep a bounded fallback if renderer integration blocks.

## Open questions

- Should the reusable module be named `nt_anim`, `nt_skeletal_anim`, or split
  into `nt_anim` runtime + `nt_skin_renderer` renderer module?
- Should this live as `extensions/skeletal_animation/` in game repos first, a
  shared sibling repo, or a future submodule shared by games?
- Do we vendor ozz inside the extension, keep it as a sibling submodule, or keep
  it as an optional SDK path until the first renderer proof passes?
- Should the first visible proof be CPU-skinned body or rigid pickaxe attached
  to the animated hand socket?
- Which model is the first target: KayKit mannequin, local blockout miner, or a
  generated/custom voxel humanoid rig?
- Should `.ozz` archives become direct runtime assets first, or should the
  extension/builder bridge wrap them in NTPACK asset records immediately?
- The current native visual proof uses debug shape overlay markers/tool to prove
  game connection. T0001 still needs a real asset-path miner/tool, not this
  overlay as final game visuals.

## Log

- 2026-06-17: Created after lead chose production skeletal path. T0004 proved
  KayKit `Pickaxing` through ozz runtime sampling, but not game rendering.
- 2026-06-17: Scope corrected after lead clarified this must be reusable
  functionality, not a Mine Cards-only game module.
- 2026-06-17: Scope corrected again after lead clarified that the engine repo
  itself must not be changed. The right shape is an engine-adjacent reusable
  extension/module: games link it beside `external/neotolis-engine`; ozz and
  C++ stay behind the extension's C-facing boundary.
- 2026-06-17: Lead added a hard rule: never touch the engine submodule from
  this work. If an engine-side change is needed, capture an issue/task with
  evidence and keep Mine Cards work on the extension path.
- 2026-06-17: Implemented `extensions/skeletal_animation/` as the reusable
  engine-adjacent module. It owns the ozz C++ backend, exposes
  `include/skeletal_animation/nt_skeletal_animation.h` as a C-facing API, and
  builds `skeletal_animation_extension` plus `skeletal_animation_ozz_probe`
  only when `SKELETAL_ANIMATION_OZZ_SOURCE_DIR` is set.
- 2026-06-17: Connected the module to `game_seed` behind
  `SKELETAL_ANIMATION_EXTENSION_ENABLED`. Enabled builds load
  `rig_medium_skeleton.ozz` + `rig_medium_pickaxing.ozz`, sample attachment
  joints each frame, and draw a small native overlay/tool proof from extension
  output.
- 2026-06-17: Validation evidence: enabled configure/build passed with
  `cmake --preset native-debug "-DSKELETAL_ANIMATION_OZZ_SOURCE_DIR=<tmp ozz>"`
  and `cmake --build --preset native-debug --target game_seed`; C API probe
  `build/tools/native-debug/skeletal_animation_ozz_probe.exe --frames 8 --fps 4`
  reported moving `head`, `handslot.l`, and `handslot.r`.
- 2026-06-17: Native screenshot evidence captured through DevAPI:
  `build/captures/mine_cards_skeletal_extension_overlay.png`. This proves
  extension-to-game wiring, not final Mine Cards art quality.
- 2026-06-17: Native animation evidence captured from the running game:
  `build/captures/mine_cards_skeletal_extension_animation.gif` and
  `build/captures/mine_cards_skeletal_extension_animation_sheet.png`. The
  captured debug overlay/tool is driven by ozz attachment samples inside
  `game_seed`, not by an external viewer.
- 2026-06-17: Replaced the visually misleading stick overlay with a readable
  blocky miner/pickaxe mining blockout driven by the same sampled `handslot`
  data. Accepted evidence:
  `build/captures/mine_cards_blocky_miner_animation.gif` and
  `build/captures/mine_cards_blocky_miner_animation_sheet.png`. This is still
  debug/blockout art, not the final 3D model path; follow-up captured as T0006.
- 2026-06-18: Review accepted for the reusable production skeletal extension
  path. Later T0001/T0006 evidence proves the same extension drives the
  KayKit/Ozz miner in the native Mining screen, and
  `git diff --shortstat -- external/neotolis-engine` remains empty.
