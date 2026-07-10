# Items Lua schema/API proof contract

Status: bounded T0364 implementation proof. This contract refines the accepted
single-source concept; it is not the production Lua evaluator or runtime blob.

## Boundary

The proof has three intentionally separate layers:

1. `items_api_*_proof.lua` demonstrates the proposed authoring vocabulary.
2. `items_api_*_proof.json` is a materialized, normalized Snapshot fixture.
3. `generate_items_api_proof.py` validates that Snapshot and atomically exports
   write-if-different public `items_game.gen.h`, build-local
   `items_game.internal.gen.h`, data/capability `items_game.gen.c`, and the
   tooling schema stub `items_game.luau`.

The exporter never executes Lua, infers schema from populated values, parses a
runtime config, or defines the production binary package. The future evaluator
owns Lua execution, reference resolution, formula evaluation, and provenance
capture. The future runtime exporter owns the blob wire format.

## Schema ownership

`items.*` is sealed core. A game cannot redefine a core field. Game extensions
use stable namespaced identities, for example:

```lua
attack = field.i64({
  id = "game.weapon.level.attack",
  required_for = { "weapon" },
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
C member. Each field owns its type, capability, numeric range, unit, rounding,
localization key, formatting hints, evolution metadata, and source span. Removed
identities remain in root `reserved_field_ids` and cannot be reused. The proof
rejects duplicate identities and members, sealed-field redefinitions, unknown
keys, invalid identities, unsupported types, malformed ranges, and incomplete
metadata. Capability IDs are C-safe lowercase identifiers; invalid IDs and
post-generation name collisions fail before rendering.

Views reference schema fields only by `field_id`. They own layout, order, and
chart composition, but cannot redefine type, unit, labels, rounding, or other
field meaning. Unknown view references fail during Snapshot validation.

A core-only Snapshot has no game fields. Consequently its generated header has
no `weapon` or `attack` symbol. This is the proof that an optional game schema
does not leak into Items core.

## Normalized level and cost semantics

Every definition declares orthogonal `tags` and one authored mode. Tags are
unique lowercase identifiers and cannot repeat the definition's `kind`.
Definitions without levels use `none`; levelled definitions use exactly one of:

- `levels.single` produces one literal row;
- `levels.table` produces literal rows;
- `levels.generate` records generated provenance;
- `levels.columns` records column provenance per value, and an override replaces only the
  selected normalized value while preserving override provenance.

The normalized Snapshot records `authoring_mode` plus complete, bounded
per-value provenance (`single`, `table`, `generate`, `columns`, or `override`).
Every present game field and transition has exactly one matching provenance
entry; missing, extra, arbitrary, or mode-inconsistent entries are rejected.
The weapon fixture has separate
single, table, generate, and columns-plus-override definitions. Every weapon
row requires `game.weapon.level.attack`; non-weapons cannot carry it. Level 1
cannot have `cost_to_reach`. Every later row must have an explicit transition.
Row N describes target level N, so the transition 1 -> 2 is stored on row 2.

Acquisition and level transitions use one normalized shape:

```json
{ "kind": "free" }
{ "kind": "cost", "cost": [{ "item_ref": "game.gold", "count": 100 }] }
```

Absence means unavailable; there is no empty-list sentinel or parallel boolean.
Cost entries refer only to stackable resources (`stack != 1`), resource IDs are
unique within a cost, and counts are positive i64 integers. The fixture proves:

- `items.ref("game.gold")` and `items.ref("game.metal")` resolution;
- a one-resource acquisition cost;
- a composite gold-plus-metal target-level cost;
- explicit free as a typed transition.

Every reference must resolve to a registered definition. Duplicate resources,
non-stackable item references, empty `cost` transitions, and inconsistent
transition keys fail deterministically.

## Generated identity and C surface

`item_id_t` is a strong typedef over `uint64_t`. The ID is exact XXH64 of the
validated UTF-8 `def_id` bytes with seed 0 and no normalization. Regression
anchors are:

```text
game.gold       -> E662E696028B01C4
game.iron_sword -> B36736FA950BF10D
```

Generation rejects both a 64-bit hash collision and a collision after C-name
sanitization. The hasher is injectable in tests so collision handling is proven
without relying on a real XXH64 collision. Both proof variants also contain a
valid `def_id` longer than 32 bytes; generated constants and exact-string lookup
have no hidden short-ID limit.

The stable base API lives in `features/items/items.h`. During this proof its
entire surface is gated by `ITEMS_GAME_API_ENABLED`, so existing targets retain
the legacy API until T0386. It owns strong struct types `item_id_t` and
`item_def_ref_t`, opaque cost handles, transition kinds, the small copy-out
`item_core_t`, lookup, cost, and debug-label registration. `item_core_t`
contains only the item ID and stack/storage scalar; it cannot expose catalog
strings or pointers.
Required lookup and invalid ranges assert; `exists`/`try_get` cover expected
absence. String lookup hashes first and still compares the original `def_id`.

With `ITEMS_GAME_API_ENABLED`, that stable header includes exactly one fixed
`items_game.gen.h` selected by the build include directory. Consumers continue
to include only `features/items/items.h`. Generated item constants use names
such as `ITEM_GAME_GOLD`.

The generated game header owns only game-specific capabilities. Capability
structs, members, field-ID constants, row tables, and accessors are derived from
the field schema rather than from hardcoded weapon properties. The current
weapon fixture therefore exposes `ITEMS_GAME_HAS_WEAPON`,
`item_weapon_level_t`, and typed weapon/level accessors while an added `double`
field automatically becomes another typed member and initializer. Its level
accessor copies a row and asserts on invalid item/level. The core-only header
and source contain no weapon or attack symbol. No public generic property bag
or raw offset API exists.

Float and double values reject NaN and infinities before generation. Canonical
C literals retain a decimal point and the required float suffix (`1.0`, `1.0f`,
`0.25f`). A declared capability with no current items still emits one portable
zero sentinel row; its per-item spans and all public counts remain zero, so the
sentinel is never observable through the API.

`features/items-core/src/items_api.c` implements the same base API unchanged for
both proof catalogs. Generated C owns only immutable catalog data, build-local
internal access functions, and game-specific capability accessors. Internal
definition strings are reachable only for exact hash-plus-string lookup and
debug-label registration. `UNAVAILABLE`, `FREE`, and `COST` remain distinct
transitions; a cost is read only through count and copy-out entry accessors.
All four outputs use temporary-file replacement only when bytes change,
preventing partial output and needless recompilation.

The build-local `items_game.luau` is a real LuaLS annotation module, not a
metadata table. It starts with `---@meta`, generates typed capability level-row
classes and schema-member `---@field` entries carrying `field_id`, unit, and
label metadata, and declares typed `items.define`, `items.extend_schema`, and
`items.view` signatures. This gives both developers and agents completion and
type navigation without making the stub another authoring source.

## Diagnostics and fail-early behavior

Every semantic failure has exactly this stable machine-readable location
shape:

```json
{
  "code": "required-field-missing",
  "file": "items_api_weapon_proof.lua",
  "line": 18,
  "column": 1,
  "path": "$.items[2].levels[0].attack"
}
```

Fixture field, view, item, and normalized-row metadata supplies exact Lua spans
in this slice. Malformed spans are themselves typed diagnostics rather than
escaping as Python conversion errors. The real evaluator must capture those
spans. The proof covers invalid/duplicate identities,
sealed fields, unknown keys, unsupported types, capability/cardinality errors,
wrong-kind fields, discontinuous levels, illegal level-1 transition costs,
missing later-level transitions, malformed tags/modes/provenance, invalid cost
resources, fractional values for integer fields, declared-range violations,
integer overflow, hash collision, and generated-name collision.

## Proof commands

```powershell
py -3.12 features/items-core/scripts/generate_items_api_proof_test.py
py -3.12 features/items-core/scripts/generate_items_api_proof.py `
  --snapshot features/items-core/tests/fixtures/items_api_weapon_proof.json `
  --out-dir <temporary-output-directory>
```

Do not commit the proof output into templates. Production codegen placement,
runtime ABI, blob binding, and Lua sandboxing remain later T0364/T0365 slices.
