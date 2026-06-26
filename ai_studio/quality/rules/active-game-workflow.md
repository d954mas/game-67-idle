# Active Game Workflow Rule

Apply when work expands or changes an active game, prototype, runtime slice, or
game-specific content.

## Check

Run the active game workflow guard when the work touches active game direction,
runtime growth, or feature/content expansion:

```powershell
node tools/game_context/workflow_guard.mjs
```

The guard is dormant for a clean seed or no active game.

## Blocks

Do not expand feature/content work when:

- lead rejection is unresolved;
- references are explicitly not ready;
- the runtime is becoming monolithic without an architecture task;
- the user said the prototype/game is stopped, done, or only a test.

Record any override as explicit lead acceptance, not as an agent decision.
