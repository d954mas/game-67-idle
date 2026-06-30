# Feature Iteration Workflow

Feature Iteration is the Game Project workflow for changing a playable game in
small, verified increments.

It owns the work rhythm for playable slices. It does not own GDD content,
quality rules, runtime automation, asset storage, or task state.

## Use

Use this group when the task changes or validates:

- gameplay mechanics;
- controls, camera, UI flow, or feedback;
- game state, save/load, progression, balance, or unlocks;
- engine integration for a current game;
- prototype or vertical-slice behavior;
- build, launch, release, packaging, or CI for the current game.

## Flow

1. Orient to the active game and current task.
2. Choose one player-visible goal.
3. Define scope, out of scope, proof, and primary runtime.
4. Implement the smallest coherent playable slice.
5. Validate with the fastest reliable proof that matches the claim.
6. Capture evidence and review product quality.
7. Update durable state only when useful.
8. Commit intentionally.

## Files

- `iteration_cycle.md`: task packet, implementation cycle, validation, evidence,
  review, durable state, and commit boundaries.
- `playable_gates.md`: routing for reference-driven work, quality checks,
  runtime proof, build/release work, and handoff.

Use `nt-game-feature-iteration` as the skill surface for this workflow.
