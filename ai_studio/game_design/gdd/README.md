# Primary GDD Workflow

Primary GDD turns a loose game idea into a scoped, implementation-facing design
package for one game.

It owns workflow, not the game content itself. Current-game design files live in
`games/<game-id>/design/`. Reusable design knowledge and source notes are
routed through `ai_studio/game_design/knowledge/`.

## Use

Use this group when the task is about:

- a new or revised game concept;
- a first GDD or visual GDD;
- gameplay loop, player verbs, economy, progression, challenge, or UI flow;
- fake gameplay screenshots or visual proof for a design;
- implementation handoff from design to prototype work.

Do not use it as a general quality system, asset-storage system, or task store.

## Flow

1. Locate or create `games/<game-id>/design/`.
2. Write a short Definition of Done: required artifacts, out of scope, accepted
   proof.
3. Lock one gate at a time: concept, references, visual proof, first slice,
   handoff.
4. Load only the file needed for the current gate.
5. Keep evidence and decisions durable; keep scratch, rejected images, and raw
   generation in `tmp/`.
6. Before implementation, make the first playable slice concrete enough that the
   next agent does not need chat history.

## Files

- `core_workflow.md`: gates, artifact set, report shape, and stop rules.
- `creative_intake.md`: short intake when taste or acceptance criteria are
  unclear.
- `design_stewardship.md`: updating existing design docs without drift.
- `gameplay_systems.md`: player verbs, loop, economy, UI flow, and challenge
  contracts.
- `visual_proof.md`: fake shots, visual tiers, and game-use visual proof.
- `implementation_handoff.md`: source order, first playable packet, and
  acceptance gates.
- `web_gdd_site.md`: visual GDD website requirements.
- `review.md`: GDD-specific review stance and routing to quality rules.
- `templates.md`: small repeated templates.

Use `nt-primary-gdd` as the skill surface for this workflow.
