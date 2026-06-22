# Cozy Automation GDD

## One-Line Concept

A small cozy automation game: place simple producers on a small grid; they generate and auto-route a resource you spend to unlock the next producer/step.

## Audience

Casual players. Progression should be clear; controls and moment-to-moment play should stay simple.

## Core Loop

1. Read the immediate situation and choose one clear action.
2. Execute the action with responsive feedback.
3. See a meaningful state change, consequence, risk, or reward.
4. Face a new short-term goal or decision that changes the next repetition.

## First Playable Slice

- One native PC scene.
- One clear player action.
- One feedback moment that proves the action changed the game state.
- One visual proof screenshot for product-read review.
- One filled `reviews/first_slice_visual_gate.md` before broad runtime work.
- One filled `data/core_loop.json` with player verbs, rules, feedback, risk,
  goals, replay reason, and reference grounding. Do not assume hands-off
  progression, away-time rewards, or reset-meta loops unless the lead
  explicitly chooses that direction.
- One project-specific `visual/live_state_acceptance_matrix.json` that names
  required UI/player-read states before broad visual acceptance.
- One visual-first session contract: goal, non-goal, proof, stop condition,
  likely files.
- One screenshot-vs-target mismatch list before runtime visual code and after
  meaningful render changes.
- If the slice depends on beauty, casual readability, generated UI, or a fake
  shot match, one strict visual product gate using `--visual-strict`.
- Optional critic packet from `tools/product_gate/visual_critique_packet.mjs`
  before the strict gate verdict.

## Art Direction Stub

Bright, saturated, friendly, readable at a glance. Avoid realistic, muddy, or low-contrast presentation.
