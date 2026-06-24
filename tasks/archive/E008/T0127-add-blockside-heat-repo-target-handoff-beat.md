---
id: T0127
title: Add Blockside Heat repo target handoff beat
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, mission, story, vehicle, npc]
created: 2026-06-23
updated: 2026-06-23
---

## What

After `tail_stop`, add one small repo target handoff interaction so the stopped
courier payoff leads into the next playable objective. Keep it to one marker or
NPC cue, state/HUD response, and native capture evidence.

## Done when

- [x] After `tail_stop`, reaching/using the handoff point advances a named
      handoff state.
- [x] HUD/state tells the player what the repo target is and what to do next.
- [x] Native capture/probe evidence covers `tail_stop` and the handoff beat.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0126 tail-stop pass. Scope is one handoff beat
  only; no full new mission chain, traffic system, inventory/economy expansion,
  district expansion, or mission menu in this slice.
- 2026-06-23: Implemented `target_handoff` story state, DevAPI action
  `game.action.target_handoff`, HUD/state `JOB: TARGET HANDOFF`, green coupe
  objective hook, and native screenshot
  `tmp/blockside-heat/target-handoff-latest.png`. Strict product-read gate
  passes in `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T15-40-59-382Z_desktop.md; screenshot: tmp/blockside-heat/target-handoff-latest.png; evidence: tmp/blockside-heat/capture-states-report.json; next: Implement a small green-coupe approach beat after target_handoff as the next narrow playable slice.
