---
id: T0009
title: Backrooms exploration and lostness pass
status: doing
epic: E001
priority: P1
tags: [prototype, backrooms-liminal, native-first, horror, exploration, research]
created: 2026-06-18
updated: 2026-06-18
---

## What

Fix the lead-identified mismatch: the current build reads as a straight horror
corridor, not as Backrooms. Research what Backrooms/liminal players expect, then
turn that into one native exploration/lostness pass: branching rooms, loops,
landmarks that stop being reliable, and proof that the player can get
disoriented while still having a playable goal.

## Done when

- [x] Durable research brief records lore, player expectations, similar games,
      review takeaways, and current-build mismatch.
- [x] Native runtime no longer presents the first playable as one straight
      path; it includes at least one readable branch/loop/room choice.
- [x] DevAPI scenario proves the player can enter a side route, return/loop, and
      still pursue the rooms/exit objective.
- [ ] Screenshot/product gate explicitly judges "looks like Backrooms / feeling
      lost", not only chase readability.
- [ ] Native build, smoke, focused scenario, readability, product gate, taskboard
      validation, and slice hygiene pass or explicit debt is logged.

## Open questions

- How far should the first lostness pass go before it becomes real procedural
  generation?

## Log

- 2026-06-18: Lead rejected current build as not Backrooms enough: too straight,
  no exploration, no feeling of being lost. Started research + implementation
  pass focused on Level 0 lostness rather than corridor chase polish.
- 2026-06-18: Implemented a small authored Level 0 pocket-maze in the native
  runtime: side dead-end, red-room shift, final exit room, room visit tracking,
  hidden director pressure, and dynamo-flashlight behavior. Focused DevAPI
  scenario passes: side route, return, red-room shift, three rooms, exit use,
  and escape. Visual/product gate remains open because room execution, lighting,
  and material quality are still below the requested polished Backrooms bar.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign\projects\backrooms-liminal\reviews\t0009_lostness_visual_gate.md; screenshot: build/captures/backrooms_t0009_branch_room.png; next: Replace the shader-box room execution with stronger architectural rooms, better fluorescent lighting/shadows, less UI plating, and a more believable exit composition.
- 2026-06-18: Validation evidence: native build PASS, DevAPI smoke PASS,
  focused T0009 scenario PASS, UI readability PASS, taskboard validate PASS,
  slice hygiene WARN with known red visual gate and profiler guard explicitly
  not used as acceptance evidence.
