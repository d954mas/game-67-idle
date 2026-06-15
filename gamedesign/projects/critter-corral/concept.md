---
type: Concept
title: Critter Corral — Concept
description: Concept gate for a herd-don't-place loop sort-puzzle. Stage Gate 1.
tags: [concept, critter-corral, sort-puzzle, herding]
timestamp: 2026-06-15
status: concept-gate (awaiting lead review before GDD expansion)
---

# Critter Corral — Concept

> Stage Gate 1 (Concept). Items marked `[A]` are working assumptions to confirm.
> Nothing is coded until the References gate (gameplay deconstruction) is ready.

## Hook / fantasy

You are not a sorter — you are a **herder**. Living, colored critters spill onto
the field and wander, clump, and follow each other. You corral each color into
its matching pen by **influence** (a lure / scare / gust), never by grabbing
them. The signature moment: a stubborn critter resists, then *pops* into its pen
and its same-color friends chain in after it.

This is the genre's proven core (loop sort-puzzle: Marble Sort!, Sand Loop,
Pixel Flow!) with one strong, non-generic twist: **the things you sort are alive
and you only influence them, never place them.** (See
`gamedesign/sources/voodoo_new_big_three_hybrid_casual_2026-06-15.md` and
`gamedesign/knowledge/hybrid_casual_patterns.md`.)

## Genre / platform / session

- Genre: casual loop sort-puzzle, herding twist.
- Platform: **native PC first** (dev harness). Eventual target TBD `[A]`
  (web/mobile casual is the natural home for the genre) — does not change the
  first slice.
- Session: short snackable rounds — clear a "wave" of critters.

## Core verbs

- **Influence** the field: lure (draw critters in), scare (push them away),
  gust (sweep a direction). `[A]` First slice ships **one** tool only (a lure)
  to keep one-goal-one-action; the others are later depth.
- Critters self-move (wander + light flocking); the player shapes the field,
  not the critter.

## Pillars (and their violations)

1. **Alive, not inert.** Critters have agency — wander, clump, follow, resist.
   *Violation:* objects that just slide/sit like dead tiles.
2. **Influence, not placement.** You never grab or drop a critter into a pen.
   *Violation:* tap-to-place / drag-a-critter-into-bin.
3. **Satisfying chain.** Same-color critters cascade into the pen after the
   first one. *Violation:* silent one-at-a-time placement with no chain/juice.

## Progression metric

Critters corralled / pens cleared per wave. Later waves add colors, faster
spawns, and trickier critter behavior (skittish, stubborn, follower). No content
expansion until the core moment feels good (see first slice).

## No-go list

- No direct drag-and-place of critters (kills the twist).
- No realistic / muddy / low-contrast art; no twitch-precision demands.
- No unfair fail; no heavy reading or tutorial walls.
- No monetization / meta systems in the first slice.

## First playable slice (one goal, one action)

Build the **core moment first, on primitives**, before any wave/score/content:

- One field. ~6–10 critters of **2 colors** wandering with simple flocking. Two
  matching pens with open gates.
- **One action:** place/move a **lure**; nearby critters move toward it; you
  sweep a clump to a pen. Matching critters enter (pop + chain); wrong-color
  ones bounce off the gate.
- **The felt moment to nail:** a resisting critter *pops* into its pen and its
  same-color friends chain in — with squash, a little burst, a pen "ding."
- Goal of the slice: it simply **feels good to herd a color home.** Wave/score/
  timer come only after the moment lands.

Primitives only (colored circles + simple eyes + a lure marker + pen rectangles).
Codex skins the art later; the free-asset/primitive build must already reach the
*direction* of the fake shot (per AGENTS definition of done), not pixel-match it.

## First-screen fake-shot DIRECTION (text; image generated later by Codex)

Bright, high-contrast, friendly, tactile/ASMR. A clear play-field with 2–3
boldly colored rounded critters mid-wander (simple expressive eyes), one labeled
pen per color with an open gate, a glowing lure beacon, and a juicy "pop into
pen" with a small dust burst + a chain of same-color critters streaming in.
Reads in 5 seconds; obviously "a game," not a debug screen. Mood: cheerful,
squishy, satisfying. Theme skin (slimes / chicks / bugs / ore-blobs) is TBD and
applied later by Codex; the mechanic reads the same regardless.

## Open design forks (lead review — Stage Gate 1)

1. **Field shape / identity:** open pasture (free 2D field — maximizes the
   herding/flock feel) **vs** the literal conveyor/loop belt (critters ride a
   belt; you divert them off into pens — keeps the genre's "loop" identity +
   timing pressure). This is the biggest fork; it sets the whole feel. `[A]`
   leaning open pasture for the standout herding identity.
2. **First-slice tool:** start with the single **lure** (above) — agree, or a
   different single primary action (gust/push)?
3. **Slice pressure:** calm clear-the-wave (puzzle) vs gentle overflow/timer
   (arcade). `[A]` calm first, to judge the moment without pressure noise.

## Next gates (not started)

- References gate: a short **gameplay** deconstruction of the loop sort-puzzle
  (observed play, screen grammar, borrow/avoid/copy-risk) in this wiki — code
  stays locked until it is ready (status: not ready for implementation).
- Visual gate: fill `reviews/first_slice_visual_gate.md` (direction + native
  capture plan + mismatch list) before broad runtime work.
- Slice gate: first 30s / loop / UI flow; then `game_implementation_plan.md`.
