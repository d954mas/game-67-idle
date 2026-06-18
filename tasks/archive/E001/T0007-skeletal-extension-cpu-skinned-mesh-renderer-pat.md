---
id: T0007
title: Skeletal extension CPU-skinned mesh renderer path
status: done
epic: E001
priority: P2
tags: [mine-cards, animation, 3d, renderer, ozz, extension]
created: 2026-06-17
updated: 2026-06-17
---

## What

Turn the T0006 proof renderer into a reusable production skeletal mesh module
that can be shared by Mine Cards and later games.

The current T0006 implementation proves the path with game-local generated mesh
data and a dynamic CPU-skinned VBO. That is acceptable for the first native
evidence, but the reusable module still needs an explicit asset format,
lifetime rules, material handling, attachment API, and validation.

Preparation contract:

`extensions/skeletal_animation/docs/skinned_mesh_renderer_contract_v001.md`

Boundary:

```text
Do not edit external/neotolis-engine.
Build beside the engine as reusable extension/game module code.
If engine hooks are needed, capture a separate issue/task with evidence.
```

## Done when

- [x] Runtime source format is defined for skinned mesh data: positions,
      indices, joint indices, weights, inverse bind matrices, material slots,
      and attachment sockets.
- [x] The module loads or receives generated runtime data without hardcoding
      Mine Cards symbols in the reusable layer.
- [x] Ozz model matrices drive mesh deformation; no hand-authored animation
      timer is used for character pose.
- [x] CPU skinning has an explicit performance budget and a path to GPU
      skinning if the first-screen budget is exceeded.
- [x] Pickaxe/tool and later armor/clothing attachments are supported by joint
      name or stable socket id.
- [x] Native proof includes at least one KayKit/Quaternius-style character plus
      one attached tool in `game_seed` or the Mining screen.
- [x] `external/neotolis-engine` remains unchanged; any missing hook is filed
      separately.

## Open questions

- Confirm after implementation whether the CPU budget (`<= 0.5 ms average` for
  one visible actor) holds on the native debug development machine.
- Decide whether the first custom Mine Cards miner should be a neutral generated
  descriptor/header or a binary skinned-mesh asset after the v001 descriptor API
  is proven.

## Log

- 2026-06-17: Created after T0006 proved an Ozz-driven KayKit CPU-skinned mesh
  in the game. This tracks production hardening and reuse beyond the local
  proof renderer.
- 2026-06-18: Prepared v001 reusable module contract at
  `extensions/skeletal_animation/docs/skinned_mesh_renderer_contract_v001.md`.
  Decisions recorded: keep v001 in `extensions/skeletal_animation/`; split the
  pose sampler API from skinned mesh API; use CPU skinning first with a measured
  budget before GPU work; accept generated neutral descriptors/headers for the
  first proof while forbidding Mine Cards symbols inside the reusable module.
  Task remains backlog until T0001 is accepted or the lead explicitly starts
  this production slice.
- 2026-06-18: Added compile/runtime scaffold for the reusable skinned mesh
  layer without changing the playable screen:
  `extensions/skeletal_animation/include/skeletal_animation/nt_skeletal_mesh.h`,
  `extensions/skeletal_animation/src/nt_skeletal_mesh_cpu.cpp`, and
  `extensions/skeletal_animation/tools/skeletal_mesh_contract_probe.c`.
  The scaffold owns neutral descriptors, CPU skinning from supplied model
  matrices, socket matrices, and a no-op draw bridge. Evidence:
  `cmake --build --preset native-debug --target skeletal_mesh_contract_probe`,
  `build/tools/native-debug/skeletal_mesh_contract_probe.exe`, and
  `cmake --build --preset native-debug --target game_seed` all passed. Native
  Mining-screen proof and final engine-boundary check remain open.
- 2026-06-18: Hardened the neutral contract probe and socket implementation.
  Socket transforms now apply local offset, quaternion rotation, and scale
  instead of translation only. The probe covers the positive CPU-skinning path
  plus descriptor/API failure paths: null descriptor, null output mesh, invalid
  index, missing vertex joint, missing socket joint, null mesh instance,
  socket/copy before pose, too few matrices, small output buffer, and missing
  socket. Evidence: `cmake --build --preset native-debug --target
  skeletal_mesh_contract_probe`, `build/tools/native-debug/skeletal_mesh_contract_probe.exe`
  (`failure_paths=11`), and `cmake --build --preset native-debug --target
  game_seed` passed. Task remains backlog; native Mining-screen proof remains
  open until the lead accepts T0001 or explicitly starts T0007.
- 2026-06-18: Started the game-facing proof pass: the next step is to move the
  `game_seed` KayKit character skinning/tool socket usage from game-local code
  onto the reusable `nt_skeletal_mesh_*` API while keeping Mine Cards-specific
  data and rendering outside `extensions/skeletal_animation/`.
- 2026-06-18: Moved the `game_seed` KayKit character proof onto the reusable
  `nt_skeletal_mesh_*` CPU skinning and `tool` socket path. Mine Cards now
  builds a neutral descriptor from generated data, reorders Ozz matrices into
  mesh joint order, copies reusable skinned positions into the game VBO, and
  places the pickaxe from the reusable socket matrix. Evidence:
  `cmake --build --preset native-debug --target game_seed`,
  `cmake --build --preset native-debug --target skeletal_mesh_contract_probe`,
  and `py -3.12 tmp\t0007_skeletal_game_proof.py` passed. Screenshot:
  `build/captures/mine_cards_t0007_skeletal_game_proof.png`; zoom:
  `build/captures/mine_cards_t0007_skeletal_game_proof_actor_zoom.png`; JSON:
  `build/captures/mine_cards_t0007_skeletal_game_proof.json`.
- 2026-06-18: The native proof also measured reusable CPU skinning at
  `0.685ms` average for `6675` vertices and `23` joints, above the v001 budget
  of `<= 0.500ms`. Created T0009 for mesh/LOD/perf trim before treating this
  path as budget-clean production art runtime.
- 2026-06-18: Engine boundary check passed: `git diff --shortstat --
  external/neotolis-engine` produced no diff output (only the local git warning
  about `C:\Users\ROG/.config/git/ignore`). Moved to review.
- 2026-06-18: T0009 closed the CPU-budget debt for this proof by precomputing
  per-joint skin matrices in the reusable CPU skinner. Current native proof
  records `0.162ms` average for `6675` vertices / `23` joints, under the
  `<=0.500ms` v001 budget.
- 2026-06-18: All T0007 acceptance criteria are now backed by native proof,
  contract probe, budget evidence, and engine-boundary check. Archiving as
  done; this does not accept T0001 or authorize mechanic expansion by itself.
