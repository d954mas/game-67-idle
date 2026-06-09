---
name: game-design-steward
description: Use when creating, editing, reviewing, or reconciling game design documents, lore, economy, balance rules, progression, feature specs, content plans, open questions, or design decisions. Triggers include GDD work, mechanics design, narrative tone, player fantasy, monetization/economy, balance tables, milestone scope, and keeping design docs aligned with implemented gameplay.
---

# Game Design Steward

Use this skill to keep design work coherent, concise, and useful for implementation.

## Workflow

1. Read `AGENTS.md` for project identity and design constraints.
2. Find the local design source of truth: common names include `gamedesign/`, `gamedesing/`, `docs/design/`, `GDD.md`, `concept.md`, `lore.md`, and `open_questions.md`.
3. Separate durable decisions from brainstorms.
4. Prefer short implementation-ready specs over long prose.
5. When a design choice affects code, state the expected player-visible behavior and validation signal.
6. Update open questions instead of inventing certainty where the project is undecided.

## Output Style

For new or revised design sections, use:

- Goal: why this exists for the player.
- Player behavior: what the player does.
- System behavior: what the game does.
- Feedback: what the player sees, hears, or receives.
- Tuning knobs: numbers likely to change.
- Validation: how to know it works in-game.

## Guardrails

- Do not bury key rules in lore prose.
- Do not create large new design taxonomies unless the current work needs them.
- Keep tone consistent with local project rules.
- If implementation and docs disagree, point out the mismatch explicitly.

