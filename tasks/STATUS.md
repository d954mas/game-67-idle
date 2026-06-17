# Project Status

## Current Goal

`Voxelheim` game iteration is stopped by lead direction on 2026-06-17. Current
work is **process review and pipeline improvement**: understand why the
development loop accepted weak UI/product evidence, then fix the workflow before
another game iteration. Live-state coverage, startup matrix generation, runtime
edge audits, and semantic/style asset rejection gates are complete.

Stopped prototype target: **Voxelheim: Frost Keep Rebuilder**, a native idle RPG
where auto-combat earns Gold + Frost Blocks and repairs the Frost Keep room by
room.

Primary source of truth:

- `gamedesign/projects/voxelheim/gdd.md`
- `gamedesign/projects/voxelheim/data/rescue_loop.json`
- `gamedesign/projects/voxelheim/visual/ui_ux_rescue_spec.md`
- `gamedesign/projects/voxelheim/references/competitor_deconstruction_2026-06-17.md`
- `gamedesign/projects/voxelheim/reviews/prototype_deconstruction_2026-06-17.md`
- `gamedesign/projects/voxelheim/reviews/process_retrospective_2026-06-17.md`

## What's Built

The native build in `src/voxelheim_main.c` proves a first rescue loop:
auto-combat -> Gold/Frost Blocks -> Gate/Forge/Campfire repairs -> rune choice
-> Avalanche Reset/Frost Shards -> Frost Blueprints/offline return. Details and
limits are in `gamedesign/projects/voxelheim/reviews/process_retrospective_2026-06-17.md`.

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

Reusable process fixes completed:

- matrix template:
  `gamedesign/knowledge/live_state_acceptance_matrix.md`
- Voxelheim fixture:
  `gamedesign/projects/voxelheim/visual/live_state_acceptance_matrix.md`
- product gate state coverage:
  `tools/product_gate/review.mjs` supports `--require-state`,
  `--covered-state`, and `--not-covered-state`.
- new-prototype startup matrix:
  `tools/game_context/new_prototype.mjs` creates
  `visual/live_state_acceptance_matrix.md/json`, and
  `tools/game_context/iteration_context.mjs` treats missing matrix as startup
  gate debt.
- runtime UI edge/chroma audit:
  `tools/assets/audit_runtime_ui_edges.py` audits source PNGs and runtime
  screenshot crops for purple/key-color fringe.
- asset semantic/style audit:
  `tools/assets/audit_asset_semantic_style.mjs` rejects generated assets that
  mean the wrong thing, mix styles, or fail composability before crop planning.

Latest game proof index:
`gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_reward_feedback.md`.
Latest regression proof:
`gamedesign/projects/voxelheim/reviews/live_ui_regression_2026-06-17.md`.
This is still not a full product pass.

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
- Forge/Campfire room art is placeholder-level.
- Current screenshots are functional proof, not final art quality.
- `tools/devapi/voxelheim_play_test.py` still has a stale offline-unlock
  expectation; use the focused probes until updated.

## Last Known Good Evidence

- Native build: `cmake --build --preset native-debug --target game_seed`.
- Product gate:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_reward_feedback.md`
- Live UI regression review:
  `gamedesign/projects/voxelheim/reviews/live_ui_regression_2026-06-17.md`
- Process retrospective:
  `gamedesign/projects/voxelheim/reviews/process_retrospective_2026-06-17.md`

## Next Priorities

1. Decide whether to archive the stopped Voxelheim prototype or reopen a new
   polish/fun-review cycle.
2. Keep the remaining Voxelheim review tasks (`T0001`, `T0005`, `T0006`) as
   game-iteration review items until that decision is made.
