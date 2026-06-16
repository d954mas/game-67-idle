# Project Status

## Current Goal

`Critter Corral` (critter-corral, epic E004) is a **generated visual/UI release-candidate**:
a complete casual herd-sort game (core moment + run/levels + 2->5 colors + 4
behaviors + between-wave upgrades + audio), now with real generated source art
integrated into the native sprite/UI path (T0070) and source-to-runtime
orientation fixed for directional assets (T0072), clear text/FTUE, portrait
touch layout, and desktop+portrait product gates passing.
Remaining for a hard release (by design, out of autonomous reach): a HUMAN
10-20 min fun/balance playtest, then tuning from that playtest; optional later
audio polish.

## Blocking Work

- No runtime implementation blocker is known.
- No open generated-art importer/check performance blocker is known after
  T0071, T0073, and the closed T0075 importer/native orchestration pass.

## Non-blocking Debt

- None recorded for this prototype yet.

## Current Gate

Critter Corral gameplay is built + reviewed (T0064 first slice, T0065 expansion,
both done). The earlier procedural-looking visual pass was superseded by T0070:
generated card, critter, pen, and upgrade icon sources are saved with
provenance, cut into runtime sprites, and pixel-audited. T0072 fixed
source-to-runtime orientation for directional generated art. T0071 optimized
the generated-source chroma-key importer hot path and added native timing tools.
T0073 optimized generated-asset build/audit scripts around the helper.
T0075 closed the generated core importer production/performance cycle: Python
keeps production asset semantics, native C handles heavy source keying, runtime
caches repair partial invalidation, and `--audit` combines import+pixel audit
without a Node wrapper.
Next product gate is the LEAD's: play a real-time 10-20 min run and judge
fun/balance.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
node tools/taskboard/cli.mjs validate
```

## Last Known Good Evidence

- Build/run/handoff: `gamedesign/projects/critter-corral/game_implementation_plan.md`
  (cmake targets `critter_corral_packs` + `game_seed`; exe
  `build/game_seed/native-debug/game_seed.exe --devapi 9123`).
- Visual/UI gate PASS: `reviews/first_slice_visual_gate.md`,
  `gamedesign/projects/critter-corral/reviews/T0068_visual_ui_desktop_gate.md`,
  `gamedesign/projects/critter-corral/reviews/T0068_visual_ui_portrait_gate.md`,
  `gamedesign/projects/critter-corral/reviews/T0068_portrait_upgrade_layout_audit.md`,
  `gamedesign/projects/critter-corral/reviews/T0069_upgrade_icon_polish_gate.md`,
  `gamedesign/projects/critter-corral/reviews/T0069_portrait_upgrade_layout_audit.md`,
  `gamedesign/projects/critter-corral/reviews/T0070_portrait_upgrade_generated_gate.md`,
  `gamedesign/projects/critter-corral/reviews/T0070_landscape_play_generated_gate.md`,
  `gamedesign/projects/critter-corral/reviews/T0072_portrait_upgrade_orientation_gate.md`,
  `gamedesign/projects/critter-corral/reviews/T0072_landscape_play_orientation_gate.md`;
  captures `build/captures/corral_visual_ui_landscape_play.png`,
  `build/captures/corral_portrait.png`, `corral_portrait_play.png`,
  `corral_portrait_upgrade.png`, `corral_landscape_upgrade_polish.png`.
- Code review CLEAN (T0065 log); -Werror clean build.
- Generated asset importer performance evidence (T0071):
  `tools/assets/benchmark_chroma_key_alpha.py`; native CMake target
  `asset_chroma_key_native`; key_to_alpha baseline 11.419s on critter source,
  optimized Python 1.373s, full generated importer about 1.27s after source
  cache, native no-write 0.087s with 8 threads.
- Generated asset build/audit script performance evidence (T0073):
  `audit_generated_source_derivation.py` expected T0070 mismatch fail improved
  from 6.603s to 0.154s; same-size compare is NumPy-vectorized; crop-plan
  builder now keys a source sheet once per chroma key and crops from that keyed
  source.
- Generated core importer production/performance evidence (T0075 archive):
  Python/native runtime PNG parity passed; native worker is threaded heavy-key
  stage only; hot no-op importer is about 0.058-0.064s; combined no-op
  import+audit is about 0.178-0.217s; Node wrapper was rejected as unnecessary
  orchestration complexity.

## Next Priorities

1. T0076: optimize skills and generation workflow now that UI/importer tooling
   is fast enough for iteration.
2. LEAD: play a real-time 10-20 min run; judge fun/balance/pacing; note fixes.
3. Tune balance/feel from that human playtest.
4. Optional later: polished SFX/music pass.
