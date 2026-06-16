# Project Status

## Current Goal

`Voxelheim` first playable slice **"Frost Keep Approach"** — a bright casual
action-RPG — is a **RELEASE-CANDIDATE**. Final critic (round 3) verdict: clears
the polished/teachable/good-looking casual-RPG prototype bar (Visual 8.5,
UI/HUD 9, Teachability 9; no blockers). Proof:
`gamedesign/projects/voxelheim/visual/proof/release_candidate.png`.

## What's Built (verified)

Tap-to-move hero, 3 ice-goblins with auto-combat + target ring, XP/level-up
(juicy), reach the Frost Keep portal → win; FTUE (3 beats); persistent state
(v2 + migration); real agy-generated sprite art through `nt_sprite_renderer`.
`voxelheim_play_test.py` = 19/19. Entry: `src/voxelheim_main.c`.

## Build / Run / Shoot

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
py -3.12 tools/devapi/shoot_voxelheim.py build/captures/x.png 9123
py -3.12 tools/devapi/voxelheim_play_test.py 9123
# atlas (only if assets change): build/voxelheim_packer/build_voxelheim_packs.exe build/voxelheim
#   then copy build/voxelheim/voxelheim.ntpack -> assets/voxelheim.ntpack
```

## Current Gate

Release-candidate bar: PASSED (final critic round 3). Two full player+critic
rounds + a confirmation pass drove the iteration.

## Non-blocking Debt / Deferred (not gating)

- Bolder flying dragon over the mountains (agy generation throttled at finish;
  background already has a small dragon).
- Deeper path perspective / background parallax.

## Last Known Good Evidence

- `gamedesign/projects/voxelheim/visual/proof/` — p1_first_screen, p5_integrated,
  p6_release_candidate, release_candidate.png.
- Commits: scaffold → P1 (`cfb6f4c`) → core loop (`078be64`) → polish (`e6232b7`)
  → composable-asset integration (`d78790f`) → final polish (`3f36ded`) → RC (`b7e01e4`).
- Image-gen + composable-asset rules: `.codex/skills/delegated-image-generation`
  + `gamedesign/projects/voxelheim/visual/art_bible.md`.

## Next Priorities (optional, post-RC)

1. Add the bolder dragon + path depth when agy generation recovers.
2. Audio pass (SFX/music) for more juice.
3. Expand beyond the first slice (more regions/enemies) if continuing.
