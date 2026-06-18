# Project Status

## Current Goal

Plan and prepare `Mine Cards` v0.01 as a block mining idle RPG: Melvor-like
economy depth, but a reduced Capybara Go-like first screen with a fixed
top character/action stage and dense idle mechanics below.

Active design wiki:

`gamedesign/projects/mine-cards/`

Active epic:

`tasks/epics/E001-mine-cards-v0-01-mining-foundation.md`

Primary tasks:

- `tasks/active/T0001-mine-cards-mining-v0-01-first-slice.md`
  is in doing. The native Mining first slice exists in `src/clean_seed_main.c`,
  but lead rejected the PC build for tiny/unclear UI and then for missing
  player focus. Current fix gate is PC readability/focus, not more mechanics.
- `tasks/active/T0008-production-equipment-source-sheet.md`
  is in review with accepted 12-item runtime equipment sprites for later UI.
- `tasks/active/T0010-custom-mine-cards-voxel-miner-source-pack.md`
  is a gated `idea` with custom miner source/provenance prep for after T0001.

## Current Direction

The old card-crawler concept is archived. The current direction is:

```text
Top miner action stage -> lower idle mechanics board -> deepen Mining -> add resource consumer -> add combat later
```

Key refs live under `gamedesign/projects/mine-cards/visual/` and
`gamedesign/projects/mine-cards/references/`.

## Current Runtime Surface

Native `game_seed` is now the Mine Cards first-screen work surface:

```powershell
cmake --build --preset native-debug --target game_seed
build/game_seed/native-debug/game_seed.exe --devapi 9123
```

Runtime entrypoint: `src/clean_seed_main.c`.

Coordinate convention: game-space UI and 2D gameplay coordinates are Y-up.
DevAPI UI bounds remain top-left screen coordinates and are converted at the
runtime boundary.

## Current Gate

Status: `T0001 PC UI scale/focus rescue implemented; awaiting lead review`.

Current red/yellow gates:

- Prior stage/board evidence is recorded in T0001 and the Mine Cards reviews
  folder, but it is superseded by the PC UI-scale rejection.
- Current rejection/fix evidence:
  `gamedesign/projects/mine-cards/reviews/t0001_ui_scale_rejection_2026-06-18.md`;
  latest focus screenshot:
  `build/captures/mine_cards_focus_v002_1280x720.png`.
- Native screenshot automation uses game-local `game.capture.framebuffer`.
- UI scale fix pass 1 is built: `nt_ui_scale` is connected in
  `src/clean_seed_main.c`, default window is `1280x720`, and evidence is
  `build/captures/mine_cards_nt_ui_scale_1280x720_window.png`.
- Focus pass 2 is built: future skills are no longer pseudo-buttons, the
  inactive bottom `SKILLS` pseudo-tab is removed, and unavailable upgrade/copper
  actions are disabled visually and in DevAPI.
- Old compact/responsive pass evidence is historical only.
- Old responsive compact UI product-read gate passed, but is no longer
  sufficient because the runtime did not use `nt_ui_scale`.
- Core mining motion gate passed: `reviews/core_moment_mining_v004_2026-06-18.md`.
  It proves miner -> rock -> hit FX -> reward in motion; it does not claim
  final custom character art.
- T0008 equipment art remains review-only and is not integrated into the first
  screen.

## Blocking Work

- First-upgrade economy is locked and T0002 is done:
  `Surface Stone` start, `Copper Vein` at Mining Lv2, Copper Vein fixed
  `coins +1`, Copper Pickaxe cost `stone 6` + `copper_ore 32` + `coins 32`.
- Public-safe 3D miner source path is locked and T0003/T0005/T0006 are done:
  KayKit `Rig_Medium_Tools.glb`, kit `Pickaxe_Stone.gltf`, Ozz archives under
  `visual/skeletal_spike/`, and reusable `extensions/skeletal_animation/`.
  Keep it outside `external/neotolis-engine`.
- Archived T0007/T0009 native proof uses `nt_skeletal_mesh_*` and a reusable
  `tool` socket. CPU skinning is budget-clean: `0.162ms` vs `<=0.500ms`.
- Old PSD art is not engine-ready and includes public-safety/IP risks.

## Last Good Evidence

- Current rejection/fix packet:
  `gamedesign/projects/mine-cards/reviews/t0001_ui_scale_rejection_2026-06-18.md`.
- Prior T0001 review packet/evidence:
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_packet_2026-06-18.md`.
- Core motion proof:
  `gamedesign/projects/mine-cards/reviews/core_moment_mining_v004_2026-06-18.md`.

## Next Priorities

1. Lead-review T0001 PC readability/focus on the actual native window; if
   rejected, name one remaining axis before adding mechanics.
2. Keep T0008 available as later equipment/UI prep, but do not integrate it
   into the first screen before T0001 is accepted.
3. Keep T0010 as gated character-source prep; promote it only after T0001 is
   accepted or the lead explicitly chooses custom character production first.
4. Keep feature expansion frozen until the UI scale/readability gate is fixed.
