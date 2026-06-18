# Project Status

## Current Goal

Review and iterate the `Backrooms Liminal` (backrooms-liminal) native-first
prototype: the first-person 3D corridor slice now exists with fuse pickup,
return-to-exit, fear/battery pressure, route instability, stalker pressure, and
strict visual/player-read proof.

## Blocking Work

- No runtime implementation blocker is known. T0001 and T0002 are in review;
  expansion should wait for lead/playtest feedback or a new narrow task.

## Non-blocking Debt

- Global AI profile review confidence is still broken by older unresolved failed
  records. The T0002 current scope guard reported usable, but slice hygiene
  still treats profiler evidence as advisory-only.

## Current Gate

Current native gate for backrooms-liminal: `data/core_loop.json`,
`reviews/first_slice_visual_gate.md`, native build, DevAPI smoke, first-screen
screenshot, post-fuse route/stalker screenshot, readability zoom, strict
product gate, and slice hygiene evidence.

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
  is a strict desktop product gate PASS for T0002 route/stalker pressure.
- `build/captures/backrooms_t0002_first_screen.png` shows the stable starting
  corridor after the route-pressure HUD addition.
- `build/captures/backrooms_t0002_shift_threat.png` shows false green exits,
  route shift, stalker pressure, and the humanoid threat state.
- `build/captures/backrooms_t0002_shift_threat_uizoom.png` is the latest
  readability montage.
- `build/captures/backrooms_t0002_slice_hygiene.md` is WARN only because
  profiler evidence is advisory/partially unparsable, not because gameplay
  validation failed.

## Next Priorities

1. Let the lead/playtest judge whether T0002 is scary and interesting enough
   for the next slice.
2. If accepted, create one narrow task for audio/hum cues, stronger route
   variation, or a short win/fail polish pass.
3. If rejected visually, freeze content expansion and improve the threat
   silhouette, false exits, lighting, or HUD readability before adding systems.
