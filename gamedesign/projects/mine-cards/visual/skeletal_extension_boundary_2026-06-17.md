# Skeletal Animation Extension Boundary - 2026-06-17

Status: `architecture boundary for T0005`.

## Decision

Production skeletal animation is a reusable engine-adjacent extension/module,
not an edit to `external/neotolis-engine` and not a Mine Cards-only subsystem.
The engine submodule is never patched from this project; engine changes become
separate issues/tasks with evidence and minimal repros.

Layering:

```text
external/neotolis-engine
  read-only engine dependency; existing render/resource/composition surface

reusable skeletal animation extension
  ozz/C++ runtime, C-facing API, converter bridge, pose/attachment/skinning API

games
  Mine Cards and future projects; content, balance, screen logic, asset choices
```

## Why

The engine is a shared dependency and must stay stable. Skeletal animation is
also too valuable to become Mine Cards-only: the same rig, clip, skinning, and
gear attachment path should be reusable by other projects.

The extension boundary lets us add C++/ozz without changing the C-first engine
repo. Games opt into the extension when they need animated characters.

## Extension Responsibilities

- Own the ozz dependency and its C++ build surface.
- Expose a small C-facing API for game/runtime use.
- Load runtime animation archives such as `.ozz`, not source GLB/FBX.
- Sample clips and produce local/model-space joint matrices.
- Resolve named attachment joints such as `head`, `handslot.l`, `handslot.r`.
- Provide the first renderer bridge: rigid attachment first, CPU skinning or
  GPU skinning after the pose path is visible.
- Provide or wrap offline conversion:
  `source GLB/FBX -> gltf2ozz -> runtime skeleton/clip assets`.

## Engine Responsibilities

- Remain unchanged for the first production proof.
- Provide public render/resource/composition APIs the extension can consume.
- Continue to own generic mesh/material/rendering behavior.
- Receive no ozz dependency from this project.
- If an engine limitation blocks the extension, get a separate issue/task with
  repro evidence instead of a submodule patch.

## Game Responsibilities

- Choose the visible character and equipment assets.
- Choose which clips are used by the Mining screen.
- Drive animation state from game state and activity state.
- Validate the screen against the Mine Cards fake-shot direction, readability,
  teachability, and core-loop gates.

## T0005 First Proof

The smallest useful vertical slice is:

```text
load rig_medium_skeleton.ozz
load rig_medium_pickaxing.ozz
sample Pickaxing over time
resolve hand socket
render a visible pickaxe/tool attached to that socket in the native screen/proof
capture screenshot or video evidence
```

This proves the reusable extension boundary before taking on full body
skinning, retargeting, or clothing.

Implemented first proof:

- module root: `extensions/skeletal_animation/`;
- public C API: `include/skeletal_animation/nt_skeletal_animation.h`;
- backend: ozz runtime behind `src/nt_skeletal_animation_ozz.cpp`;
- game connection: `game_seed` links `skeletal_animation_extension` only when
  `SKELETAL_ANIMATION_OZZ_SOURCE_DIR` is set;
- native proof screenshot:
  `build/captures/mine_cards_skeletal_extension_overlay.png`.

The current screenshot is a debug overlay proof, not final game art. The next
Mining screen must replace it with real asset-path miner/tool rendering or
record a bounded fallback.

## Open Design Choices

- Module location: `extensions/skeletal_animation/`, a sibling repo, or a
  future shared submodule.
- First API prefix: `nt_skeletal_anim`, `skeletal_anim`, or another neutral
  reusable name.
- First render bridge: rigid attachments, CPU-skinned mesh, or GPU-skinned mesh.
- Runtime asset packing: direct `.ozz` files first or NTPACK records through an
  extension/builder bridge.
