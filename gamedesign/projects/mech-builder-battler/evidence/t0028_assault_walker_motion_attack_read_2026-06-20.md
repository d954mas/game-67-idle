# T0028 Assault Walker Motion And Attack Read

## Result

The sourced Assault Walker no longer reads as only a static imported mesh in
battle. The hero now has visible whole-body motion response, cannon target
beams, muzzle/weapon charge, stomp rings, vent lines, combat material glow, and
slightly longer cannon flash so screenshots reliably show action before the
reward overlay.

This is still a runtime VFX/pose pass, not a full rigged animation import.

## What Changed

- Added whole-body battle squash, lean, pitch, recoil, and slight stance spread
  to the Assault Walker mesh render path.
- Added combat glow on selected green/glass/dark source material parts during
  fire/heat/rocket moments.
- Added Assault Walker-specific battle VFX: stomp rings, foot sparks, muzzle
  charge, target beams, and vent streaks.
- Extended cannon flash from `0.16` to `0.26` seconds so visual proof catches
  the attack state more reliably.
- Added early T0028 DevAPI captures before the fast first battle reaches the
  reward overlay.

## Evidence

- Native build:
  `cmake --build --preset native-debug --target game_seed`.
- DevAPI smoke:
  `py -3.12 tools/mech-builder-battler/devapi_playable_smoke.py 9124`.
- Smoke log:
  `build/logs/native_devapi_9124_20260620_004622_832.log`.
- Attack screenshot:
  `build/captures/mech_t0028_assault_walker_cannon_recoil_smoke.png`.
- Motion screenshot:
  `build/captures/mech_t0028_assault_walker_early_motion_smoke.png`.
- Strict product gate:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T00-47-10_desktop-assault-walker-attack-read.md`.

## Remaining Gap

- The hero still needs true authored limb animation or a rigged/part-separated
  import pass to reach final mech fantasy quality.
- Lighting, shadows, and material polish should continue around this stronger
  movement baseline.
- The current proof is native PC only; web/mobile export remains deferred until
  explicitly approved.
