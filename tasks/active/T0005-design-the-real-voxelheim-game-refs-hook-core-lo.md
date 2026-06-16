---
id: T0005
title: "Design the REAL Voxelheim game: refs -> hook -> core loop -> progression/economy -> FTUE (pass the core-loop gate)"
status: todo
epic: E001
priority: P1
tags: [voxelheim, design, core-loop, gate]
created: 2026-06-16
updated: 2026-06-16
---

## What

The first slice is a polished visual/tech proof, NOT a designed game (lead:
"I don't get what the game is, the loop, the idea, the refs"). Do the design
work that should have come first, and make the build pass the new
**Game / core-loop gate** in AGENTS.md. BLOCKED on the genre decision (idle vs
real-time action-RPG, see Open questions) — once chosen, run the design.

## Done when

- [ ] Genre/direction chosen by the lead (idle / action-RPG / other).
- [ ] Reference deconstruction: 1-3 named real games in the chosen genre, with a
      Reference Digest (observed loop, economy, progression, retention hook,
      borrow/avoid) per `gamedesign/knowledge/reference_deconstruction.md`.
- [ ] Hook/fantasy stated in ONE sentence a player would repeat.
- [ ] Core loop with NUMBERS: >=3 interlocking verbs, the reward, and the reason
      to repeat; first-30s, first-5-min, and the session loop.
- [ ] Progression / economy: what grows, what is spent, why; the "next 5 minutes"
      pull; meta/retention (upgrades / prestige / unlocks as fits the genre).
- [ ] `data/balance.json` (+ economy data) so the loop is implementable from
      files, not invented in code.
- [ ] gdd.md rewritten around the loop (not the screen); concept.md hook updated.
- [ ] Build re-judged by a **game-design critic/playtest** (fun + loop +
      why-replay), separate from the art/UX critic — passes the core-loop gate.

## Open questions

- Genre: the project is `game-67-idle` but the build is real-time action-RPG.
  Idle/incremental, deepen the action-RPG, or other? (Lead decides; pending.)

## Log

- 2026-06-16 created from the session retrospective (AI_PIPELINE_HISTORY): the
  pipeline gated appearance + screen-teachability but not the GAME; added the
  Game/core-loop gate to AGENTS.md. This task is the real design work.
