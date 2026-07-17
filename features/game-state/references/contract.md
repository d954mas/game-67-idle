# State Contract

Load this when deciding fragment schemas, save/load behavior, DevAPI state
shape, generated state boundaries, dirty/autosave behavior, or migrations.

## Fragment Schema And Codegen

Each schema is version 2 and owns one fragment:

```json
{
  "schema_version": 2,
  "fragment": "game",
  "version": 1,
  "fields": {},
  "reserved": []
}
```

Fields are keyed by stable paths. There are no numeric field ids. Keep removed
paths in `reserved`; strings require `max_length`, and lists/maps require
`max_count`.

`u32` fields declare integer `default`, `min`, and `max` within
`0..4294967295`. They use `uint32_t` in C and an exact JSON number on the wire;
strings, fractions, negative values, and overflow are rejected.

Schema v2 has one bounded aggregate extension: a fragment may declare one
top-level `list<Object>` whose object type declares exactly one nested
`list<Object>`. Both lists require `max_count` and a non-empty `order_by` over
canonical primitive fields. The generated state uses separate fixed pools;
the nested pool stores only an internal parent index, while JSON/DevAPI nests
children under their parent. Every aggregate object field is required during
parse. Deeper recursion, heap-backed state, type reuse as a map, and multiple
aggregate roots are rejected. Removed fields remain in the owning fragment or
type `reserved` list.

The generator requires `--schema` and accepts `--fragment` as an expected-id
check. Per fragment it emits `<id>_state.h`, `<id>_state.c`,
`<id>_state_schema.gen.h`, and `<id>_state_events.gen.{h,c}`. The generated
source owns `<id>_state` and `<id>_state_fragment`. Do not hand-edit generated
files. DevAPI dispatch is the hand-written, registry-based
`src/game_save_devapi.c`, not generated output.

## Save Envelope And Registry

One save contains every registered fragment:

```json
{
  "format": 1,
  "save_version": 1,
  "saved_at": 1720080000000,
  "save_seq": 42,
  "app": "template",
  "build": "0",
  "features": {
    "settings": { "v": 1 },
    "items": { "v": 1 },
    "progression": { "v": 1 },
    "game": { "v": 1 }
  }
}
```

Register all fragments before `game_save_init()`. Serialization and aggregate
DevAPI output follow registration order; the template registers `settings`,
`items`, `progression`, then `game` last. The shell injects each fragment's
`v`; fragment
`to_json()` output omits it. Unknown feature keys are retained as orphans and
round-tripped after registered fragments.

## Storage And Load Lifecycle

`game_storage` uses logical slot names internally. Native saves use a temporary
file followed by replacement, retain a last-known-good backup, and quarantine
unreadable/corrupt saves. Web saves use `GAME_STORAGE_APP_ID`-scoped
`localStorage`, report persistence availability, and do not have a native
backup file.

Load distinguishes absence from failure:

- absent save: reset all fragments, run new-game hooks, save, and report
  `FRESH`;
- valid save: load fragments independently and report `LOADED`;
- invalid primary with valid backup: recover and report `RECOVERED_BAK`;
- read/parse failure: quarantine, reset, and report `CORRUPT_RESET` without
  silently treating the save as absent;
- newer envelope or known-fragment version: report `NEWER`, perform no writes,
  and pause autosave.

An absent fragment resets independently. A fragment parse or migration failure
resets only that fragment and is recorded in the load result. Orphans survive
load/save. Public transform/export/import behavior is defined by the installed
`game_save.h` and `game_storage.h`.

## DevAPI

Runtime state is always compiled. The hand-written dispatch is compiled and
registered only with `GAME_DEVAPI_ENABLED` and exposes exactly:

```text
game.state.schema
game.state.get
game.state.set
game.state.patch
game.state.save
game.state.load
game.state.reset
```

State paths start with a fragment id. `get` with an empty path aggregates
registered fragments in order and includes `orphans` only when present.
`patch` is atomic within each fragment group; failure in one fragment does not
roll back or modify another. `save` and `load` use the fixed autosave slot and
accept no `key` or `doc` parameter. `reset` applies new-game semantics.
`game.state.schema` returns each normalized schema with ordered `fields` arrays;
the compatibility alias `document` equals `fragment`. Handler error codes are
limited to `bad_params` and `internal`; detail belongs in the error message.

## Dirty State, Migrations, And Actions

Mark dirty only after a validated mutation reaches live state. Autosave runs at
a frame boundary with debounce and maximum-interval limits. Clear dirty only
after durable save; retain it after failure. Web builds synchronously flush on
visibility/pagehide events.

Per-fragment migrations transform a copy from vN to vN+1 before current-schema
parsing. They must not call domain actions or depend on network, wall-clock
time, live balance, or mutable external data. Moving shipped data between
fragments requires incrementing `GAME_SAVE_DOC_VERSION` and adding a
document-level migration over raw `features`. A clean break is valid only
before shipped saves and must explicitly accept data loss.

Generated state stores and serializes data. Gameplay rules belong in
hand-written domain actions. Raw `game.state.*` mutations are for debug/editor
overrides, fixtures, and targeted tests; gameplay and bots should prefer
semantic actions.
