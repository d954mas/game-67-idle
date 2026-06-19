# T0027 Assault Walker Hero Asset Pass

## Result

The primary hero mech now uses a downloaded, licensed source model instead of
the previous weak hero body. The selected Poly Pizza `Mech Assault Walker` gives
the first screen a much stronger mech silhouette: cockpit, weapon arms, chunky
legs, and a brighter toy/plastic Roblox-like read.

This is not the final hero rig. It is the first correct asset-first baseline
for visual iteration.

## What Changed

- Downloaded and stored the selected source GLB and preview under
  `assets/source/models/poly_pizza/alimayo_arango/`.
- Added material-split extraction support with a license suffix so CC-BY assets
  are not mislabeled as CC0 runtime files.
- Added 13 Assault Walker static mesh parts to the native asset pack.
- Replaced the primary visible hero render path with the Assault Walker mesh
  parts, using Y-up placement and toy/plastic part colors.
- Added DevAPI screenshot coverage for the new hero in hangar and battle.

## Evidence

- Candidate/provenance note:
  `gamedesign/projects/mech-builder-battler/references/hero_mech_asset_sourcing_2026-06-20.md`.
- Native build:
  `cmake --build --preset native-debug --target game_seed`.
- DevAPI smoke:
  `py -3.12 tools/mech-builder-battler/devapi_playable_smoke.py 9124`.
- Smoke log:
  `build/logs/native_devapi_9124_20260620_003456_948.log`.
- Hangar screenshot:
  `build/captures/mech_t0027_assault_walker_hero_hangar_smoke.png`.
- Battle screenshot:
  `build/captures/mech_t0027_assault_walker_battle_smoke.png`.
- Motion screenshot:
  `build/captures/mech_t0027_assault_walker_motion_smoke.png`.
- Strict product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T00-36-10_desktop-assault-walker-hero.md`.

## Remaining Gap

- Add authored/rigged movement so the strong silhouette also moves like a juicy
  mech instead of only being a strong static import.
- Add final credits/attribution UI or document before any public build.
- Keep improving world materials, studs-like surfaces, lighting, shadows, and
  post effects around the new hero so the whole scene matches the stronger
  asset bar.
