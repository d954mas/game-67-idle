# Project Status

## Current Goal

Build the `Backrooms Liminal` (backrooms-liminal) native-first prototype:
one first-person 3D corridor, fuse pickup, return-to-exit, fear/battery pressure,
and strict visual/player-read proof before any broader maze/content expansion.

## Blocking Work

- No runtime implementation blocker is known yet; the next blocker should come
  from the first GDD/reference/fake-shot pass.

## Non-blocking Debt

- None recorded for this prototype yet.

## Current Gate

First playable native gate for backrooms-liminal: `data/core_loop.json`,
`reviews/first_slice_visual_gate.md`, native build, DevAPI smoke, first-screen
screenshot, after-fuse screenshot, readability zoom, and strict product gate.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
cmake --build --preset native-debug --target game_seed
py -3.12 tools/devapi/smoke.py
node tools/taskboard/cli.mjs validate
```

## Last Known Good Evidence

- `tmp/prototype_startup_gate_context.json` after kickoff.
- `gamedesign/projects/backrooms-liminal/reviews/first_slice_visual_gate.md` is the
  first-slice visual/product gate template and must be filled before broad
  runtime work; it names the optional visual critic packet command.
- `gamedesign/projects/backrooms-liminal/visual/live_state_acceptance_matrix.json`
  is the machine-readable state coverage matrix for product gates.

## Next Priorities

1. Replace the clean seed screen with the scoped Backrooms 3D runtime.
2. Update DevAPI smoke per-game expectations for `backrooms.*` UI/state.
3. Build native debug and run smoke/capture.
4. Run readability/product gates and freeze expansion if the screenshot fails.
