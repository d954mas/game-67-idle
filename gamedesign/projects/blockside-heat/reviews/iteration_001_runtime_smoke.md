# Iteration 001 Runtime Smoke

## Goal

Prove the first `Blockside Heat` native runtime slice is no longer just a clean
seed: real mesh pack, playable state loop, DevAPI state changes, and engine text
HUD wiring exist.

## Build

- Command: `cmake --build --preset native-debug`
- Result: pass
- Output: `build/game_seed/native-debug/game_seed.exe`
- Pack: `build/game_seed/native-debug/assets/blockside_heat.ntpack`

## Runtime Smoke

Command pattern:

```powershell
Start-Process build/game_seed/native-debug/game_seed.exe `
  --devapi 9137 --window-size 1280x720
python tools/devapi/devapi_cli.py 9137 game.state
python tools/devapi/devapi_cli.py 9137 game.action.pickup_package
python tools/devapi/devapi_cli.py 9137 game.action.complete_job
python tools/devapi/devapi_cli.py 9137 ui.tree
```

Observed:

- `game.state`: `runtime=blockside_heat`, `job_stage=start`, `cash=0`,
  `wanted_level=0`.
- `game.action.pickup_package`: `job_stage=package_collected`,
  `package_collected=true`, `wanted_level=1`.
- `game.action.complete_job`: `job_stage=complete`,
  `package_delivered=true`, `cash=75`, `wanted_level=0`.
- `ui.tree`: root screen plus `hud.job`, `hud.cash`, and `action.primary`
  nodes are registered.

Saved evidence:

- `tmp/blockside-heat/state-start.json`
- `tmp/blockside-heat/state-pickup.json`
- `tmp/blockside-heat/state-complete.json`
- `tmp/blockside-heat/ui-tree.json`
- `tmp/blockside-heat/state-start-rerun.json`
- `tmp/blockside-heat/state-pickup-rerun.json`
- `tmp/blockside-heat/state-complete-rerun.json`
- `tmp/blockside-heat/ui-tree-rerun.json`

Rerun after removing unused seed shape-renderer plumbing:

- Command: `cmake --build --preset native-debug`
- Result: pass
- DevAPI result: `job_stage=complete`, `package_delivered=true`, `cash=75`,
  `wanted_level=0`.

## Screenshot Gate

Resolved for automation by adding the game-local DevAPI endpoint
`game.capture.framebuffer`, which writes the next rendered native backbuffer as
P6 PPM. `tools/devapi/devapi_client.py` converts that PPM to PNG.

Older OS capture paths failed in the Codex desktop execution context:

- `tools/devapi/capture_window.py --process-id <pid>` failed with
  `BitBlt failed with Win32 error 6`.
- `tools/devapi/capture_screen.ps1 -ProcessId <pid>` failed with
  `CopyFromScreen ... The handle is invalid`.

Current screenshot evidence:

- `tmp/blockside-heat/first-native-screenshot.png`: first successful
  framebuffer screenshot; product gate failed because objects floated without a
  readable street/ground base.
- `tmp/blockside-heat/first-native-screenshot-iter4.png`: city-base iteration;
  product gate moved to review because the intersection now reads, but action
  direction and density still need work.
- `tmp/blockside-heat/pickup-stress-screenshot.png`: retry/stress evidence;
  automation reached `caught` before a stable package-collected screenshot.
- `tmp/blockside-heat/job-complete-screenshot.png`: reward evidence with
  `cash=75`, `job_stage=complete`, and next-job lock text.
- `tools/blockside-heat/capture_states.py`: repeatable native capture script
  for first, pickup/stress, reward, and UI tree evidence.
- `tmp/blockside-heat/capture-states-report.json`: latest scripted report.
- `tmp/blockside-heat/first-native-screenshot-latest.png`
- `tmp/blockside-heat/pickup-stress-latest.png`
- `tmp/blockside-heat/job-complete-latest.png`

## Director Review

What is better:

- The prototype now has a real native game identity, a game-specific asset pack,
  and a small job state loop instead of the clean seed cycle button.
- Runtime uses pulled low-poly GLB assets and a packed engine font resource.
- DevAPI can prove the job loop changes state.
- Native framebuffer screenshot capture now works in-process through DevAPI.
- The first screen now has a readable ground/road intersection after adding the
  project-owned `blockside_city_base` mesh asset.

What is weak:

- Visual quality is review-only, not a strict pass: the city is sparse and the
  package/route target needs stronger world-space direction.
- Strict product pass is still open: the package/route target needs a stronger
  world-space affordance.
- Physics is arcade state movement, not a full vehicle/rigid-body simulation.
- Weapon is a simple stun proof, not a full combat system.

Next:

1. Add a route/package marker or pickup affordance and recapture first screen.
2. Improve vehicle/NPC behavior only after strict first-screen product gate
   passes or lead accepts the visual debt.
