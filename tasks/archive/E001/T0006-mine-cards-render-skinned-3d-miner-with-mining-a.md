---
id: T0006
title: Mine Cards render skinned 3D miner with mining animation
status: done
epic: E001
priority: P1
tags: [mine-cards, animation, 3d, renderer, assets, ozz]
created: 2026-06-17
updated: 2026-06-17
---

## What

Replace the accepted skeletal debug/blockout proof with the first real native
3D miner render path: a skinned or intentionally rigid-block humanoid model in
the game, holding a pickaxe, playing the mining animation from the reusable
skeletal extension.

This is the production visual path for Mine Cards v0.01. The current blocky
shape proof in `src/clean_seed_main.c` only proves extension-to-game animation
wiring; it is not final game art and must not become the shipped miner.

Boundary:

```text
Do not edit external/neotolis-engine.
Use extension/game code and engine public rendering APIs.
If a missing engine hook blocks real mesh rendering, file a task/issue with
evidence instead of patching the submodule.
```

## Done when

- [x] First model target is chosen and recorded: KayKit mannequin,
      project-owned blocky/voxel humanoid, or generated/custom GLB.
- [x] Model provenance and license/IP safety are recorded in the Mine Cards
      visual docs.
- [x] Runtime model import/conversion path is defined: source GLB/FBX ->
      packed/runtime mesh + `.ozz` skeleton/clip or documented temporary local
      runtime files.
- [x] Native game renders a readable 3D miner model, not shape-only debug art.
- [x] Pickaxe is visibly attached to the animated hand socket.
- [x] Mining animation is driven by the reusable skeletal extension in
      `extensions/skeletal_animation/`, not a hand-authored timer.
- [x] Native GIF/contact sheet evidence shows the miner mining in `game_seed`
      or the first Mining screen.
- [x] Any renderer/engine limitation is captured as a separate issue/task with
      evidence; `external/neotolis-engine` remains unchanged.

## Open questions

- Use KayKit mannequin first for fastest skinned proof, or build the
  project-owned blocky/voxel miner first for stronger art direction?
- Does v0.01 require CPU-skinned mesh rendering immediately, or is rigid
  voxel-limb attachment acceptable if it uses the real model asset path?
- Should the first proof stay in `game_seed` while the Mining screen is built,
  or move directly into T0001's first Mining runtime?

## Log

- 2026-06-17: Created after the lead accepted the skeletal debug proof but
  asked when the game will show a full 3D miner model with mining animation
  instead of debug/blockout art.
- 2026-06-17: Lead rejected the procedural/blockout proof as debug art and
  asked to use a real kit asset path plus the existing Ozz integration.
- 2026-06-17: Added `tools/assets/generate_kaykit_skinned_mesh_header.py`,
  generating `src/mine_cards_kaykit_mesh.gen.h` from KayKit
  `Rig_Medium_Tools.glb` and local kit `Pickaxe_Stone.gltf`.
- 2026-06-17: Replaced the blockout mesh-part renderer with an Ozz-driven
  CPU-skinned KayKit proof in `src/mine_cards_model_proof.c`. The game copies
  model-space matrices from `extensions/skeletal_animation/` each frame and
  skins the KayKit mesh on the game side; `external/neotolis-engine` was not
  edited.
- 2026-06-17: Native evidence captured:
  `build/captures/mine_cards_ozz_kaykit_miner_v2_animation.gif` and
  `build/captures/mine_cards_ozz_kaykit_miner_v2_animation_sheet.png`. This is
  accepted for review as the first real kit/Ozz render proof, not final custom
  Mine Cards character art.
- 2026-06-18: Review accepted as the first real KayKit/Ozz miner render path.
  Follow-up T0001 evidence shows it inside the Mining screen with
  progress-synced sampling and clearer actor/target framing:
  `build/captures/mine_cards_core_moment_v004.gif` and
  `build/captures/mine_cards_compact_ui_v003_landscape_surface.png`.
