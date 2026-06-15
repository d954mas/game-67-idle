---
id: T0065
title: Expand Critter Corral to 10-20 min release-ready gameplay
status: done
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

- [x] Run structure: TITLE -> waves -> WAVE_CLEARED -> upgrade -> soft WIN (wave
      10) -> endless; restart (R/marker); ~10-20 min to/through the win. (inc 1)
- [x] Progression: 2->5 colors, ~4->40 critters (cap 64), gentle speed-up; calm
      ramp. (inc 1)
- [x] Behavior variety: normal/skittish/stubborn/follower, progressive+capped,
      readable tells, each changes the herding decision. (inc 2)
- [x] Light meta: between-wave pick-1-of-3 upgrades (6 capped/stacking types),
      visible in HUD. (inc 3)
- [x] FTUE <=3 beats, tutorial-by-doing (tiny wave 1 + pulsing lure hint). (inc 1)
- [x] Release polish: 7 procedural SFX (chain rising pitch), juice, smooth
      transitions, perf fine; visual gate PASS; run completable. (inc 4)
- [~] Playtest evidence: completability + escalation VERIFIED (devapi to wave 13+;
      chain 137/win 1). Subjective FUN/balance over a real-time run needs a HUMAN
      playtest -> handed to lead.
- [x] Code review CLEAN: no critical/high/medium; LOW findings cosmetic (LOW-1
      confirmed non-issue vs engine result-ownership convention).

## Open questions

- Endless-escalation vs a fixed "win" at wave N? Lean: escalating with a soft
  win milestone + endless after, so a session is ~10-20 min but can continue.

## Log

- 2026-06-15: Created after the first slice (T0064) passed the visual gate.
  Expand per GDD roadmap, one layer at a time, re-judging feel each step.

- 2026-06-16: Increment 1 (run structure + progression). Phases TITLE->PLAYING->WAVE_CLEARED->soft WIN(wave10)->endless; restart (R/marker); progression waves ramp 2->5 colors, 4->40 critters, gentle speed-up; dynamic pen layout (sides/corners/bottom) with color flags + inward gate-direction cues; FTUE wave-1 + pulsing lure hint; fontless wave/score/remaining HUD. DevAPI game.start + game.debug.skip_wave. Playtest confirmed ramp (table) + win milestone. Visual gate stays PASS (corral_run.png wave 13, audit pass). Builds clean -Werror. Watch: late-wave density vs calm identity -> depth (behavior/meta) next, not just more count.

- 2026-06-16: Increment 2 (critter behavior variety). Added behavior types normal/skittish(flees lure when crowded)/stubborn(slow to steer, bigger+dark outline)/follower(clings to neighbours). Progressive unlock (skittish w3, stubborn w5, follower w7), normal stays majority, shares capped -> fair/calm. Readable tells (stubborn+skittish read instantly; follower subtlest). game.state reports loose_by_behavior. Builds clean -Werror; playtest table confirms progressive intro; corral_behaviors.png (wave 9, 4 behaviors) audit pass, composition readable. Visual gate stays PASS.

- 2026-06-16: Increment 3 (light meta). Between-wave pick-1-of-3 upgrades, 6 types (lure radius/pull, second lure, wider gates, calmer critters, longer chain), capped lvl3, stacking, effects wired + visible (HUD acquired-row, second-lure renders). Fontless icon cards (PIL icons added; pack=16 sprites). DevAPI: upgrades + effective{} + pending_choice; game.debug.pick_upgrade. Fixed a real bug: resolve_atlas_regions names[] was undersized after new regions (broke rendering) -> fixed. Builds clean -Werror; playtest shows upgrades accumulate + apply (lure_radius 150->196, second_lure on). corral_upgrade.png audit pass, cards clearly readable. Visual gate PASS.

- 2026-06-16: Increment 4 (audio + polish). Engine audio is procedural PCM (winmm) -> added 7 soft synth SFX via game_audio: pop (pitch wobble), chain (rising pitch per link), gated bonk, upgrade chime, wave/win flourishes, start pad. Particle burst grows with chain depth. game.state.audio counters prove firing (pop 38/chain 137/win 1 in a run). Smooth transitions; perf fine (60fps cap, no hot-path alloc). Builds clean -Werror; corral_polish.png audit pass.

- 2026-06-16: RELEASE-CANDIDATE. All roadmap layers shipped (inc 1-4): run structure/progression, behavior variety, meta upgrades, audio+polish. Code review CLEAN (no critical/high/medium; LOW cosmetic). Visual gate PASS. Builds clean -Werror. Handoff: game_implementation_plan.md. Honest remaining (out of autonomous reach, by design): subjective fun/balance over a real-time 10-20 min run = lead playtest; bespoke art = Codex (swap placeholder PNGs in-place). Closing implementation; gameplay is release-shaped.
