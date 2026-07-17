# State Workflow

Load this when adding or changing state fields, generated APIs, migrations,
runtime actions, DevAPI commands, fixtures, or save behavior.

## Workflow

1. Read the version-2 fragment schema and installed save/storage headers.
2. Edit path-keyed `fields`, types, hooks, enums, or `reserved` tombstones in
   the schema; never introduce numeric field ids.
3. Regenerate and inspect all five `<id>_state*` outputs. Do not hand-edit them.
4. Keep generated storage/serialization separate from hand-written domain
   actions and the hand-written registry DevAPI dispatch.
5. Register every fragment before `game_save_init()` in deterministic order.
   The template order is `settings`, `items`, `progression`, then `game` last.
6. Add deterministic migrations and fixtures for renamed, deleted, moved, or
   compensated data.
7. Run generator tests, native save/storage tests, and relevant DevAPI/runtime
   scenarios.

For a nested aggregate, keep `order_by` explicit at both levels. Treat each
`max_count` as the fixed pool budget (the nested budget is global, not
per-parent), and keep cross-object uniqueness, ownership, and gameplay policy
in domain actions.

Runtime access should flow as:

```text
schema -> generated fragment storage -> domain actions -> gameplay/UI/semantic DevAPI
```

Gameplay and UI must not mutate generated state fields directly.

## Migrations

Per-fragment migration order is:

```text
copy saved fragment -> migrate vN to current -> validate -> parse live state
```

Keep migrations deterministic and isolated from domain actions, network,
wall-clock time, and mutable runtime balance. A failure resets only the affected
fragment.

When shipped data moves between fragments, increment `GAME_SAVE_DOC_VERSION`
and add a document-level migration over the raw envelope `features` object
before fragment dispatch. Use a clean break only before shipped saves, and
record the accepted data loss.

## DevAPI

The hand-written registry dispatch exposes exactly:

```text
game.state.schema
game.state.get
game.state.set
game.state.patch
game.state.save
game.state.load
game.state.reset
```

Paths start with the fragment id. Empty-path `get` aggregates fragments in
registration order plus optional orphans. A cross-fragment patch is atomic per
fragment group, not across the whole save. Save/load use the fixed autosave slot
and have no `key` or `doc` parameters.

Use raw state writes for debug/editor overrides, fixtures, and scenario setup.
Use semantic actions for gameplay tests.

## Rules

- Runtime state is always enabled; only DevAPI is gated by
  `GAME_DEVAPI_ENABLED`.
- Preserve unknown feature blobs as round-tripped orphans.
- Treat ABSENT differently from an existing save that cannot be read.
- Do not force state with different ownership into one fragment.
- Make failed mutations transactional: validate a copy, then replace live
  fragment state.
- Web saves require explicit browser persistence and lifecycle flushing.
