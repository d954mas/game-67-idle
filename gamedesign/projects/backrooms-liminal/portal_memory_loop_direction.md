---
title: Portal Memory Loop Direction
project: backrooms-liminal
status: draft
created: 2026-06-18
---

# Portal Memory Loop Direction

## Core Hook

Backrooms Liminal should not be another straight Level 0 maze. The unique hook:

> The building is bigger inside than outside, learns the player's route, and
> forges familiar rooms. The player survives by marking space, moving found
> objects, and proving which exits are real.

## Player Fantasy

You are not fighting a monster. You are doing spatial forensics inside a place
that lies.

The player should feel:

- "I have been here, but something changed."
- "My mark proves this wall moved."
- "This object belongs somewhere else."
- "That doorway is impossible, but it might be the only real exit."

## Pillars

1. Impossible rooms
   - Some rooms are larger inside than their doorway footprint suggests.
   - Doors can lead back into the same room from a different side.
   - A hallway can loop while still visually looking straight.
   - The outside shape of a room and the inside route disagree.

2. Player-authored evidence
   - The player can draw arrows, numbers, symbols, or short marks on walls and
     floors.
   - Marks are gameplay evidence, not decoration.
   - Some marks stay reliable. Some are copied incorrectly by the place.
   - The player can use mismatch between a real mark and a forged mark to
     identify a false loop.

3. Object placement logic
   - The player finds mundane objects: key, chair, fuse box cover, badge,
     breaker handle, torn carpet strip, wet floor sign, room plate.
   - Objects must be placed in matching rooms or on matching landmarks.
   - Correct placement stabilizes a route, unlocks a door, or reveals a portal
     seam.
   - Wrong placement can create a false exit or trigger a room rewrite.

4. No ordinary map
   - The journal records tasks and observations, but not a clean minimap.
   - The player's own wall/floor marks become the map.
   - The world can create copied marks, so the player must test, not blindly
     trust.

## First Playable Loop

1. Enter a yellow office corridor.
2. Draw a mark on a wall or floor.
3. Find a mundane object in a side room.
4. Return through an impossible loop and notice the mark is copied or missing.
5. Place the object on the correct landmark.
6. A real exit seam appears.
7. Enter the seam before the room copies the solution.

## First Native Spike

Minimum proof for the next implementation slice:

- Impossible geometry is visible without reading as a sci-fi portal: a narrow
  architectural cut/door seam shows a room volume that is wider/deeper than the
  outside corridor can contain.
- A closed door blocks progress until the player finds and fits a missing
  handle.
- The player can place at least one mark on a wall/floor surface.
- The player can pick up one object and place it at one valid target.
- DevAPI report proves mark placement, object pickup, object placement, and
  exit reveal.
- Screenshot proof shows the impossible-room composition, not only state fields.

## Design Risk

- Portal rendering can become a technical rabbit hole. The first spike should
  fake one impossible architectural cut convincingly before attempting a
  general recursive/non-Euclidean renderer.
- Drawing must be simple: one or two mark types first, not a full paint system.
- Object puzzles must be readable and physical. Avoid abstract keycards unless
  the room itself explains why the object belongs there.
- The place copying marks is powerful but should not feel random. The player
  needs rules they can learn.

## Visual Direction

The visual bar is high: this feature only works if the rooms look physical and
lit, not like debug shader boxes. The portal-room screenshot should emphasize:

- believable fluorescent panels;
- contact shadows at floor/wall seams;
- damp carpet and stained wallpaper;
- one impossible doorway that is clear in composition;
- minimal diary UI, no meters;
- a player mark visible on an actual surface.
