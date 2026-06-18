---
id: T0004
title: Mine Cards skeletal animation sidecar spike
status: done
epic: E001
priority: P2
tags: [mine-cards, animation, 3d, spike, engine-adjacent]
created: 2026-06-17
updated: 2026-06-17
---

## What

Run a scoped technical spike for a skeletal/Mixamo-style animation sidecar. The
lead explicitly asked to check 3D, skeletons, animation, suitable models, and
ready animation sources.

The lead selected the production skeletal path after the ready-animation proof.
This task now closes the feasibility spike and hands off renderer/skinning work
to a production integration task.

Spike target:

- one original or placeholder Mine Cards humanoid miner;
- one ready humanoid animation clip;
- one rigid gear attachment following a hand/head/back joint;
- a native proof or clear failure report.

## Done when

- [x] Candidate runtime path is selected: `cgltf` custom evaluator,
      `ozz-animation`, Blockbench-style bone transforms, or another explicit
      option.
- [x] Asset conversion path is documented: source model/clip -> GLB/runtime
      data -> native playback.
- [x] Local GLB sidecar can parse one skin, sample one clip pose, and report
      attachment node transforms.
- [x] Native C/cgltf smoke target can parse the local GLB and see skin, joints,
      inverse bind matrices, animation channels, and attachment nodes.
- [x] Native C/cgltf smoke target can parse and sample one ready external
      mining-adjacent GLB clip (`KayKit Character Animations / Pickaxing`) and
      report hand-slot attachment nodes.
- [x] One native proof shows the clip playing, or the task records why the path
      is not viable yet.
- [x] One rigid gear attachment follows the animated pose, or the task records
      why gear must remain mesh-part only.
- [x] Decision is recorded: adopt skeletal path now, defer it, or stay with
      mesh-part animation for v0.01.

## Open questions

- Which exact character model becomes the first skinned Mine Cards miner:
  KayKit mannequin retarget proof, local blockout retarget, or custom generated
  voxel humanoid rig?
- Should the first renderer proof use CPU skinning for speed of integration, or
  go directly to GPU skinning?

## Log

- 2026-06-17: Created from the lead's skeletal-animation direction. This is a
  strategic candidate, but it is intentionally separate from the first native
  Mining screen.
- 2026-06-17: Started spike after lead asked to check 3D/skeleton/animation and
  suitable ready model/animation sources.
- 2026-06-17: Blender 4.3.2 found at
  `C:\Program Files\Blender Foundation\Blender 4.3\blender.exe`; no callable
  Blender MCP exposed in current toolset.
- 2026-06-17: Added `tools/assets/build_mine_cards_skeletal_probe.py` and
  generated local proof assets under
  `gamedesign/projects/mine-cards/visual/skeletal_spike/`.
- 2026-06-17: GLB structure check passed for probe: 9 meshes, 1 skin, 8 joints,
  animation `mine_swing_loop` with 25 channels. This proves Blender/GLB
  skeleton export, not native runtime playback.
- 2026-06-17: External source shortlist recorded in
  `gamedesign/projects/mine-cards/data/asset_candidates.json`; preferred first
  sources are Quaternius Universal Base Characters + Universal Animation
  Library, then KayKit Adventurers, with Mixamo as second-stage/manual account
  flow.
- 2026-06-17: Found Quaternius itch free upload for Universal Base Characters
  Standard zip (`122 MB`, upload id `15861669`). Automated request reached the
  itch download landing page but did not fetch the final CDN zip; saved the HTML
  under `tmp/mine-cards/external/quaternius/` for follow-up.
- 2026-06-17: Added `tools/assets/inspect_skeletal_glb.mjs`. It samples
  `mine_swing_loop` at `0.5s`, reads 8 inverse bind matrices, and reports
  attachment node world positions for `head`, `right_arm`, and `pickaxe`.
  This proves GLB-side pose evaluation, not native renderer playback.
- 2026-06-17: Rechecked local old art folder. It contains PSD screen comps and
  old card/UI assets, useful as visual/reference input but not ready 3D models
  or animation clips.
- 2026-06-17: Rechecked current itch metadata. Quaternius Universal Animation
  Library now shows a v3.0 update from 2026-06-16 with root motion and GLB
  export notes; KayKit Adventurers lists Free 2.0 (12 MB). Direct downloads
  still require itch purchase/download URL flow or a manual session.
