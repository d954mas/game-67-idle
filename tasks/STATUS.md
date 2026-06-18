# Project Status

## Current Goal

Review and iterate the `Backrooms Liminal` (backrooms-liminal) native-first
prototype: the first-person 3D corridor slice now exists with fuse pickup,
return-to-exit, fear/battery pressure, route instability, stalker pressure, and
native generated-PCM horror audio cues, readable win/fail/replay states, and
three deterministic risky route-choice anomalies on the return path with strict
visual/player-read proof.

## Blocking Work

- No runtime implementation blocker is known. T0001, T0002, T0003, T0004, and
  T0005 are in review; expansion should wait for lead/playtest feedback or a
  new narrow task.

## Non-blocking Debt

- Global AI profile review confidence is still broken by older unresolved failed
  records. T0005 current-scope guard is also broken by earlier failed
  route-choice scenario attempts that were fixed and rerun successfully; slice
  hygiene treats profiler evidence as advisory-only.

## Current Gate

Current native gate for backrooms-liminal: `data/core_loop.json`,
`reviews/first_slice_visual_gate.md`, native build, DevAPI smoke, first-screen
screenshot, post-fuse route/stalker screenshot, native audio cue status,
win/fail/replay screenshots, route-choice screenshots/status report,
readability zoom, strict product gate, and slice hygiene evidence.

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
- `build/captures/backrooms_t0003_first_screen.png` and
  `build/captures/backrooms_t0003_audio_threat.png` are the latest visual proof
  after adding audio cues.
- `build/captures/backrooms_t0003_audio_status.json` proves generated PCM cues
  fired in automation: flashlight, fuse hum, fuse pickup, stalker, caught, and
  escape.
- `build/captures/backrooms_t0003_audio_threat_uizoom.png` is the latest
  readability montage.
- `build/captures/backrooms_t0003_slice_hygiene.md` is WARN only because
  profiler evidence is advisory/partially unparsable, not because gameplay
  validation failed.
- `build/captures/backrooms_t0004_win_overlay.png` and
  `build/captures/backrooms_t0004_fail_overlay.png` show readable end-state
  overlays with run time, fear, battery, and replay prompt.
- `build/captures/backrooms_t0004_replay_status.json` proves win/fail restart
  behavior through DevAPI (`can_restart` true on end states, false after
  replay).
- `build/captures/backrooms_t0004_win_overlay_uizoom.png` and
  `build/captures/backrooms_t0004_fail_overlay_uizoom.png` are the latest
  readability montages.
- `build/captures/backrooms_t0004_slice_hygiene.md` is WARN only because
  profiler evidence is advisory/partially unparsable, not because gameplay
  validation failed.
- `build/captures/backrooms_t0005_first_screen.png` shows the stable first
  screen after the route-choice slice.
- `build/captures/backrooms_t0005_route_choice.png` shows the first active
  return-path lane anomaly with green safe-lane lighting, red danger side,
  stalker pressure, HUD state, and the `MOVE LEFT - TRUST HUM` prompt.
- `build/captures/backrooms_t0005_wrong_turn.png` shows wrong-turn feedback,
  higher fear, and stalker pressure after choosing the unsafe lane.
- `build/captures/backrooms_t0005_route_choice_status.json` proves all route
  anomaly checks true: three total choices, active safe side, correct resolve,
  wrong punishment, second choice right, and third choice left.
- `build/captures/backrooms_t0005_route_choice_uizoom.png` is the latest
  readability montage for the route-choice HUD/prompt.
- `gamedesign/projects/backrooms-liminal/reviews/product_read_gate_t0005_desktop.json`
  is a strict desktop product gate PASS for risky route-choice anomalies.
- `build/captures/backrooms_t0005_slice_hygiene.md` is WARN only because
  profiler evidence is advisory due to fixed earlier scenario failures, not
  because gameplay validation failed.

## Next Priorities

1. Let the lead/playtest judge whether T0005 makes the return path scary and
   interesting enough.
2. If accepted, create one narrow task for a short full-run playtest scenario,
   stronger branching geometry, or final visual polish on the stalker/lighting.
3. If rejected visually or mechanically, freeze content expansion and improve
   the anomaly readability, threat silhouette, wrong-turn feedback, or HUD
   clarity before adding broader systems.
