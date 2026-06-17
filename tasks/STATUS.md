# Project Status

## Current Goal

`Voxelheim` is in **rescue redesign** after lead feedback that the prototype is
ugly, unclear, hard to read, and too simple/banal. The current target is
**Voxelheim: Frost Keep Rebuilder**: a native idle RPG where auto-combat earns
Gold + Frost Blocks and the player visibly repairs the Frost Keep room by room.

Primary source of truth:

- `gamedesign/projects/voxelheim/gdd.md`
- `gamedesign/projects/voxelheim/data/rescue_loop.json`
- `gamedesign/projects/voxelheim/visual/ui_ux_rescue_spec.md`
- `gamedesign/projects/voxelheim/references/competitor_deconstruction_2026-06-17.md`
- `gamedesign/projects/voxelheim/reviews/prototype_deconstruction_2026-06-17.md`

## What's Built

The native build in `src/voxelheim_main.c` now proves the first
**Frost Keep Rebuilder** loop:

- auto-combat earns Gold + Frost Blocks;
- Gate -> Forge -> Campfire repairs create Keep Rank, cards, training, helper;
- Keep Rank 3 unlocks **Avalanche Reset** and persistent Frost Shards;
- **Frost Blueprints** spend Shards on permanent bonuses;
- offline return unlocks after Avalanche Reset and grants Gold + Blocks;
- latest UI pass separates Frost Keep objective from Frost Blueprints and keeps
  card-choice/offline popup text readable;
- reward feedback now exists for repair, card choice, Avalanche Reset,
  Blueprint purchase, and offline collect via sprite bursts, floaters, pulses,
  and generated audio cues.

Remaining drift: final room art, reward timing/audio polish, and broader
retention review are still not complete.

## Build / Run / Shoot

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
py -3.12 tools/devapi/shoot_voxelheim.py build/captures/x.png 9123
py -3.12 tmp/shard_blueprints_probe.py 9162 build/captures/ui_rescue_blueprints_layout.png
py -3.12 tmp/reward_feedback_probe.py
```

## Current Gate

**Repair-chain + Avalanche Reset + Frost Blueprints + Offline Return
product-read gates: passed for the current native slice.** The old
visual/teachability pass is no longer accepted as product proof; this pass is
specific to the first repair/meta/retention chain, not the full game.

Latest proof:

- product gate:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_reward_feedback.md`
- screenshots:
  `build/captures/ui_rescue_blueprints_layout.png`,
  `build/captures/ui_rescue_offline_layout.png`,
  `build/captures/ui_rescue_card_choice.png`,
  `build/captures/ui_reward_gate_repair.png`,
  `build/captures/ui_reward_blueprint.png`,
  `build/captures/ui_reward_offline.png`
- readable zooms:
  `build/captures/ui_rescue_blueprints_layout_uizoom.png`,
  `build/captures/ui_rescue_offline_layout_uizoom.png`,
  `build/captures/ui_rescue_card_choice_uizoom.png`,
  `build/captures/ui_reward_blueprint_uizoom_cmp.png`,
  `build/captures/ui_reward_offline_uizoom_cmp.png`
- probes:
  `tmp/shard_blueprints_probe.py`, `tmp/offline_return_probe.py`,
  `tmp/rescue_probe.py`, `tmp/reward_feedback_probe.py`

This is still not a full product pass: the assembled UI and first repair chain
are readable and the first meta/retention loop exists, but final art polish,
reward timing polish, and broader retention/fun review are still required.

## Blocking Gaps

- Reward feedback exists, but transient floaters can still overlap one Blueprint
  detail line for a short frame; final timing/audio mix is not polished.
- Forge/Campfire now have world markers, but room-specific art is still
  placeholder-level.
- The current screenshot is a readable functional proof, not final art quality.
- `tools/devapi/voxelheim_play_test.py` still has a stale offline-unlock
  expectation; use the focused probes until updated.

## Last Known Good Evidence

- Native build: `cmake --build --preset native-debug --target game_seed`.
- Latest product gate:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_reward_feedback.md`.
- Design evidence: `gamedesign/projects/voxelheim/reviews/prototype_deconstruction_2026-06-17.md`
  and `gamedesign/projects/voxelheim/references/competitor_deconstruction_2026-06-17.md`.

## Next Priorities

1. Replace placeholder Forge/Campfire markers with polished generated room art.
2. Run a broader retention/fun critic pass on the first 5-minute loop.
3. Tune balance after the critic pass using the current probes as baselines.
4. Polish reward timing/audio mix after room art stops moving.
