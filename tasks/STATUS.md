# Project Status

## Current Goal

Build the `Voxelheim` first playable slice **"Frost Keep Approach"** — a casual
action-RPG screen — to the LOCKED Theme-A "Bright Roblox" fake-shot direction
(`gamedesign/projects/voxelheim/visual/fake_shot_first_screen.png`).
Visual-first freeze: do not expand systems/state/content until the first screen
passes the strict visual gate.

## Current Playable Path

P0 art (T0002) → P1 readable first screen + visual gate (T0003) → P2-P4 casual
core loop (T0004). Roadmap: `gamedesign/projects/voxelheim/game_implementation_plan.md`.

## Blocking Work

- T0002 (art) is the next actionable: art bible + Bright Roblox source sheets via
  **agy**, cut/audited via `generated-game-ui-assets`. T0003/T0004 gated behind it.

## Non-blocking Debt

- Native build/run/screenshot commands for a new screen not yet confirmed
  (discover during T0003).

## Current Gate

P1 **strict visual gate** (T0003): the native first screen must reach the
Theme-A fake-shot direction (`node tools/ai.mjs gate --visual-strict`) before any
gameplay systems, plus the teachability gate (a newcomer reads goal + action in
~10s).

## Required Validation

```powershell
node tools/taskboard/cli.mjs validate
node tools/ai.mjs gate --visual-strict ...   # at P1 (T0003)
```

## Last Known Good Evidence

- Theme A locked: `gamedesign/projects/voxelheim/visual/fake_shot_first_screen.png`
  (3 themed fake shots generated via agy; A chosen). Rejected directions:
  `visual/fake_shot_prompts.md`.
- `gdd.md` (slice + locked visual direction) + `game_implementation_plan.md`.
- Headless image-gen path: `delegated-image-generation` skill (agy).

## Next Priorities

1. T0002 — write `visual/art_bible.md`; generate Bright Roblox source sheets via
   agy; cut/audit via `generated-game-ui-assets`; build the atlas.
2. T0003 — readable first screen with real sprites; confirm native build/run
   commands; screenshot-vs-fake-shot mismatch list; pass `--visual-strict`.
3. T0004 — casual core loop (move/fight/clear/loot/level) once the gate passes.
