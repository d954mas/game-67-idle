# Project Status

## Current Goal

Review and iterate the `Backrooms Liminal` (backrooms-liminal) native-first
prototype: the first-person 3D corridor slice now exists with fuse pickup,
return-to-exit, fear/battery pressure, and strict visual/player-read proof.

## Blocking Work

- No runtime implementation blocker is known. The first slice is in task review;
  expansion should wait for lead/playtest feedback or a new narrow task.

## Non-blocking Debt

- AI profile guard is red because the current scope has unresolved failed
  profile records. Do not use this session profile as review evidence until it
  is cleaned up or explicitly accepted as advisory-only.

## Current Gate

First playable native gate for backrooms-liminal: `data/core_loop.json`,
`reviews/first_slice_visual_gate.md`, native build, DevAPI smoke, first-screen
screenshot, after-fuse screenshot, readability zoom, strict product gate, and
slice hygiene evidence.

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
- `build/captures/backrooms_first_screen.png` shows the first player-facing
  corridor/HUD state.
- `build/captures/backrooms_after_fuse.png` shows fuse feedback, powered exit,
  and the silhouette stress state.
- `build/captures/backrooms_first_screen_uizoom.png` is the readability montage.
- `gamedesign/projects/backrooms-liminal/reviews/product_read_gate_latest.json`
  is a strict desktop product gate PASS for the first slice.
- `build/captures/backrooms_slice_hygiene.md` is WARN only because profiler
  review evidence is advisory/unusable, not because gameplay validation failed.

## Next Priorities

1. Let the lead/playtest judge whether the first corridor is scary and
   interesting enough for the next slice.
2. If accepted, create a new narrow task for route uncertainty/maze variation,
   enemy pressure, or audio/lighting polish.
3. If rejected visually, freeze content expansion and improve the first-screen
   mood/readability before adding routes or systems.
