---
id: T0008
title: Sprint footstep and chase audio feedback for Backrooms Liminal
status: review
epic: E001
priority: P1
tags: [prototype, backrooms-liminal, native-first, horror, audio]
created: 2026-06-18
updated: 2026-06-18
---

## What

Make sprint and blackout chase pressure audible in the native Backrooms slice.
Walking should produce slower generated footstep thumps, sprinting should
produce faster harsher steps, and blackout pursuit should add a heartbeat/chase
pulse so the escape feels physical instead of only visual.

## Done when

- [x] Walking and sprinting trigger generated movement cues at distinct cadences.
- [x] Blackout/ambush chase triggers a heartbeat or chase pulse while active.
- [x] `game.audio.status` exposes the new cue counts and a focused native
      scenario proves they increment.
- [x] Native build, smoke, audio scenario, screenshot/readability, product gate,
      slice hygiene, and taskboard validation pass, or any debt is explicit.

## Open questions

- Are the generated pulses enough, or should a later slice add authored audio
  assets and a real mixer?

## Log

- 2026-06-18: Started focused sprint/footstep/chase audio feedback slice after
  T0007 made sprint mechanically useful but mostly silent.
- 2026-06-18: product gate PASS (desktop); review: gamedesign\projects\backrooms-liminal\reviews\product_read_gate_t0008_desktop.md; screenshot: build/captures/backrooms_t0008_sprint_audio.png; next: continue to the next narrow slice
- 2026-06-18: Implemented generated PCM footstep, sprint-step, and heartbeat
  cues. Evidence: `build/captures/backrooms_t0008_audio_status.json` proves
  walking increments `footstep`, sprint increments `sprint_step`, blackout chase
  increments `heartbeat`; `build/captures/backrooms_t0008_sprint_audio.png`,
  readability PASS, and product gate PASS.
