# Dragon Grove GDD

## One-Line Concept

Original merge-3 dragon grove puzzle: merge eggs, sprouts, and dragons on a
small Y-up grid to restore a clearing.

## Audience

Casual players. The first screen should be readable in five seconds: where the
player is, what can merge, what changed, and why to continue.

## Core Loop

1. Read a small 5x5 grove grid and the highlighted merge-ready group.
2. Trigger one merge-3 action for eggs or sprouts.
3. See the merged object, restored tile progress, and reward message update.
4. Repeat until the first clearing goal is complete or no merge is available.

## First Playable Slice

- One native PC scene: a compact magical grove board.
- One clear player action: merge the highlighted group of three matching items.
- One feedback moment: a hatchling/bloom appears and one shadow tile restores.
- One visible blocked state when no merge group is available.
- One visual proof screenshot/capture for product-read review.
- One filled `data/core_loop.json` with player verbs, rules, feedback, risk,
  goals, replay reason, and reference grounding.

## Reference Boundary

Reference digest: `reference_digest.md`.

This is an original game. It borrows broad merge-3 + dragon-garden genre
grammar from the named reference, but it must avoid the reference game's brand,
art, map layout, UI copy, economy, and progression pacing.

## Slice Controls

- Primary native proof path: build `game_seed` and capture the first native
  screen.
- Internal layout: Y-up grid coordinates. Platform/input Y-down conversion
  belongs only at the boundary.
- Debug keyboard action is acceptable for the first slice if the HUD makes the
  action clear.

## Art Direction Stub

Bright, saturated, friendly, readable at a glance. Avoid realistic, muddy, or
low-contrast presentation.

## Known Visual Debt

The first native slice may use `nt_shape_renderer` as temporary debug debt only.
Before any product pass, replace procedural/debug shapes and non-text HUD with
generated runtime art plus engine text/font rendering.
