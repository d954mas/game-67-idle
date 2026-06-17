# Project Status

## Current Goal

`Voxelheim` game iteration is stopped by lead direction on 2026-06-17. Current
work is **process review and pipeline improvement**: understand why the
development loop accepted weak UI/product evidence, then fix the workflow before
another game iteration.

The stopped prototype target is **Voxelheim: Frost Keep Rebuilder**: a native
idle RPG where auto-combat earns Gold + Frost Blocks and the player visibly
repairs the Frost Keep room by room.

Primary source of truth:

- `gamedesign/projects/voxelheim/gdd.md`
- `gamedesign/projects/voxelheim/data/rescue_loop.json`
- `gamedesign/projects/voxelheim/visual/ui_ux_rescue_spec.md`
- `gamedesign/projects/voxelheim/references/competitor_deconstruction_2026-06-17.md`
- `gamedesign/projects/voxelheim/reviews/prototype_deconstruction_2026-06-17.md`
- `gamedesign/projects/voxelheim/reviews/process_retrospective_2026-06-17.md`

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
- live UI hotfix removes the reported Gate CTA overlap/muddy text state, cleans
  the purple-edge source contamination in `assets/raw/button.png`, and replaces
  the Blocks HUD badge with an icy block icon.

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

**Game iteration is no longer expanding. Process-review gate is active.**
Repair-chain + Avalanche Reset + Frost Blueprints + Offline Return product-read
gates passed for their captured states, but they are not full-game or full-UI
acceptance. The live UI text/edge hotfix is proof of a regression fix, not a
strict final product pass.

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
  `build/captures/ui_reward_offline_uizoom_cmp.png`,
  `build/captures/ui_text_live_overlap_fix_uizoom.png`,
  `build/captures/ui_text_live_overlap_fix_uizoom_cmp.png`
- probes:
  `tmp/shard_blueprints_probe.py`, `tmp/offline_return_probe.py`,
  `tmp/rescue_probe.py`, `tmp/reward_feedback_probe.py`,
  `tmp/ui_text_overlap_probe.py`, `tmp/audit_cta_purple.py`

This is still not a full product pass: the assembled UI and first repair chain
are readable and the first meta/retention loop exists, but final art polish,
reward timing polish, and broader retention/fun review are still required.

## Blocking Gaps

- CTA text/edge hotfix has live screenshot proof, but
  `tools/devapi/ui_readability.py` still warns on CTA stroke thickness; do not
  call this a final UI product pass until that is resolved or the metric is
  improved.
- Any future UI acceptance must include the exact live state from
  `tmp/ui_text_overlap_probe.py`: post-offline collect, Blueprints visible, Gate
  CTA affordable, combat/floaters active, HUD visible, and a CTA purple-edge
  audit.
- Reward feedback exists, but final timing/audio mix is not polished.
- Forge/Campfire now have world markers, but room-specific art is still
  placeholder-level.
- The current screenshot is a readable functional proof, not final art quality.
- `tools/devapi/voxelheim_play_test.py` still has a stale offline-unlock
  expectation; use the focused probes until updated.

## Last Known Good Evidence

- Native build: `cmake --build --preset native-debug --target game_seed`.
- Latest product gate:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_reward_feedback.md`.
- Latest live UI regression review:
  `gamedesign/projects/voxelheim/reviews/live_ui_regression_2026-06-17.md`.
- Design evidence: `gamedesign/projects/voxelheim/reviews/prototype_deconstruction_2026-06-17.md`
  and `gamedesign/projects/voxelheim/references/competitor_deconstruction_2026-06-17.md`.

## Next Priorities

1. T0013: add reusable live-state UI acceptance matrix gate for all future games.
2. T0012: require state-coverage tags in product gates.
3. T0011: clean Voxelheim source-of-truth drift in `AGENTS.md` and status.
4. After process fixes, decide whether to archive this prototype or reopen a
   new polish/fun-review cycle.
