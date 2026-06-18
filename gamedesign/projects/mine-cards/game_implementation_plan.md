# Mine Cards Implementation Plan

Status: `not ready for implementation`.

This plan captures the likely first native slice after the Melvor-like pivot.
Do not start runtime code from this file alone; first produce and review the
Mining v0.01 fake shot and final public-safe art direction.

## First Native Slice

Build one playable Mining activity screen:

1. Portrait game view centered in the native PC window.
2. Original modular 3D blocky miner and one visible mine node.
3. Node picker with `Surface Stone`, `Copper Vein`, and one locked/deeper node.
4. A large progress bar that completes Mining ticks.
5. Reward log for ore, coins, Mining XP, and mastery XP.
6. Optional rare `Geode` bonus event.
7. Pickaxe upgrade panel with clear cost and before/after speed.
8. Worn Pickaxe to Copper Pickaxe visual swap.
9. Simple transform-driven idle/mining animation.
10. Unlock or preview the next node after the first upgrade.

## Required Design Inputs Before Code

- Accepted fake shot from `visual/fake_shot_brief_melvor_blocky.md`.
- Accepted 3D character direction from `visual/3d_character_direction.md`.
- Current mismatch list between fake shot and native screenshot direction.
- First-slice values reviewed in `data/parameters.json`.
- Future-system boundaries reviewed in `systems_foundation.md`.
- Runtime asset plan from `visual/art_inventory.md`.
- First modular miner source path from `visual/runtime_asset_plan_v001.md`:
  procedural/blockout GLB mesh parts.

## Likely Files

- `src/clean_seed_main.c` or successor native game entrypoint.
- `src/devapi/` only for proof automation if needed.
- `assets/` or generated atlas manifest after asset-pipeline planning.
- `gamedesign/projects/mine-cards/data/*.json`.

## Validation Gates

- Native build/run command identified.
- Native screenshot of the Mining activity screen.
- Native proof that the miner has visible idle/mining motion.
- UI readability zoom crop for HUD/progress/upgrade text.
- Product gate with visual, teachability, and game-loop checks.
- Core-loop critic: can the player state what is running, what grows, what can
  be upgraded, and why to continue for another five minutes?

## Explicit Non-Goals

- No web prototype.
- No card run implementation.
- No combat implementation.
- No broad skill tree implementation.
- No premium/gem economy.
- No offline progress until the first active screen works.
- No final art claim from raw PSD comps.
