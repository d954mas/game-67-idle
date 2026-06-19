# T0029 Arena Dressing Depth Pass

## Result

The battle arena and hangar now read less like a flat test plate. The world has
edge rails, corner pylons, pad energy rings, colored rail accents, and extra
block detailing around the mech. The goal was to add toy/Roblox-like depth
without hiding the Assault Walker, combat targets, or HUD.

This is still runtime-authored block dressing, not a final sourced world asset
set.

## What Changed

- Added block pylons with studs to arena/hangar corners.
- Added edge rails and color accents to frame the playable floor.
- Added pad energy rings for the hangar and battle pad.
- Added T0029 hangar and battle screenshots to DevAPI smoke.
- Tuned ring opacity, rail color, pylon height, and rail placement after visual
  inspection so dressing supports the mech instead of overpowering it.

## Evidence

- Native build:
  `cmake --build --preset native-debug --target game_seed`.
- DevAPI smoke:
  `py -3.12 tools/mech-builder-battler/devapi_playable_smoke.py 9124`.
- Smoke log:
  `build/logs/native_devapi_9124_20260620_005725_581.log`.
- Battle screenshot:
  `build/captures/mech_t0029_arena_dressing_battle_smoke.png`.
- Hangar screenshot:
  `build/captures/mech_t0029_arena_dressing_hangar_smoke.png`.
- Strict product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T00-58-10_desktop-arena-dressing-depth.md`.

## Remaining Gap

- Later world art should replace runtime-authored block dressing with sourced
  or generated world asset families where useful.
- The right edge pylon is close to the HUD in the battle camera; later layout
  polish should move or gate it per viewport.
- Lighting/shadow/material polish still needs a dedicated pass.
