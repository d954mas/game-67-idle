# Items typed catalog contract

How an authored Lua catalog becomes the typed C API a game compiles against.

## Boundary

Four layers, each owning one thing:

1. `design/items/*.lua` is the only authoring source.
2. `items_lua_sandbox.py` executes it in isolation and emits
   `items.lua.evaluation.v1`.
3. `items_snapshot.py` normalizes that evaluation into `items.snapshot.v1` — the
   rich model tools and the web studio read.
4. `items_c_catalog.py` projects the Snapshot onto the facts the typed C API
   returns and atomically exports write-if-different public
   `items_catalog.gen.h`, build-local `items_catalog.internal.gen.h`,
   data/capability `items_catalog.gen.c`, and the tooling stub
   `items_catalog.luau`.

The generator never executes Lua, never infers schema from populated values, and
never re-validates what the Snapshot already sealed. It fails on what only the
projection can see: identity collisions, currency policy, cost references,
capability member coverage, and generation budgets.

The fixtures under `tests/fixtures/items_api_*_proof.{lua,lua.json,json}` are
reproducible: `items_c_catalog_test.py` re-evaluates the Lua through the real
sandbox and requires the committed Snapshot to match byte for byte.

## Schema ownership

`items.*` is sealed core. A game cannot redefine a core field. Game extensions
use stable namespaced identities:

```lua
local weapon = items.kind({ id = "weapon", label_key = "kind.weapon" })

attack = field.i64({
  id = "game.weapon.level.attack",
  required_for = { weapon },
  min = 0,
  max = 1000000,
  unit = "damage",
  rounding = "exact",
  label_key = "item.attack",
  ui = { format = "integer", description_key = "item.attack.description" },
  evolution = { since = 1, deprecated = false },
})
```

The field identity is `game.weapon.level.attack`; `attack` is only its generated
C member. Each field owns its capability, numeric range, unit, rounding,
localization key, formatting hints, evolution metadata, and source span. Fields
in this catalog are `i64` and `exact`; there is no floating-point level value.

A kind is declared once, by the space that owns it, and reaches a field only as
the handle `items.kind`/`tracks.kind` returned -- there is no name to resolve and
no name to misspell. On the wire the evaluator keeps the spaces apart:
`required_for_items` names item kinds, `required_for_tracks` names track kinds,
and a field carrying only the latter is a progression column this catalog never
sees. One id declared in both spaces is two kinds and binds nothing across them.

A core-only catalog has no game fields, so its generated header has no `weapon`
or `attack` symbol. That is the proof an optional game schema cannot leak into
Items core.

## Normalized level and cost semantics

Every definition declares orthogonal `tags` and one authored mode. Definitions
without levels use `none`; levelled definitions use exactly one of:

- `levels.single` produces one literal row;
- `levels.table` produces literal rows;
- `levels.generate` records generated provenance;
- `levels.columns` records column provenance per value, and an override replaces
  only the selected normalized value while preserving override provenance.

Row N describes target level N, so the transition 1 -> 2 is stored on row 2.
Level 1 cannot carry `cost_to_reach`; every later row must state one. Every
level row of a capability item must carry every field that capability declares —
a missing member fails generation rather than emitting a zero.

Acquisition and level transitions collapse to three generated kinds:

- absence means `ITEM_TRANSITION_UNAVAILABLE`;
- `items.free()` means `ITEM_TRANSITION_FREE`;
- `items.cost` / `items.costs` means `ITEM_TRANSITION_COST` plus a cost span.

Acquisition is authored as `acquire = { cost = ... }`. Cost entries refer only
to stackable resources, resource IDs are unique within a cost, and counts are
positive i64 integers.

Cost span zero is the reserved null span, so an unavailable or free transition
never indexes cost data.

## Generated identity and C surface

`item_id_t` is a strong typedef over `uint64_t`. The ID is exact XXH64 of the
validated UTF-8 item id with seed 0 and no normalization. Regression anchors:

```text
game.gold       -> E662E696028B01C4
game.iron_sword -> B36736FA950BF10D
```

Generation rejects both a 64-bit hash collision and a collision after C-name
sanitization. Item ids longer than 32 bytes are ordinary; generated constants
and exact-string lookup have no hidden short-ID limit.

Generated index order is sorted item id, so `item_def_ref_t` indices stay stable
while the id set does.

The stable base API lives in `features/items/items.h`; consumers include only
that header, and it pulls in exactly one `items_catalog.gen.h` selected by the
build include directory. Generated item constants use names such as
`ITEM_GAME_GOLD`. `item_core_t` carries only the item ID and stack scalar; it
cannot expose catalog strings or pointers. Required lookup and invalid ranges
assert; `exists`/`try_get` cover expected absence. String lookup hashes first and
still compares the original id.

The generated header owns only game-specific capabilities. Capability structs,
members, field-ID constants, row tables, and accessors derive from the field
schema, not from hardcoded weapon properties. A declared capability with no
current items still emits one portable zero sentinel row; its per-item spans and
public counts stay zero, so the sentinel is never observable.

`items_api.c` implements the base API unchanged for every catalog. Generated C
owns only immutable data, the build-local internal seam, and capability
accessors. Definition strings are reachable only for exact hash-plus-string
lookup and debug-label registration.

`ITEMS_CATALOG_SCHEMA_ABI` covers the accessor surface, the field schema, and the
item id set. `ITEMS_CATALOG_CONTENT_FINGERPRINT` additionally covers the Snapshot
digest, so any authored metadata edit moves it while the ABI holds still.

All four outputs use temporary-file replacement only when bytes change,
preventing partial output and needless recompilation.

`items_catalog.luau` is a real LuaLS annotation module, not a metadata table. It
starts with `---@meta` and generates typed capability level-row classes with
`---@field` entries carrying field identity and unit, giving developers and
agents completion without becoming another authoring source.

## Commands

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_c_catalog_test.py
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_c_catalog.py `
  --snapshot <snapshot.json> --out-dir <build-local-output-directory>
```

Generated output stays build-local; it is never committed into a template or a
game.
