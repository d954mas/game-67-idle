---
id: T0010
title: Portal memory marking and object-placement spike
status: doing
epic: E001
priority: P1
tags: [prototype, backrooms-liminal, portal-rendering, marking, object-puzzle, native-first]
created: 2026-06-18
updated: 2026-06-18
---

## What

Turn the new differentiator into a narrow native spike: Backrooms Liminal is not
just another maze, it is a place that is bigger inside than outside and can copy
or corrupt the player's evidence. Prove one impossible-room/portal moment where
the player can mark a surface, find an object, place it at the correct landmark,
and reveal a real exit.

## Done when

- [x] Project direction doc exists and names the hook, core loop, and visual
      target for portal rooms, player marks, and object placement.
- [ ] Native runtime shows one readable impossible-room/portal composition:
      the room reads larger/different inside than the approach suggests.
- [x] Player can place at least one visible mark on a wall or floor surface.
- [x] Player can pick up one mundane object and place it on one valid target.
- [x] Correct placement reveals or stabilizes a real exit; wrong/missing
      placement does not.
- [x] DevAPI scenario proves mark placement, pickup, placement, exit reveal,
      and escape.
- [ ] Screenshot/product gate judges whether the portal-room proof looks
      high-quality and distinctive, not like a debug shader trick.

## Open questions

- Should the first mark be freehand drawing, a stamped symbol, or both?
- Which object best communicates the rule: key, breaker handle, room plate, or
  mundane prop such as a chair/sign?
- Should the first portal be real recursive rendering or a staged one-portal
  illusion backed by explicit room state?

## Log

- 2026-06-18: Lead proposed the differentiator: portal-rendered impossible
  rooms, loops, drawing on walls/floors, finding keys/items, placing them on
  correct spots, and escaping by understanding the space. Direction captured in
  `gamedesign/projects/backrooms-liminal/portal_memory_loop_direction.md`.
- 2026-06-18: First native test scope narrowed to impossible rooms, closed
  doors, drawing marks, and one missing door handle. Ladders/keys remain in the
  object puzzle vocabulary but are deferred until this proof works.
- 2026-06-18: Native T0010 mechanics now pass through DevAPI:
  `build/captures/backrooms_t0010_portal_memory_status.json` proves mark
  placement, locked door rejection without the handle, handle pickup, handle
  fitting, exit reveal, and escape. Visual gate remains red in
  `gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md`;
  the impossible geometry reads too flat and needs a stronger non-Euclidean
  render pass before this task can close.
- 2026-06-18: product gate FAIL (desktop); review: gamedesign/projects/backrooms-liminal/reviews/t0010_portal_memory_visual_gate.md; screenshot: build/captures/backrooms_t0010_impossible_geometry.png; next: Replace the flat impossible-cut illusion with a stronger non-Euclidean render pass: nested room planes, convincing wall thickness, shadowed door depth, and authored room landmarks.
