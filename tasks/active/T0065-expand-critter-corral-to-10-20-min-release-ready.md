---
id: T0065
title: Expand Critter Corral to 10-20 min release-ready gameplay
status: doing
epic: E004
priority: P1
tags: [prototype, critter-corral, gameplay, release]
created: 2026-06-15
updated: 2026-06-15
---

## What

The first playable slice is done (T0064): sprite-rendered core moment + passing
visual gate. Expand it to a full **10-20 minute release-ready gameplay session**,
following the GDD roadmap ONE layer at a time, re-judging feel (and re-running
the visual gate) after each. Free placeholder sprites stay; Codex does bespoke
art later.

## Done when

- [ ] Run structure: an escalating sequence of waves that totals ~10-20 minutes;
      clear win/advance + lose/restart flow; a title/start beat and restart.
- [ ] Progression curve: more critters + +1 color over time (up to ~5), with a
      pacing that ramps without feeling unfair (calm-first identity kept).
- [ ] Critter behavior variety (the twist's depth): e.g. skittish (briefly flee
      the lure), stubborn (slow to steer), follower (clings to other colors) —
      each forces a different herding read.
- [ ] Light meta between waves: one readable upgrade pick (bigger lure radius,
      second lure, calmer critters, wider gate), earned from corralled count.
- [ ] FTUE <=3 beats, tutorial-by-doing (lure moves critters -> match pen =
      pop+chain -> clear the wave). No walls.
- [ ] Release polish: consistent juice + audio pass + perf; visual gate stays
      PASS; a full run is completable.
- [ ] Playtest evidence: a ~10-20 min run is completable and reads as fun; the
      core moment + escalation verified via capture/playtest (not just static).

## Open questions

- Endless-escalation vs a fixed "win" at wave N? Lean: escalating with a soft
  win milestone + endless after, so a session is ~10-20 min but can continue.

## Log

- 2026-06-15: Created after the first slice (T0064) passed the visual gate.
  Expand per GDD roadmap, one layer at a time, re-judging feel each step.

- 2026-06-16: Increment 1 (run structure + progression). Phases TITLE->PLAYING->WAVE_CLEARED->soft WIN(wave10)->endless; restart (R/marker); progression waves ramp 2->5 colors, 4->40 critters, gentle speed-up; dynamic pen layout (sides/corners/bottom) with color flags + inward gate-direction cues; FTUE wave-1 + pulsing lure hint; fontless wave/score/remaining HUD. DevAPI game.start + game.debug.skip_wave. Playtest confirmed ramp (table) + win milestone. Visual gate stays PASS (corral_run.png wave 13, audit pass). Builds clean -Werror. Watch: late-wave density vs calm identity -> depth (behavior/meta) next, not just more count.

- 2026-06-16: Increment 2 (critter behavior variety). Added behavior types normal/skittish(flees lure when crowded)/stubborn(slow to steer, bigger+dark outline)/follower(clings to neighbours). Progressive unlock (skittish w3, stubborn w5, follower w7), normal stays majority, shares capped -> fair/calm. Readable tells (stubborn+skittish read instantly; follower subtlest). game.state reports loose_by_behavior. Builds clean -Werror; playtest table confirms progressive intro; corral_behaviors.png (wave 9, 4 behaviors) audit pass, composition readable. Visual gate stays PASS.