- 2026-06-17: Added native smoke target
  `mine_cards_skeletal_glb_probe` (`tools/assets/inspect_skeletal_glb_native.c`)
  in the root CMake project. It builds and runs on the probe GLB, reporting 18
  nodes, 9 meshes, 1 skin, 8 joints, inverse bind matrices, `mine_swing_loop`
  with 25 channels/samplers, and attachment nodes `pickaxe`/`right_arm`.
  This proves native cgltf data access, not native animation playback/skinning.
- 2026-06-17: Downloaded KayKit Character Animations Free 1.1 to
  `tmp/mine-cards/external/kaykit/` and selected
  `Animations/gltf/Rig_Medium/Rig_Medium_Tools.glb` as the first ready
  animation proof. The pack license file states CC0.
- 2026-06-17: Extended the native smoke target to choose a named clip with
  `--animation`. Native run on KayKit `Pickaxing` reports 30 nodes, 6 meshes, 1
  skin, 29 animations, 23 joints, inverse bind matrices, `Pickaxing` at index
  19 with 51 channels/samplers, 23 sampled joint matrices, and attachment
  sockets `head`, `handslot.l`, `handslot.r`. This proves a ready external GLB
  clip can be parsed and sampled natively, but still not rendered/played in the
  game loop.
- 2026-06-17: Added native C playback trace support to
  `mine_cards_skeletal_glb_probe` with `--frames`, `--fps`, and `--trace-csv`.
  Running KayKit `Pickaxing` for 8 frames shows animated motion over time:
  `handslot.l` moves from roughly `[-0.09147, 1.40226, 0.33275]` at frame 0
  to `[-0.12134, 0.57862, 0.53295]` at frame 2 and back toward the high swing
  by frame 7. CSV evidence:
  `tmp/mine-cards/external/kaykit/pickaxing_native_trace.csv`.
- 2026-06-17: Lead chose the production skeletal path. Selected
  `ozz-animation` as runtime animation library rather than continuing with the
  hand-written `cgltf` sampler as the product runtime.
- 2026-06-17: Downloaded and built `ozz-animation` 0.16.0 in `tmp` with
  samples/tests/howtos/fbx disabled and glTF tooling enabled. Windows/LLVM
  requires `_CRT_SECURE_NO_WARNINGS` because ozz builds with `-Werror` and uses
  standard CRT calls such as `fopen`/`strcpy`.
- 2026-06-17: Added `ozz_pickaxing_import_config.json` and converted KayKit
  `Rig_Medium_Tools.glb` into ozz runtime archives:
  `visual/skeletal_spike/ozz_runtime/rig_medium_skeleton.ozz` and
  `visual/skeletal_spike/ozz_runtime/rig_medium_pickaxing.ozz`.
- 2026-06-17: Added optional CMake integration via
  `SKELETAL_ANIMATION_OZZ_SOURCE_DIR` and
  `extensions/skeletal_animation/tools/skeletal_animation_ozz_probe.c`.
  `skeletal_animation_ozz_probe` loads the `.ozz` skeleton/animation, runs
  ozz `SamplingJob` + `LocalToModelJob`, and traces `head`, `handslot.l`, and
  `handslot.r` over 8 frames. Evidence CSV:
  `gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_runtime/rig_medium_pickaxing_ozz_trace.csv`.
- 2026-06-17: Scope boundary corrected after lead clarified the engine
  repository must not be changed. The production follow-up is a reusable
  engine-adjacent extension/module that games link beside
  `external/neotolis-engine`, not a patch inside the engine submodule.
- 2026-06-17: Decision recorded: adopt production skeletal path for Mine Cards.
  Next production task is renderer/skinning/bridge integration; this spike is
  in review because it proves animation runtime playback, not final game
  rendering.
- 2026-06-18: Closed as feasibility spike. Lead selected the production
  skeletal path, T0007 tracks renderer/skinning integration, and
  `gamedesign/projects/mine-cards/reviews/next_slice_decision_packet_2026-06-18.md`
  records why the remaining work belongs to the production follow-up rather than
  this spike.
