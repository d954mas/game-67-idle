# Little Lives GDD

## One-Line Concept

A tiny **3D** life sim: direct one Sim around a single room to keep their needs
(Energy, Hunger, Fun, Hygiene) met — a small, honest slice of *The Sims*.

## Audience

Casual players. Readable at a glance; simple moment-to-moment play; the "what
should I do next" answer is always visible (the lowest need).

## Core Loop

See `data/core_loop.json` for the structured model. In short:

1. Read the four need bars and the Sim's mood.
2. Pick the lowest need and send the Sim to the furniture that restores it
   (click its button or press 1–4).
3. The Sim walks over and uses it; that need refills while the others keep
   decaying.
4. New lowest need → next decision. Keep everything out of the red.

## First Playable Slice (native 3D)

- **One room, 3D.** Floor + grid, four walls, ceiling open to the camera.
  Orbit camera (drag to rotate, wheel to zoom), 3/4 overhead "dollhouse" view.
- **One Sim** (capsule body + sphere head) that idles, walks to a target, and
  bobs while "using" furniture. Faces its movement direction.
- **Four furniture pieces**, each restoring exactly one need:
  - **Bed** (back-left) → Energy
  - **Fridge** (back-right) → Hunger
  - **Sofa + TV** (front-right) → Fun
  - **Shower** (front-left) → Hygiene
- **Needs** (0–100) decay continuously at different rates; using the matching
  furniture refills the one need quickly.
- **HUD overlay** (2D over the 3D scene): four color-coded need bars (green →
  amber → red), a mood readout, a day clock, a Simoleons counter (flavor), and
  four furniture action buttons. The lowest need / its button is highlighted as
  the suggested next action.
- **Controls:** keys `1`/`2`/`3`/`4` direct the Sim to Bed/Fridge/Sofa/Shower;
  the matching HUD buttons do the same with the mouse; left-drag orbits the
  camera; wheel zooms; `Esc` quits (native).

### Out of scope for this slice

Build/Buy mode, placing furniture, multiple rooms/Sims, relationships, careers,
real economy/shopping, and any away-time/idle progression. Furniture set is
fixed in code.

## Visual Target (fake-shot direction)

Bright, saturated, friendly "dollhouse" diorama: warm wood floor, soft pastel
walls, clearly distinct furniture silhouettes in candy colors, one cheerful Sim
in the middle. Need bars read instantly by color. Think *The Sims* live mode
crossed with a toy-box. No realism, no muddy/low-contrast surfaces.

This first slice uses the engine **shape renderer** (procedural cubes/spheres/
capsules) as acknowledged debug-art debt — same status as the clean seed. The
follow-up task swaps furniture/Sim/room to real textured meshes + UI font text
via the asset pipeline (see `tasks/`).

## Art Direction Stub

Bright, saturated, friendly, readable at a glance. Distinct per-object hues so
each need/furniture is identifiable by color alone. Avoid realistic, muddy, or
low-contrast presentation.
