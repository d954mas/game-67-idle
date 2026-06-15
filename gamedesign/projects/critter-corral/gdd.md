# Critter Corral — GDD

Living source of truth. Concept gate: `concept.md`. Reference digest:
`references/sort_puzzle_deconstruction.md`. Visual gate:
`reviews/first_slice_visual_gate.md`.

## One-line

A loop sort-puzzle reframed as herding: living colored critters wander an open
pasture; you corral each color into its matching pen by INFLUENCE (a lure),
never by placing them. Same-color critters chain in after the first.

## Audience / platform / session

Casual. Native PC first; primitives now, art skinned later (Codex). Snackable
rounds (clear a wave). Calm — no timer in the first slice.

## Pillars (violations)

1. Alive, not inert (critters wander/clump/follow/resist; not dead tiles).
2. Influence, not placement (you shape the field; never grab a critter).
3. Satisfying chain (a color pops into its pen and friends cascade in — juice).

## THE CORE MOMENT (build this first, on primitives)

Lure a mixed clump toward a pen; the matching color *pops* in (squash + burst +
"ding") and same-color critters chain after it; wrong-color critters bounce off
the gate. Nothing else matters until this feels good. This is the slice's
definition of done (felt, judged vs the concept fake-shot DIRECTION).

## First playable slice (one goal, one action)

- Open pasture field; ~8–12 critters of **2 colors**, wandering with light
  flocking (separation + wander; gentle cohesion).
- Two pens (colored rectangles with an open gate) at fixed spots.
- **One action:** the lure follows the cursor (or a placed point); critters
  within a radius steer toward it. Player sweeps clumps to pens.
- Pen accepts only its color (matching pop+chain; wrong bounces).
- **Goal:** corral all critters into their pens (clear the wave). No timer, no
  fail-out in the slice — calm. A simple "wave cleared!" beat ends it.
- Juice budget (cheap, primitive): squash/scale on entry, a small particle
  burst, a pen flash + count tick, a soft sound. Juice IS the product here.

## Roadmap to 10–20 minutes of release-ready gameplay (AFTER the core moment lands)

Layer ONE thing at a time, re-judging feel each step; do not pre-build:
1. Waves + progression: more critters, +1 color per few waves (up to ~5),
   faster/odder wander. A run = a sequence of waves (~10–20 min total).
2. Critter behavior variety (the depth, our twist's payoff): skittish (flee the
   lure briefly), stubborn (slow to steer), follower (clings to other colors) —
   each forces a different herding read.
3. Light meta between waves: a visible upgrade choice (bigger lure radius,
   second lure, calmer critters, wider gate) bought with corralled-count — one
   readable pick, not a shop.
4. Readable FTUE <=3 beats: (1) lure moves critters, (2) match pen = pop+chain,
   (3) clear the wave. Tutorial-by-doing, no walls.
5. Fail/pressure (only if it improves feel): a soft "pasture full" cap or a
   relaxed wave timer on later waves — calm-first identity preserved.
6. Release polish: consistent juice, audio pass, win/lose/restart flow, a title
   screen, perf, and the visual direction reached on primitives (then Codex skins).

## Out of scope (now)

Monetization/meta-economy, multiple tools, content matrices, art assets,
web/mobile port. Add only after the core moment + a few waves prove fun.
