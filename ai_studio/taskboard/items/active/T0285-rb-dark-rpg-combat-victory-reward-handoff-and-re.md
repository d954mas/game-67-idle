---
id: T0285
title: "rb-dark-rpg: combat victory reward handoff and result UX"
status: doing
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, combat, rewards, uiux]
created: 2026-07-04
updated: 2026-07-05
---

## What

Polish the victory side of combat rewards. The current result panel already
shows rewards, but the handoff is still too generic: the primary button says
"return to guard" even for non-guard fights, and runtime evidence should prove
that visible reward outcomes match the applied state.

This slice keeps rewards immediate and deterministic:

- rewards are granted in `game_actions_resolve_encounter`;
- result UI explains what changed;
- top HUD reflects updated HP/gold after the result closes;
- first fight and later multi-reward fight both have runtime evidence.

## Done when

- [ ] Win DevAPI scenarios assert applied XP, gold, reward ids, reward items,
      quest step/status, and HUD-visible state after closing the result.
- [ ] Victory result copy works for both first guard encounter and later
      location encounters; no hardcoded "return to guard" outside the first
      encounter.
- [ ] Multi-reward result still lists every reward item clearly.
- [ ] 16:9 and phone evidence is captured for first win and later multi-reward win.
- [ ] Existing combat, first-scene, scenario, and content checks pass.
- [ ] Subagent review covers reward correctness and victory UX.

## Open questions

- Resolved for v1: first gate-check victory can use context-specific `К стражу`;
  later/non-guard victories use neutral `Продолжить` unless a safe authored next
  objective is available.
- Resolved for v1: result panel may show one short next-step line, but detailed
  quest explanation stays in journal/dialogue slices.

## Log

- 2026-07-04: Created after T0282/T0284 closed defeat and recovery loops.
- 2026-07-04: Planning locked: polish victory result/reward handoff and add runtime win assertions.
- 2026-07-05: Design lock captured in games/rb-dark-rpg/design/combat_reward_loop_v1.md.
  T0285 remains the active next slice before visual combat polish.
- 2026-07-05: Lead direction locked: combat starts from a concrete place/threat
  and the bottom nav only opens sections. Finish T0285 first, including
  no-click-through result close evidence, then T0286 combat readability and
  T0287 reward presentation.
