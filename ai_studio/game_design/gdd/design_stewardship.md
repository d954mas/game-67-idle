# Design Stewardship

Load this when updating or reviewing existing design docs after a first GDD
exists.

## Workflow

1. Find the local design source of truth under
   `games/<game-id>/design/`.
2. Separate accepted decisions from brainstorms and stale notes.
3. Prefer short implementation-ready specs over long prose.
4. When a design choice affects code, state expected player-visible behavior and
   validation signal.
5. Update open questions instead of inventing certainty.
6. If implementation and docs disagree, point out the mismatch explicitly.

## Spec Shape

For new or revised design sections, use:

- Goal: why this exists for the player.
- Player behavior: what the player does.
- System behavior: what the game does.
- Feedback: what the player sees, hears, or receives.
- Tuning knobs: numbers likely to change.
- Validation: how to know it works in-game.

## Guardrails

- Do not bury key rules in lore prose.
- Do not put project facts in reusable knowledge files.
- Do not create large new taxonomies unless the current work needs them.
- Keep tone consistent with current project rules.
