---
id: T0009
title: Skinned mesh proof performance trim
status: done
epic: E001
priority: P2
tags: [mine-cards, animation, 3d, performance, skeletal]
created: 2026-06-18
updated: 2026-06-17
---

## What

Bring the reusable skeletal mesh proof back under the v001 CPU skinning budget
or make the next renderer path explicit.

T0007 proved the `game_seed` path using `nt_skeletal_mesh_*`, Ozz model
matrices, and a reusable `tool` socket, but the first native measurement was:

```text
skeletal mesh proof: reusable CPU skin avg=0.685ms vertices=6675 joints=23 budget<=0.500ms
```

This is acceptable proof evidence, but not budget-clean production evidence.

Scope:

- reduce the proof mesh cost, likely by generating a lower-vertex runtime LOD
  for the first-screen actor;
- or prove that a small API/data change brings CPU skinning under budget;
- or file a GPU skinning renderer task with measurement evidence if CPU v001 is
  not worth optimizing further.

Out of scope:

- editing `external/neotolis-engine`;
- replacing the Mine Cards character art;
- adding equipment mechanics or inventory UI.

## Done when

- [x] A new native proof records reusable CPU skinning at `<= 0.500ms` average
      for the first-screen actor, or a GPU/LOD renderer follow-up is created
      with the measured blocker attached.
- [x] The proof still shows a KayKit/Quaternius-style character plus attached
      tool in `game_seed` or the Mining screen.
- [x] The chosen path is recorded in T0007/T0009 logs and `tasks/STATUS.md`.
- [x] `external/neotolis-engine` remains unchanged.

## Open questions

- Should the v0.01 actor use a lower-poly generated descriptor, a generated
  binary runtime mesh, or wait for a GPU skinning extension?

## Log

- 2026-06-18: Created from T0007 native proof. Evidence:
  `build/captures/mine_cards_t0007_skeletal_game_proof.json` reports
  `0.685ms` average reusable CPU skinning for `6675` vertices / `23` joints,
  above the `<= 0.500ms` v001 budget.
- 2026-06-18: Started with the smallest production-oriented fix: precompute
  `model_matrix * inverse_bind_matrix` once per joint per pose in the reusable
  CPU skinner instead of recomputing that matrix for every vertex influence.
- 2026-06-18: Budget-clean proof passed after precomputing per-joint skin
  matrices in `extensions/skeletal_animation/src/nt_skeletal_mesh_cpu.cpp`.
  Evidence: `cmake --build --preset native-debug --target game_seed`,
  `cmake --build --preset native-debug --target skeletal_mesh_contract_probe`,
  `build\tools\native-debug\skeletal_mesh_contract_probe.exe`, and
  `py -3.12 tmp\t0007_skeletal_game_proof.py` passed. Runtime proof report:
  `build/captures/mine_cards_t0007_skeletal_game_proof.json` now records
  `skeletal mesh proof: reusable CPU skin avg=0.162ms vertices=6675 joints=23
  budget<=0.500ms`. Screenshot:
  `build/captures/mine_cards_t0007_skeletal_game_proof.png`; zoom:
  `build/captures/mine_cards_t0009_skeletal_budget_actor_zoom.png`. Engine
  boundary check produced no diff under `external/neotolis-engine` except the
  local git warning about `C:\Users\ROG/.config/git/ignore`. Moved to review.
- 2026-06-18: All T0009 acceptance criteria are backed by current proof and
  validation. Archiving as done; no GPU/LOD follow-up is needed for the current
  first-screen actor budget.
