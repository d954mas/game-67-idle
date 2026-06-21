# Project Status

## Current Goal

Review the first native playable slice for `Dragon Grove` (dragon-grove), an
original merge-3 dragon grove puzzle inspired by merge-game genre grammar
without copying brand, art, UI copy, maps, or economy.

## Current Runtime Surface

Native `game_seed` is the current prototype work surface:

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
```

Runtime entrypoint: `src/clean_seed_main.c`.

The engine submodule at `external/neotolis-engine` is read-only from this repo.
Reusable sidecar modules, tools, skills, and game code may be edited here.

## Current Gate

Task T0029 is in review. The implemented slice is one 5x5 Y-up logical grid,
one DevAPI-backed merge-ready action, visible reward/restore feedback, and a
blocked/no-merge state. This is a debug playable slice, not a product visual
pass: `nt_shape_renderer` is temporary debt, and final visuals still require
generated runtime art plus engine font/text rendering.

## Blocking Work

- None for the debug playable slice. Broad feature/content expansion is blocked
  until product visual/readability review accepts or replaces the debug visuals.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
node tools/taskboard/cli.mjs validate
cmake --build --preset native-debug --target game_seed
py -3.12 tools/dragon-grove/smoke.py
node tools/ai.mjs validate --review
```

## Last Known Good Evidence

- `py -3.12 tools/dragon-grove/smoke.py` PASS: runtime `dragon_grove`,
  `merge_count=3`, `restored_tiles=3`, level-2 reward, blocked state, `ui.tree`
  nodes, screenshot.
- Screenshot: `build/captures/dragon-grove-smoke.png`.
- `node tools/ai.mjs validate --review` PASS.

## Next Priorities

1. Product-review the first screen before adding systems/content.
2. Decide whether to replace debug shapes/text with generated runtime art and
   engine font rendering or close the experiment.
3. If continuing, decompose `src/clean_seed_main.c` before the next runtime
   feature.
