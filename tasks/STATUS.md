# Project Status

## Current Goal

Review and iterate the `Backrooms Liminal` (backrooms-liminal) native-first
prototype: the first-person 3D corridor slice now exists with fuse pickup,
return-to-exit, fear/battery pressure, route instability, stalker pressure, and
native generated-PCM horror audio cues, readable win/fail/replay states, and
three deterministic risky route-choice anomalies on the return path. Wrong
route choices now trigger a blackout ambush with closer stalker pressure, while
correct choices produce a safe-turn relief pulse; holding Shift now gives a real
sprint escape action that trades battery for distance and lower chase pressure.
Walking, sprinting, and blackout chase now fire distinct generated movement /
heartbeat audio cues proven through DevAPI status. The slice has strict
visual/player-read proof.

## Blocking Work

- No runtime implementation blocker is known. T0001, T0002, T0003, T0004,
  T0005, T0006, T0007, and T0008 are in review; expansion should wait for
  lead/playtest feedback or a new narrow task.

## Non-blocking Debt

- Global AI profile review confidence is still broken by older unresolved failed
  records. T0008 current-scope guard is also red because unresolved profiler
  failure records exist; gameplay/build/product evidence is green, but profiler
  evidence must remain advisory-only.

## Current Gate

Current native gate for backrooms-liminal: `data/core_loop.json`,
`reviews/first_slice_visual_gate.md`, native build, DevAPI smoke, first-screen
screenshot, post-fuse route/stalker screenshot, native audio cue status,
win/fail/replay screenshots, route-choice screenshots/status report,
blackout/sprint screenshots/status reports, sprint footstep/chase audio status,
readability zoom, strict product gate, and slice hygiene evidence.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
cmake --build --preset native-debug --target game_seed
py -3.12 tools/devapi/smoke.py
node tools/taskboard/cli.mjs validate
```

## Last Known Good Evidence

- Historical T0001-T0006 evidence is in the review task logs and product gates
  under `gamedesign/projects/backrooms-liminal/reviews/`; do not expand that
  history inline here.
- `gamedesign/projects/backrooms-liminal/visual/live_state_acceptance_matrix.json`
  is the required live-state matrix for strict product gates.
- `gamedesign/projects/backrooms-liminal/reviews/product_read_gate_latest.json`
  points to the latest strict desktop product gate, currently T0008.
- `build/captures/backrooms_t0008_first_screen.png` shows the stable first
  screen after sprint/audio tuning.
- `build/captures/backrooms_t0008_sprint_audio.png` shows blackout sprint
  guidance with `SHIFT SPRINT`, `LIGHTS OUT - SPRINT`, and close stalker
  pressure.
- `build/captures/backrooms_t0008_audio_status.json` proves movement/chase audio
  checks true: walking increments `footstep`, sprint increments `sprint_step`,
  blackout chase increments `heartbeat`, and sprinting state is true.
- `build/captures/backrooms_t0008_sprint_audio_uizoom.png` and
  `build/captures/backrooms_t0008_sprint_audio_uizoom_cmp.png` are the latest
  readability and before/after regression montages.
- `gamedesign/projects/backrooms-liminal/reviews/product_read_gate_t0008_desktop.json`
  is a strict desktop product gate PASS for sprint footstep/chase audio.
- `tmp/t0008_slice_hygiene.md` is WARN only because profiler guard evidence is
  inconclusive; current T0008 build, smoke, scenario, readability, taskboard,
  and product evidence passed.

## Next Priorities

1. Let the lead/playtest judge whether T0008 makes blackout chase feel playable,
   scary, and physically readable enough.
2. If accepted, create one narrow task for a short full-run playtest scenario,
   stronger branching geometry, or audio balance/authored asset polish.
3. If rejected visually or mechanically, freeze content expansion and improve
   sprint readability, battery cost, threat distance, warning timing, audio
   cadence, or HUD clarity before adding broader systems.
