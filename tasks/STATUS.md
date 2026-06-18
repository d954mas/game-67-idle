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
  is in review. The native Mining first slice exists in `src/clean_seed_main.c`;
  compact UI, core motion, live-state matrix, stress, and strict gates pass.
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

Status: `T0001 native Mining first slice ready for lead review`.

Current red/yellow gates:

- Prior stage/board evidence is recorded in T0001 and the Mine Cards reviews
  folder.
- Slice hygiene is an explicit snapshot warning, not a strict fail:
  `build/captures/mine_cards_t0001_slice_hygiene_snapshot.md`.
- Native screenshot automation uses game-local `game.capture.framebuffer`.
- The UI pack now includes generated icons, stage pieces, and sprite/FX proof.
  Blank UI kit source is accepted; slice9/decor work is still pending.
- The saved equipment sheet with shadow problems is recorded as reference/probe
  material only; T0008 has accepted 12-item runtime sprites and is in lead
  review, not integrated into the native Mining first screen.
- Responsive compact UI product-read gate passed:
  `reviews/product_gate_compact_ui_v003_2026-06-18.md`.
- Core mining motion gate passed: `reviews/core_moment_mining_v004_2026-06-18.md`.
  It proves miner -> rock -> hit FX -> reward in motion; it does not claim
  final custom character art.
- Visual director review:
  `gamedesign/projects/mine-cards/reviews/visual_director_review_stage_rescue_v008_2026-06-17.md`.
- Matrix coverage is current for the first-screen states and 720x480 stress.
- Art-source rescue packet:
  `art_requests/mine-cards-stage-ui-family-v001.json`; accepted icons, stage
  background, sprite/FX, blank UI kit, and compact UI kit sources are recorded
  there.

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

- Base design/economy evidence lives in
  `gamedesign/projects/mine-cards/` and T0002/T0003 logs.
- Ozz/KayKit skeletal evidence lives in T0005/T0006 and the visual wiki.
- Latest responsive full-screen proof: `build/captures/mine_cards_compact_ui_v003_*`.
- Latest motion proof: `build/captures/mine_cards_core_moment_v004_sheet.png`
  and `build/captures/mine_cards_core_moment_v004.gif`.
- Latest responsive product gate:
  `gamedesign/projects/mine-cards/reviews/product_gate_compact_ui_v003_2026-06-18.md`.
- Latest core-moment product gate:
  `gamedesign/projects/mine-cards/reviews/core_moment_mining_v004_2026-06-18.md`.
- Latest live-state proof:
  `build/captures/mine_cards_live_state_v003_state.json`.
- Lead-review packet:
  `gamedesign/projects/mine-cards/reviews/t0001_lead_review_packet_2026-06-18.md`.
- T0001 acceptance audit:
  `gamedesign/projects/mine-cards/reviews/t0001_acceptance_audit_2026-06-18.md`.
- Next-slice decision packet:
  `gamedesign/projects/mine-cards/reviews/next_slice_decision_packet_2026-06-18.md`.
- T0008 equipment acceptance audit:
  `gamedesign/projects/mine-cards/reviews/t0008_equipment_art_acceptance_audit_2026-06-18.md`.
- T0007 renderer contract:
  `extensions/skeletal_animation/docs/skinned_mesh_renderer_contract_v001.md`.
- T0007 native proof: `build/captures/mine_cards_t0007_skeletal_game_proof.png`;
  budget zoom: `build/captures/mine_cards_t0009_skeletal_budget_actor_zoom.png`.
- Custom miner source packet:
  `gamedesign/projects/mine-cards/visual/custom_voxel_miner_source_packet_v001.md`.

## Next Priorities

1. Lead-review T0001 using the latest screenshots/GIF and decide accept vs one
   more visual/product pass.
2. Keep T0008 available as later equipment/UI prep, but do not integrate it
   into the first screen before T0001 is accepted.
3. Keep T0010 as gated character-source prep; promote it only after T0001 is
   accepted or the lead explicitly chooses custom character production first.
4. If rejected, keep feature expansion frozen and fix only the cited visual or
   product-read issue.
