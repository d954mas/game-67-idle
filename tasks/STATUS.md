# Project Status

## Current Goal

`Critter Corral` (critter-corral, epic E004) is a **gameplay release-candidate**:
a complete native casual herd-sort game (core moment + run structure + 2->5
colors + 4 behaviors + between-wave upgrades + audio + fontless HUD), rendered
on free placeholder sprites, code review CLEAN, visual gate PASS. Remaining for
a hard release (out of autonomous reach, by design): a HUMAN fun/balance
playtest, and bespoke art via Codex (swap placeholder PNGs in-place).

## Blocking Work

- No runtime implementation blocker is known yet; the next blocker should come
  from the first GDD/reference/fake-shot pass.

## Non-blocking Debt

- None recorded for this prototype yet.

## Current Gate

Critter Corral gameplay is built + reviewed (T0064 first slice, T0065 expansion,
both done). Visual gate PASS (`reviews/first_slice_visual_gate.md`,
`build/captures/corral_review2.png`). Next gate is the LEAD's: play a real-time
10-20 min run and judge fun/balance; then commission the Codex art pass. No
open implementation task.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
node tools/taskboard/cli.mjs validate
```

## Last Known Good Evidence

- Build/run/handoff: `gamedesign/projects/critter-corral/game_implementation_plan.md`
  (cmake targets `critter_corral_packs` + `game_seed`; exe
  `build/game_seed/native-debug/game_seed.exe --devapi 9123`).
- Visual gate PASS: `reviews/first_slice_visual_gate.md`; captures
  `build/captures/corral_review2.png` (readability), `corral_run.png` (5 colors),
  `corral_behaviors.png`, `corral_upgrade.png`.
- Code review CLEAN (T0065 log); -Werror clean build.

## Next Priorities

1. LEAD: play a real-time 10-20 min run; judge fun/balance/pacing; note fixes.
2. Codex bespoke art pass: replace placeholder sprite PNGs in-place (same atlas
   regions) per `game_implementation_plan.md`; rebuild the pack, no code change.
3. Optional later: a polished audio/SFX pass; balance tuning from the playtest.
