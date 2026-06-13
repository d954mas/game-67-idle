# Design Stewardship

Keeping existing design docs coherent after the first GDD exists: edits,
reviews, reconciliation with implemented gameplay, balance rules, lore,
economy, content plans, and open questions.

## Workflow

1. Read `AGENTS.md` for project identity and design constraints.
2. Find the local design source of truth. In this repo, game-specific design
   source of truth lives in `gamedesign/projects/<game-id>/`. Common files
   inside it include `gdd.md`, `concept.md`, `decisions.md`,
   `open_questions.md`, `references/`, `sources/`, and `data/`.
3. Separate durable decisions from brainstorms.
4. Prefer short implementation-ready specs over long prose.
5. When a design choice affects code, state the expected player-visible
   behavior and validation signal.
6. Update open questions instead of inventing certainty where the project is
   undecided.

## Spec Template

For new or revised design sections, use:

- Goal: why this exists for the player.
- Player behavior: what the player does.
- System behavior: what the game does.
- Feedback: what the player sees, hears, or receives.
- Tuning knobs: numbers likely to change.
- Validation: how to know it works in-game.

## Guardrails

- Do not bury key rules in lore prose.
- Do not put project facts in `gamedesign/knowledge/`; keep that folder for
  reusable cross-project rules only.
- Do not create large new design taxonomies unless the current work needs
  them.
- Keep tone consistent with local project rules.
- If implementation and docs disagree, point out the mismatch explicitly.
