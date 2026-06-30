# State Workflow

Load this when adding or changing state fields, generated APIs, migrations,
runtime actions, DevAPI state commands, fixtures, or save behavior.

## Workflow

1. Read the schema first.
2. Inspect the generator and selected generated output before editing call
   sites.
3. Add scalar fields through schema edits plus regeneration when possible.
4. Edit `game_state.c.in` only for structural patterns such as owned-object
   maps, id lists, and ref-checked optional strings.
5. Put gameplay operations in a domain layer such as `game_state_actions.h/.c`.
6. Add deterministic migrations and fixtures for moved, renamed, deleted, or
   compensated data.
7. Validate generator tests, then native runtime and relevant DevAPI scenarios.

## Runtime Access

Use this order:

```text
schema -> generated GameState storage -> domain actions -> gameplay/UI/semantic DevAPI
```

Generated state stores data. Domain actions enforce rules and invariants.
Gameplay/UI should not mutate raw `GameState` fields directly.

## Migrations

Migration order:

```text
read save JSON -> migrate vN to current -> validate current schema -> parse GameState
```

Add migrations as `state/migrations/vN_to_vN_plus_1.c` or the local equivalent.
Keep versioned constants near the migration when compensation logic is needed.

## DevAPI

Generated state supports:

```text
game.state.schema
game.state.get
game.state.set
game.state.patch
game.state.save
game.state.load
game.state.reset
```

Use raw state writes for debug, editor overrides, fixtures, and scenario setup.
Use semantic actions for gameplay tests.

## Rules

- Do not hand-edit generated `game_state.*`.
- Do not store game-specific state rules in reusable AI Studio docs.
- Do not force all state into one global document.
- Do not use domain actions in migrations.
- Do not mutate raw `GameState` from gameplay/UI.
- Prefer maps for owned entities and lists of ids for order.
- Make failed writes transactional: validate a copy, then replace runtime state.
- Web saves need explicit browser persistence, not only `fopen`.
