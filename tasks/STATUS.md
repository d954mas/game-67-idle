# Project Status

## Current Goal

Review and iterate the `Backrooms Liminal` (backrooms-liminal) native-first
prototype: the first-person 3D corridor slice now exists with fuse pickup,
return-to-exit, fear/battery pressure, route instability, stalker pressure, and
native generated-PCM horror audio cues, readable win/fail/replay states, and
three deterministic risky route-choice anomalies on the return path. Wrong
route choices now trigger a blackout ambush with closer stalker pressure, while
correct choices produce a safe-turn relief pulse. The slice has strict
visual/player-read proof.

## Blocking Work

- No runtime implementation blocker is known. T0001, T0002, T0003, T0004,
  T0005, and T0006 are in review; expansion should wait for lead/playtest
  feedback or a new narrow task.

## Non-blocking Debt

- Global AI profile review confidence is still broken by older unresolved failed
  records. T0006 current-scope guard reported usable, but slice hygiene still
  treats global profiler evidence as advisory-only because of older unresolved
  failures.

## Current Gate

Current native gate for backrooms-liminal: `data/core_loop.json`,
`reviews/first_slice_visual_gate.md`, native build, DevAPI smoke, first-screen
screenshot, post-fuse route/stalker screenshot, native audio cue status,
win/fail/replay screenshots, route-choice screenshots/status report,
blackout ambush screenshots/status report, readability zoom, strict product
gate, and slice hygiene evidence.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
cmake --build --preset native-debug --target game_seed
py -3.12 tools/devapi/smoke.py
node tools/taskboard/cli.mjs validate
```

## Last Known Good Evidence

- Historical T0001-T0005 evidence is in the review task logs and product gates
  under `gamedesign/projects/backrooms-liminal/reviews/`; do not expand that
  history inline here.
- `gamedesign/projects/backrooms-liminal/visual/live_state_acceptance_matrix.json`
  is the required live-state matrix for strict product gates.
- `gamedesign/projects/backrooms-liminal/reviews/product_read_gate_latest.json`
  points to the latest strict desktop product gate, currently T0006.
- `build/captures/backrooms_t0006_first_screen.png` shows the stable first
  screen after the blackout ambush slice.
- `build/captures/backrooms_t0006_choice_ready.png` shows the route choice
  before the blackout/relief branch.
- `build/captures/backrooms_t0006_safe_turn_relief.png` shows the safe-turn
  relief pulse, lower pressure, and readable `SAFE TURN - MOVE` prompt.
- `build/captures/backrooms_t0006_blackout_ambush.png` shows the wrong-turn
  blackout ambush with darkened lighting, red warning, and closer stalker.
- `build/captures/backrooms_t0006_blackout_status.json` proves all blackout
  checks true: active choice, safe-turn relief, wrong-turn blackout, ambush
  timer active, blackout decay, and wrong-turn pressure increase.
- `build/captures/backrooms_t0006_blackout_ambush_uizoom.png` and
  `build/captures/backrooms_t0006_blackout_ambush_uizoom_cmp.png` are the
  latest readability and before/after regression montages.
- `gamedesign/projects/backrooms-liminal/reviews/product_read_gate_t0006_desktop.json`
  is a strict desktop product gate PASS for the blackout ambush lighting climax.
- `build/captures/backrooms_t0006_slice_hygiene.md` is WARN only because global
  profiler evidence has older historical debt; current T0006 gameplay
  validation passed.
- `build/captures/backrooms_t0006_first_screen.png` shows the stable first
  screen after the blackout ambush slice.
- `build/captures/backrooms_t0006_choice_ready.png` shows the route choice
  before the blackout/relief branch.
- `build/captures/backrooms_t0006_safe_turn_relief.png` shows the safe-turn
  relief pulse, lower pressure, and readable `SAFE TURN - MOVE` prompt.
- `build/captures/backrooms_t0006_blackout_ambush.png` shows the wrong-turn
  blackout ambush with darkened lighting, red warning, and closer stalker.
- `build/captures/backrooms_t0006_blackout_status.json` proves all blackout
  checks true: active choice, safe-turn relief, wrong-turn blackout, ambush
  timer active, blackout decay, and wrong-turn pressure increase.
- `build/captures/backrooms_t0006_blackout_ambush_uizoom.png` and
  `build/captures/backrooms_t0006_blackout_ambush_uizoom_cmp.png` are the
  latest readability and before/after regression montages.
- `gamedesign/projects/backrooms-liminal/reviews/product_read_gate_t0006_desktop.json`
  is a strict desktop product gate PASS for the blackout ambush lighting climax.
- `build/captures/backrooms_t0006_slice_hygiene.md` is WARN only because
  global profiler evidence has older historical debt; current T0006 gameplay
  validation passed.

## Next Priorities

1. Let the lead/playtest judge whether T0006 makes wrong turns frightening
   enough without becoming unreadable.
2. If accepted, create one narrow task for a short full-run playtest scenario,
   stronger branching geometry, or sprint/chase tuning after blackout.
3. If rejected visually or mechanically, freeze content expansion and improve
   blackout readability, threat silhouette distance, warning timing, or HUD
   clarity before adding broader systems.
