# State Management

State Management owns schema-first game state workflow and the generator that
emits the C `GameState` API.

It belongs to Game Project because state schemas are game-owned. AI Studio keeps
the reusable workflow and generator here; current game data belongs in
`state/`, while the reusable template state lives in `template/state/`.

## Use

Use this group when work touches:

- `state/*.schema.json` or `template/state/*.schema.json`;
- generated `game_state.h`, `game_state.c`, `game_state_devapi.c`, or
  `game_state_schema.gen.h`;
- JSON save/load envelopes;
- versioned migrations and fixtures;
- DevAPI `game.state.*` commands;
- inventory/equipment references;
- bot/test setup that reads or writes progression data.

## Files

- `generate_state.py`: schema-to-C generator.
- `game_state.c.in`: source template for generated `game_state.c`.
- `generate_state_test.py`: generator regression tests.
- `contract.md`: state documents, save envelope, storage keys, codegen, dirty
  state, migrations, and domain actions.
- `workflow.md`: schema-first workflow and runtime access rules.
- `review.md`: review checklist and failure signals.

## Commands

```powershell
python ai_studio/game_project/state_management/generate_state.py
python ai_studio/game_project/state_management/generate_state.py --schema state/game_state.schema.json --out-dir src/generated
python -m unittest ai_studio.game_project.state_management.generate_state_test
```

Default behavior:

- inside a created game with `state/game_state.schema.json`, generate to
  `src/generated/`;
- inside this AI Studio root, use `template/state/game_state.schema.json` and
  generate to `template/src/generated/`.

Use `nt-game-state-management` as the skill surface for this workflow.
