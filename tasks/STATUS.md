# Project Status

## Current Goal

Post-prototype pipeline cleanup after `Mech Builder Battler`
(`mech-builder-battler`). The mech prototype is stopped; do not continue game
implementation, content, web/mobile export, or frontend/runtime work without a
fresh explicit request.

Project wiki: `gamedesign/projects/mech-builder-battler/README.md`.
Latest control note:
`gamedesign/projects/mech-builder-battler/references/mobile_control_patterns_2026-06-19.md`.

## Current Runtime Surface

Native `game_seed` is the work surface:

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
```

Runtime entrypoint: `src/clean_seed_main.c`.

The engine submodule at `external/neotolis-engine` is read-only from this repo.
Reusable sidecar modules, tools, skills, and game code may be edited here.

## Current Gate

The current gate is reusable pipeline quality, not game feature progress. The
session review identified these priority fixes: hard visual/runtime invariants,
OKF-style shared asset catalog, legal downloaded asset intake, texture
generation/tiling workflow, lead-rejection gates, split smoke tests, and
post-prototype status cleanup.

Fixed so far: OKF-style shared asset catalog, legal asset intake helper,
texture tiling audit workflow, and strict lead-rejection closeout guard.

Latest audit:
`docs/ai-pipeline/mech-session-pipeline-audit-2026-06-20.md`.

Historical prototype baseline: sourced Assault Walker hero, CC0 Quaternius
enemy, Sentinel Mech side-pad display, stylized-studs world texture, and CC0
Kenney station props with a plastic shader. The prototype stopped because the
mech still read as a static/plastic figure, animation/material quality was not
high enough, battle pacing was too fast, arena framing was weak, and controls
felt suspect.

## Next Priorities

1. Split native smoke tests into movement, visual framing, combat pacing,
   reward loop, and asset-load proofs before the next playable game iteration.
2. Clean/archive stopped-prototype taskboard noise before starting a new game.
